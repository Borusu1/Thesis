#pragma once

#include <array>
#include <cstring>

#include <Adafruit_PN532.h>
#include <Arduino.h>
#include <esp_system.h>

#include "board/Pinout.h"
#include "config/DeviceConfig.h"
#include "core/FixedString.h"
#include "domain/Models.h"
#include "domain/TagCodec.h"

namespace device::drivers {

struct NfcProbeResult {
    bool ok = false;
    FixedString<48> detail;
};

class NfcDriver {
public:
    NfcProbeResult begin() {
        if (initialized_) {
            return cachedProbeResult_;
        }

        pinMode(device::board::PN532_SS, OUTPUT);
        digitalWrite(device::board::PN532_SS, HIGH);
        digitalWrite(device::board::TFT_CS, HIGH);
        delay(20);

        nfc_.begin();
        delay(50);

        const uint32_t versionData = nfc_.getFirmwareVersion();
        if (versionData == 0) {
            cachedProbeResult_.ok = false;
            cachedProbeResult_.detail.assign("PN532 firmware probe failed");
            return cachedProbeResult_;
        }

        nfc_.SAMConfig();
        nfc_.setPassiveActivationRetries(0x01);

        char buffer[48] {};
        const auto written = snprintf(
            buffer,
            sizeof(buffer),
            "PN532 fw %lu.%lu",
            static_cast<unsigned long>((versionData >> 16) & 0xFF),
            static_cast<unsigned long>((versionData >> 8) & 0xFF)
        );

        initialized_ = true;
        cachedProbeResult_.ok = true;
        cachedProbeResult_.detail.assign({buffer, written > 0 ? static_cast<std::size_t>(written) : 0});
        return cachedProbeResult_;
    }

    NfcProbeResult probe() {
        return begin();
    }

    void resetScanState() {
        tagPresentLatch_ = false;
        lastUidLength_ = 0;
        consecutiveMisses_ = 0;
        Serial.printf("[nfc] scan state reset (poll #%lu)\n", static_cast<unsigned long>(pollCount_));
    }

    bool pollTag(uint32_t nowMs, device::domain::NfcTagSnapshot& snapshot) {
        if (!initialized_) {
            return false;
        }

        uint8_t uid[7] {};
        uint8_t uidLength = 0;
        digitalWrite(device::board::TFT_CS, HIGH);

        const uint32_t pollStartMs = millis();
        const bool found = nfc_.readPassiveTargetID(
            PN532_MIFARE_ISO14443A,
            uid,
            &uidLength,
            device::config::kNfcPollTimeoutMs
        );
        const uint32_t pollElapsedMs = millis() - pollStartMs;

        ++pollCount_;
        if (found) {
            consecutiveMisses_ = 0;
        } else {
            ++consecutiveMisses_;
        }

        // Heartbeat + anomaly logging: print every Nth poll, or any poll that
        // burned most of its timeout budget (a sign the PN532 link is struggling).
        const bool slowPoll = pollElapsedMs * 4 >= device::config::kNfcPollTimeoutMs * 3;
        if (slowPoll || (pollCount_ % 25) == 0) {
            Serial.printf("[nfc] poll #%lu found=%d elapsed=%lums timeout=%lums latch=%d misses=%lu heap=%lu\n",
                static_cast<unsigned long>(pollCount_),
                found ? 1 : 0,
                static_cast<unsigned long>(pollElapsedMs),
                static_cast<unsigned long>(device::config::kNfcPollTimeoutMs),
                tagPresentLatch_ ? 1 : 0,
                static_cast<unsigned long>(consecutiveMisses_),
                static_cast<unsigned long>(esp_get_free_heap_size()));
        }

        // After a run of misses, ping the chip with a lightweight command to
        // tell apart "PN532 stopped responding" (link desync) from
        // "PN532 fine, just doesn't see a card" (RF/coupling issue).
        if (consecutiveMisses_ > 0 && (consecutiveMisses_ % 40) == 0) {
            const uint32_t probeStartMs = millis();
            const uint32_t fw = nfc_.getFirmwareVersion();
            Serial.printf("[nfc] chip-alive probe misses=%lu result=%s elapsed=%lums\n",
                static_cast<unsigned long>(consecutiveMisses_),
                fw != 0 ? "responding" : "NO_RESPONSE",
                static_cast<unsigned long>(millis() - probeStartMs));
        }

        if (!found) {
            if (!tagPresentLatch_) {
                return false;
            }
            Serial.printf("[nfc] tag removed uid=%s\n", lastUidHex_.c_str());
            tagPresentLatch_ = false;
            lastUidLength_ = 0;
            lastUidHex_.clear();
            snapshot.clear();
            return true;
        }

        if (tagPresentLatch_ && sameUid(uid, uidLength, lastUid_.data(), lastUidLength_)) {
            Serial.printf("[nfc] poll #%lu found=1 same-uid=%s elapsed=%lums (no change, latch holds)\n",
                static_cast<unsigned long>(pollCount_),
                lastUidHex_.c_str(),
                static_cast<unsigned long>(pollElapsedMs));
            return false;
        }

        snapshot.clear();
        snapshot.present = true;
        snapshot.tagFamily.assign("Type2");
        assignUidHex(uid, uidLength, snapshot.uidHex);
        Serial.printf("[nfc] tag detected uid=%s uid_len=%u\n", snapshot.uidHex.c_str(), static_cast<unsigned>(uidLength));
        snapshot.addDetailLine("Tag detected");
        addUidDetail(snapshot);

        std::array<uint8_t, ((device::config::kType2EndPage - device::config::kType2StartPage) + 1) * 4> rawPages {};
        if (!readType2Pages(device::config::kType2StartPage, device::config::kType2EndPage, rawPages)) {
            Serial.printf("[nfc] read fail uid=%s pages=%u-%u\n", snapshot.uidHex.c_str(), device::config::kType2StartPage, device::config::kType2EndPage);
            snapshot.payloadKind = device::domain::NfcPayloadKind::ReadError;
            snapshot.errorCode = device::domain::ErrorCode::NfcReadError;
            snapshot.addDetailLine("Read pages 4-15 failed");
            rememberTag(uid, uidLength, nowMs);
            return true;
        }

        const std::string_view rawView(reinterpret_cast<const char*>(rawPages.data()), rawPages.size());
        FixedString<96> ndefPayload;
        if (device::domain::TagCodec::extractNdefMessage(rawView, ndefPayload)) {
            const auto decodedUuid = device::domain::TagCodec::decodeNdefTextRecord(ndefPayload.view());
            if (decodedUuid.has_value()) {
                snapshot.payloadKind = device::domain::NfcPayloadKind::NdefUuid;
                snapshot.uuid = *decodedUuid;
                Serial.printf("[nfc] payload=NDEF_UUID uid=%s uuid=%s\n", snapshot.uidHex.c_str(), snapshot.uuid.c_str());
                snapshot.addDetailLine("NDEF UUID parsed");
                rememberTag(uid, uidLength, nowMs);
                return true;
            }
        }

        if (device::domain::TagCodec::isBlankType2Data(rawView)) {
            snapshot.payloadKind = device::domain::NfcPayloadKind::Blank;
            Serial.printf("[nfc] payload=BLANK uid=%s\n", snapshot.uidHex.c_str());
            snapshot.addDetailLine("Blank user pages");
            rememberTag(uid, uidLength, nowMs);
            return true;
        }

        FixedString<32> legacyRaw;
        const std::string_view legacyView(reinterpret_cast<const char*>(rawPages.data()), 32);
        if (device::domain::TagCodec::extractLegacyRaw32(legacyView, legacyRaw)) {
            snapshot.payloadKind = device::domain::NfcPayloadKind::LegacyRaw32;
            snapshot.legacyRaw = legacyRaw;
            Serial.printf("[nfc] payload=LEGACY_RAW uid=%s raw=%s\n", snapshot.uidHex.c_str(), snapshot.legacyRaw.c_str());
            snapshot.addDetailLine("Legacy 32-byte payload");
            rememberTag(uid, uidLength, nowMs);
            return true;
        }

        snapshot.payloadKind = device::domain::NfcPayloadKind::Unsupported;
        snapshot.errorCode = device::domain::ErrorCode::NfcUnsupportedPayload;
        Serial.printf("[nfc] payload=UNSUPPORTED uid=%s\n", snapshot.uidHex.c_str());
        snapshot.addDetailLine("Unsupported NFC payload");
        rememberTag(uid, uidLength, nowMs);
        return true;
    }

    bool startProvision(device::domain::ProvisionSnapshot& snapshot) {
        snapshot.inProgress = true;
        snapshot.success = false;
        snapshot.stage = device::domain::ProvisionStage::Writing;
        snapshot.errorCode = device::domain::ErrorCode::None;
        snapshot.writtenUuid.clear();
        snapshot.clearDetails();
        snapshot.addDetailLine("Writing NDEF UUID");

        FixedString<36> generatedUuid;
        if (!generateRandomUuidV4(generatedUuid)) {
            snapshot.stage = device::domain::ProvisionStage::Failure;
            snapshot.inProgress = false;
            snapshot.errorCode = device::domain::ErrorCode::Validation;
            snapshot.addDetailLine("UUID generation failed");
            return false;
        }
        snapshot.writtenUuid = generatedUuid;
        Serial.printf("[nfc] provision generated uuid=%s chip=%s\n", snapshot.writtenUuid.c_str(), snapshot.uidHex.c_str());

        FixedString<96> tlvPayload;
        if (!device::domain::TagCodec::encodeNdefTextTlv(generatedUuid.view(), tlvPayload)) {
            snapshot.stage = device::domain::ProvisionStage::Failure;
            snapshot.inProgress = false;
            snapshot.errorCode = device::domain::ErrorCode::Validation;
            snapshot.addDetailLine("TLV encoding failed");
            return false;
        }

        std::array<uint8_t, ((device::config::kType2EndPage - device::config::kType2StartPage) + 1) * 4> pageBuffer {};
        if (!device::domain::TagCodec::packType2Data(tlvPayload.view(), pageBuffer)) {
            snapshot.stage = device::domain::ProvisionStage::Failure;
            snapshot.inProgress = false;
            snapshot.errorCode = device::domain::ErrorCode::Validation;
            snapshot.addDetailLine("Type2 page packing failed");
            return false;
        }

        if (!writeType2Pages(device::config::kType2StartPage, device::config::kType2EndPage, pageBuffer)) {
            snapshot.stage = device::domain::ProvisionStage::Failure;
            snapshot.inProgress = false;
            snapshot.errorCode = device::domain::ErrorCode::NfcWriteError;
            snapshot.addDetailLine("Write pages 4-15 failed");
            Serial.printf("[nfc] provision write failed uuid=%s chip=%s\n", snapshot.writtenUuid.c_str(), snapshot.uidHex.c_str());
            return false;
        }

        Serial.printf("[nfc] provision write ok uuid=%s chip=%s\n", snapshot.writtenUuid.c_str(), snapshot.uidHex.c_str());
        snapshot.stage = device::domain::ProvisionStage::Verifying;
        snapshot.clearDetails();
        snapshot.addDetailLine("Verifying written UUID");
        return true;
    }

    void finishProvision(device::domain::ProvisionSnapshot& snapshot) {
        std::array<uint8_t, ((device::config::kType2EndPage - device::config::kType2StartPage) + 1) * 4> rawPages {};
        if (!readType2Pages(device::config::kType2StartPage, device::config::kType2EndPage, rawPages)) {
            snapshot.stage = device::domain::ProvisionStage::Failure;
            snapshot.inProgress = false;
            snapshot.errorCode = device::domain::ErrorCode::NfcVerifyError;
            snapshot.clearDetails();
            snapshot.addDetailLine("Verify read failed");
            Serial.printf("[nfc] provision verify read failed uuid=%s chip=%s\n", snapshot.writtenUuid.c_str(), snapshot.uidHex.c_str());
            return;
        }

        FixedString<96> ndefPayload;
        const std::string_view rawView(reinterpret_cast<const char*>(rawPages.data()), rawPages.size());
        if (!device::domain::TagCodec::extractNdefMessage(rawView, ndefPayload)) {
            snapshot.stage = device::domain::ProvisionStage::Failure;
            snapshot.inProgress = false;
            snapshot.errorCode = device::domain::ErrorCode::NfcVerifyError;
            snapshot.clearDetails();
            snapshot.addDetailLine("Verify TLV parse failed");
            Serial.printf("[nfc] provision verify tlv failed uuid=%s chip=%s\n", snapshot.writtenUuid.c_str(), snapshot.uidHex.c_str());
            return;
        }

        const auto decodedUuid = device::domain::TagCodec::decodeNdefTextRecord(ndefPayload.view());
        if (!decodedUuid.has_value() || decodedUuid->view() != snapshot.writtenUuid.view()) {
            snapshot.stage = device::domain::ProvisionStage::Failure;
            snapshot.inProgress = false;
            snapshot.errorCode = device::domain::ErrorCode::NfcVerifyError;
            snapshot.clearDetails();
            snapshot.addDetailLine("Verify UUID mismatch");
            Serial.printf(
                "[nfc] provision verify mismatch expected=%s got=%s chip=%s\n",
                snapshot.writtenUuid.c_str(),
                decodedUuid.has_value() ? decodedUuid->c_str() : "<none>",
                snapshot.uidHex.c_str()
            );
            return;
        }

        Serial.printf("[nfc] provision verify ok uuid=%s chip=%s\n", snapshot.writtenUuid.c_str(), snapshot.uidHex.c_str());
        snapshot.stage = device::domain::ProvisionStage::Success;
        snapshot.inProgress = false;
        snapshot.success = true;
        snapshot.errorCode = device::domain::ErrorCode::None;
        snapshot.clearDetails();
        snapshot.addDetailLine("Provision complete");
        char detail[48] {};
        const auto written = snprintf(detail, sizeof(detail), "UUID: %s", snapshot.writtenUuid.c_str());
        snapshot.addDetailLine(written > 0 ? detail : "UUID unavailable");
    }

private:
    template <std::size_t Size>
    bool readType2Pages(
        uint8_t startPage,
        uint8_t endPage,
        std::array<uint8_t, Size>& output
    ) {
        constexpr std::size_t kPageBytes = 4;
        if (((endPage - startPage) + 1) * kPageBytes != output.size()) {
            return false;
        }

        std::size_t offset = 0;
        for (uint8_t page = startPage; page <= endPage; ++page) {
            uint8_t buffer[4] {};
            if (!nfc_.mifareultralight_ReadPage(page, buffer)) {
                Serial.printf("[nfc] read page fail page=%u\n", static_cast<unsigned>(page));
                return false;
            }
            for (std::size_t index = 0; index < kPageBytes; ++index) {
                output[offset++] = buffer[index];
            }
            delay(5);
        }
        return true;
    }

    template <std::size_t Size>
    bool writeType2Pages(
        uint8_t startPage,
        uint8_t endPage,
        const std::array<uint8_t, Size>& input
    ) {
        constexpr std::size_t kPageBytes = 4;
        if (((endPage - startPage) + 1) * kPageBytes != input.size()) {
            return false;
        }

        std::size_t offset = 0;
        for (uint8_t page = startPage; page <= endPage; ++page) {
            uint8_t buffer[4] {};
            for (std::size_t index = 0; index < kPageBytes; ++index) {
                buffer[index] = input[offset++];
            }

            bool written = false;
            for (uint8_t attempt = 0; attempt < device::config::kNfcPageWriteRetries; ++attempt) {
                if (nfc_.ntag2xx_WritePage(page, buffer)) {
                    Serial.printf("[nfc] write page=%u attempt=%u ok data=%02x%02x%02x%02x\n",
                        static_cast<unsigned>(page),
                        static_cast<unsigned>(attempt + 1),
                        buffer[0], buffer[1], buffer[2], buffer[3]);
                    written = true;
                    break;
                }
                Serial.printf("[nfc] write page=%u attempt=%u failed\n",
                    static_cast<unsigned>(page),
                    static_cast<unsigned>(attempt + 1));
                delay(20);
            }

            if (!written) {
                return false;
            }
            delay(10);
        }
        return true;
    }

    static bool sameUid(const uint8_t* lhs, uint8_t lhsLength, const uint8_t* rhs, uint8_t rhsLength) {
        return lhsLength == rhsLength && std::memcmp(lhs, rhs, lhsLength) == 0;
    }

    static void assignUidHex(const uint8_t* uid, uint8_t uidLength, FixedString<32>& output) {
        char buffer[32] {};
        std::size_t offset = 0;
        for (uint8_t index = 0; index < uidLength && (offset + 2) < sizeof(buffer); ++index) {
            const auto written = snprintf(&buffer[offset], sizeof(buffer) - offset, "%02x", uid[index]);
            if (written <= 0) {
                break;
            }
            offset += static_cast<std::size_t>(written);
        }
        output.assign({buffer, offset});
    }

    static void addUidDetail(device::domain::NfcTagSnapshot& snapshot) {
        char buffer[48] {};
        const auto written = snprintf(buffer, sizeof(buffer), "UID: %s", snapshot.uidHex.c_str());
        snapshot.addDetailLine(written > 0 ? buffer : "UID unavailable");
    }

    static bool generateRandomUuidV4(FixedString<36>& output) {
        std::array<uint8_t, 16> seed {};
        for (std::size_t offset = 0; offset < seed.size(); offset += 4) {
            const uint32_t randomWord = esp_random();
            seed[offset + 0] = static_cast<uint8_t>((randomWord >> 24) & 0xFF);
            seed[offset + 1] = static_cast<uint8_t>((randomWord >> 16) & 0xFF);
            seed[offset + 2] = static_cast<uint8_t>((randomWord >> 8) & 0xFF);
            seed[offset + 3] = static_cast<uint8_t>(randomWord & 0xFF);
        }
        return device::domain::TagCodec::formatUuidV4(seed, output);
    }

    void rememberTag(const uint8_t* uid, uint8_t uidLength, uint32_t nowMs) {
        lastUid_.fill(0);
        std::memcpy(lastUid_.data(), uid, uidLength);
        lastUidLength_ = uidLength;
        lastProcessedAtMs_ = nowMs;
        tagPresentLatch_ = true;
        assignUidHex(uid, uidLength, lastUidHex_);
    }

    Adafruit_PN532 nfc_ {device::board::PN532_SS};
    bool initialized_ = false;
    NfcProbeResult cachedProbeResult_ {};
    std::array<uint8_t, 7> lastUid_ {};
    uint8_t lastUidLength_ = 0;
    uint32_t lastProcessedAtMs_ = 0;
    bool tagPresentLatch_ = false;
    FixedString<32> lastUidHex_ {};
    uint32_t pollCount_ = 0;
    uint32_t consecutiveMisses_ = 0;
};

}

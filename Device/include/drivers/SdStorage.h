#pragma once

#include <SD.h>
#include <SPI.h>

#include "board/Pinout.h"
#include "core/FixedString.h"

namespace device::drivers {

struct SdProbeResult {
    bool ok = false;
    bool mounted = false;
    uint64_t cardSizeMb = 0;
    FixedString<48> detail;
};

class SdStorage {
public:
    SdStorage()
        : spiBus_(HSPI) {
    }

    SdProbeResult probe() {
        pinMode(device::board::SD_CS, OUTPUT);
        digitalWrite(device::board::SD_CS, HIGH);

        spiBus_.begin(device::board::SD_SCK, device::board::SD_MISO, device::board::SD_MOSI, device::board::SD_CS);
        delay(10);

        SdProbeResult result;

        // A single SD.begin() right after power-up often fails while the card's
        // internal regulator is still settling. Retry a few times with SD.end()
        // and a growing back-off before giving up.
        bool mounted = false;
        for (uint8_t attempt = 1; attempt <= kMountAttempts; ++attempt) {
            if (SD.begin(device::board::SD_CS, spiBus_, kSpiHz)) {
                mounted = true;
                break;
            }
            Serial.printf("[sd] mount attempt %u/%u failed\n",
                          static_cast<unsigned>(attempt),
                          static_cast<unsigned>(kMountAttempts));
            SD.end();
            delay(static_cast<uint32_t>(attempt) * 150U);
        }
        if (!mounted) {
            result.detail.assign("SD mount failed");
            return result;
        }

        result.mounted = true;
        result.cardSizeMb = SD.cardSize() / (1024ULL * 1024ULL);

        File file = SD.open("/boot.chk", FILE_WRITE);
        if (!file) {
            result.detail.assign("SD boot.chk open failed");
            return result;
        }
        file.print("boot-ok");
        file.close();

        file = SD.open("/boot.chk", FILE_READ);
        if (!file) {
            result.detail.assign("SD boot.chk read failed");
            return result;
        }

        char buffer[16] {};
        const auto bytesRead = file.readBytes(buffer, sizeof(buffer) - 1);
        file.close();
        SD.remove("/boot.chk");

        if (bytesRead != 7 || strncmp(buffer, "boot-ok", 7) != 0) {
            result.detail.assign("SD boot.chk verify failed");
            return result;
        }

        char detail[48] {};
        const auto written = snprintf(
            detail,
            sizeof(detail),
            "SD %llu MB smoke OK",
            static_cast<unsigned long long>(result.cardSizeMb)
        );
        result.ok = true;
        result.detail.assign({detail, written > 0 ? static_cast<std::size_t>(written) : 0});
        return result;
    }

private:
    static constexpr uint8_t  kMountAttempts = 3;
    static constexpr uint32_t kSpiHz         = 4000000U;

    SPIClass spiBus_;
};

}  // namespace device::drivers

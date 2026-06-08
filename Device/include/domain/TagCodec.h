#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <optional>
#include <string_view>

#include "core/FixedString.h"

namespace device::domain {

class TagCodec {
public:
    static bool normalizeUuid(std::string_view input, FixedString<36>& output) {
        if (input.size() != 36) {
            output.clear();
            return false;
        }

        for (std::size_t index = 0; index < input.size(); ++index) {
            const char ch = input[index];
            const bool isHyphen = index == 8 || index == 13 || index == 18 || index == 23;
            if (isHyphen) {
                if (ch != '-') {
                    output.clear();
                    return false;
                }
                continue;
            }
            if (!std::isxdigit(static_cast<unsigned char>(ch))) {
                output.clear();
                return false;
            }
        }

        std::array<char, 36> normalized {};
        std::transform(input.begin(), input.end(), normalized.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return output.assign(std::string_view(normalized.data(), normalized.size()));
    }

    static bool encodeNdefTextRecord(std::string_view uuid, FixedString<96>& output) {
        FixedString<36> normalizedUuid;
        if (!normalizeUuid(uuid, normalizedUuid)) {
            output.clear();
            return false;
        }

        std::array<char, 96> bytes {};
        std::size_t index = 0;
        bytes[index++] = static_cast<char>(0xD1);
        bytes[index++] = static_cast<char>(0x01);
        bytes[index++] = static_cast<char>(1 + 2 + normalizedUuid.size());
        bytes[index++] = 'T';
        bytes[index++] = static_cast<char>(0x02);
        bytes[index++] = 'e';
        bytes[index++] = 'n';
        for (char ch : normalizedUuid.view()) {
            bytes[index++] = ch;
        }
        return output.assign(std::string_view(bytes.data(), index));
    }

    static bool encodeNdefTextTlv(std::string_view uuid, FixedString<96>& output) {
        FixedString<96> record;
        if (!encodeNdefTextRecord(uuid, record)) {
            output.clear();
            return false;
        }

        std::array<char, 96> bytes {};
        std::size_t index = 0;
        bytes[index++] = static_cast<char>(0x03);
        bytes[index++] = static_cast<char>(record.size());
        for (char ch : record.view()) {
            bytes[index++] = ch;
        }
        bytes[index++] = static_cast<char>(0xFE);
        return output.assign(std::string_view(bytes.data(), index));
    }

    static std::optional<FixedString<36>> decodeNdefTextRecord(std::string_view payload) {
        if (payload.size() < 7 || static_cast<unsigned char>(payload[0]) != 0xD1 || payload[3] != 'T') {
            return std::nullopt;
        }

        const auto languageSize = static_cast<std::size_t>(payload[4] & 0x3F);
        const auto textOffset = static_cast<std::size_t>(5 + languageSize);
        if (payload.size() < textOffset) {
            return std::nullopt;
        }

        FixedString<36> normalizedUuid;
        if (!normalizeUuid(payload.substr(textOffset), normalizedUuid)) {
            return std::nullopt;
        }

        return normalizedUuid;
    }

    static bool extractNdefMessage(std::string_view rawPages, FixedString<96>& output) {
        output.clear();
        std::size_t index = 0;
        while (index < rawPages.size()) {
            const auto tlvType = static_cast<unsigned char>(rawPages[index]);
            if (tlvType == 0x00) {
                ++index;
                continue;
            }
            if (tlvType == 0xFE) {
                return false;
            }
            if (index + 1 >= rawPages.size()) {
                return false;
            }

            const auto tlvLength = static_cast<unsigned char>(rawPages[index + 1]);
            if (tlvLength == 0xFF) {
                return false;
            }

            const auto valueOffset = index + 2;
            if (valueOffset + tlvLength > rawPages.size()) {
                return false;
            }

            if (tlvType == 0x03) {
                return output.assign(rawPages.substr(valueOffset, tlvLength));
            }

            index = valueOffset + tlvLength;
        }

        return false;
    }

    static bool extractLegacyRaw32(std::string_view rawPages, FixedString<32>& output) {
        output.clear();
        if (rawPages.size() < 32) {
            return false;
        }

        for (std::size_t index = 0; index < 32; ++index) {
            const unsigned char ch = static_cast<unsigned char>(rawPages[index]);
            if (ch < 32 || ch > 126) {
                output.clear();
                return false;
            }
        }

        return output.assign(rawPages.substr(0, 32));
    }

    static bool isBlankType2Data(std::string_view rawPages) {
        return std::all_of(rawPages.begin(), rawPages.end(), [](unsigned char ch) {
            return ch == 0x00 || ch == 0xFF;
        });
    }

    template <std::size_t Size>
    static bool packType2Data(std::string_view tlvPayload, std::array<uint8_t, Size>& output) {
        if (tlvPayload.size() > output.size()) {
            output.fill(0);
            return false;
        }

        output.fill(0);
        for (std::size_t index = 0; index < tlvPayload.size(); ++index) {
            output[index] = static_cast<uint8_t>(tlvPayload[index]);
        }
        return true;
    }

    static bool formatUuidV4(std::array<uint8_t, 16> seed, FixedString<36>& output) {
        seed[6] = static_cast<uint8_t>((seed[6] & 0x0F) | 0x40);
        seed[8] = static_cast<uint8_t>((seed[8] & 0x3F) | 0x80);

        constexpr char kHex[] = "0123456789abcdef";
        std::array<char, 36> formatted {};
        std::size_t outIndex = 0;

        for (std::size_t index = 0; index < seed.size(); ++index) {
            if (index == 4 || index == 6 || index == 8 || index == 10) {
                formatted[outIndex++] = '-';
            }
            formatted[outIndex++] = kHex[(seed[index] >> 4) & 0x0F];
            formatted[outIndex++] = kHex[seed[index] & 0x0F];
        }

        return output.assign(std::string_view(formatted.data(), formatted.size()));
    }
};

}

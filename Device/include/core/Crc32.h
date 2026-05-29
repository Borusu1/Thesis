#pragma once

#include <cstdint>
#include <string_view>

inline uint32_t crc32(std::string_view input) {
    uint32_t crc = 0xFFFFFFFFu;
    for (unsigned char byte : input) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            const uint32_t mask = static_cast<uint32_t>(-(static_cast<int32_t>(crc & 1u)));
            crc = (crc >> 1u) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

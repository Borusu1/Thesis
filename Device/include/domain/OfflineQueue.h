#pragma once

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "config/DeviceConfig.h"
#include "core/Crc32.h"
#include "domain/Models.h"

namespace device::domain {

class OfflineQueue {
public:
    bool enqueue(const OfflineOperationRecord& record) {
        if (size_ >= records_.size()) {
            return false;
        }
        records_[size_++] = record;
        return true;
    }

    std::optional<OfflineOperationRecord> dequeue() {
        if (size_ == 0) {
            return std::nullopt;
        }

        OfflineOperationRecord first = records_[0];
        for (std::size_t index = 1; index < size_; ++index) {
            records_[index - 1] = records_[index];
        }
        --size_;
        records_[size_] = {};
        return first;
    }

    std::size_t size() const {
        return size_;
    }

    bool empty() const {
        return size_ == 0;
    }

    static std::string serialize(const OfflineOperationRecord& record) {
        std::string payload;
        payload.reserve(256);
        payload += std::to_string(static_cast<int>(record.operationType));
        payload += '|';
        payload += record.clientOperationId.view();
        payload += '|';
        payload += record.tagUuid.view();
        payload += '|';
        payload += std::to_string(record.productId);
        payload += '|';
        payload += std::to_string(record.quantity);
        payload += '|';
        payload += record.chipUidHex.view();
        payload += '|';
        payload += record.note.view();

        const auto checksum = crc32(payload);
        payload += '|';
        payload += std::to_string(checksum);
        return payload;
    }

    static std::optional<OfflineOperationRecord> deserialize(std::string_view line) {
        std::array<std::string_view, 8> parts {};
        std::size_t start = 0;
        std::size_t count = 0;

        while (start <= line.size() && count < parts.size()) {
            const auto separator = line.find('|', start);
            if (separator == std::string_view::npos) {
                parts[count++] = line.substr(start);
                break;
            }
            parts[count++] = line.substr(start, separator - start);
            start = separator + 1;
        }

        if (count != parts.size()) {
            return std::nullopt;
        }

        std::string body(line.substr(0, line.rfind('|')));
        const auto expectedChecksum = crc32(body);
        uint32_t parsedChecksum = 0;
        if (!parseUnsigned(parts[7], parsedChecksum) || parsedChecksum != expectedChecksum) {
            return std::nullopt;
        }

        OfflineOperationRecord record;
        uint32_t operationType = 0;
        if (!parseUnsigned(parts[0], operationType) || operationType > static_cast<uint32_t>(OperationType::ShipmentFull)) {
            return std::nullopt;
        }
        record.operationType = static_cast<OperationType>(operationType);
        if (!record.clientOperationId.assign(parts[1]) || !record.tagUuid.assign(parts[2])) {
            return std::nullopt;
        }
        if (!parseUnsigned(parts[3], record.productId) || !parseUnsigned(parts[4], record.quantity)) {
            return std::nullopt;
        }
        if (!record.chipUidHex.assign(parts[5]) || !record.note.assign(parts[6])) {
            return std::nullopt;
        }
        return record;
    }

    static uint32_t retryBackoffMs(uint8_t retryCount) {
        return device::config::kSyncBackoffBaseMs * (1UL << retryCount);
    }

private:
    static bool parseUnsigned(std::string_view input, uint32_t& output) {
        const auto* begin = input.data();
        const auto* end = begin + input.size();
        const auto result = std::from_chars(begin, end, output);
        return result.ec == std::errc{} && result.ptr == end;
    }

    std::array<OfflineOperationRecord, device::config::kMaxOfflineOperations> records_ {};
    std::size_t size_ = 0;
};

}  // namespace device::domain

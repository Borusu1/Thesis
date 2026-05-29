#pragma once

#include <array>
#include <cstddef>
#include <cstring>
#include <string_view>

template <std::size_t Capacity>
class FixedString {
public:
    constexpr FixedString() = default;

    explicit FixedString(std::string_view value) {
        assign(value);
    }

    bool assign(std::string_view value) {
        if (value.size() > Capacity) {
            clear();
            return false;
        }

        clear();
        std::memcpy(buffer_.data(), value.data(), value.size());
        size_ = value.size();
        return true;
    }

    void clear() {
        buffer_.fill('\0');
        size_ = 0;
    }

    constexpr std::size_t size() const {
        return size_;
    }

    constexpr std::size_t capacity() const {
        return Capacity;
    }

    constexpr bool empty() const {
        return size_ == 0;
    }

    constexpr const char* c_str() const {
        return buffer_.data();
    }

    constexpr std::string_view view() const {
        return std::string_view(buffer_.data(), size_);
    }

private:
    std::array<char, Capacity + 1> buffer_ {};
    std::size_t size_ = 0;
};

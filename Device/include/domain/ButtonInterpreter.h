#pragma once

#include <cstdint>
#include <optional>

#include "config/DeviceConfig.h"

namespace device::domain {

enum class ButtonPressType : uint8_t {
    ShortPress,
    LongPress,
};

struct ButtonEvent {
    device::config::Button button;
    ButtonPressType type;
};

class ButtonInterpreter {
public:
    std::optional<ButtonEvent> update(device::config::Button button, bool isPressed, uint32_t nowMs) {
        if (button != activeButton_) {
            state_ = State::Idle;
            activeButton_ = button;
        }

        switch (state_) {
            case State::Idle:
                if (isPressed) {
                    state_ = State::Debouncing;
                    pressedAtMs_ = nowMs;
                }
                return std::nullopt;
            case State::Debouncing:
                if (!isPressed) {
                    state_ = State::Idle;
                    return std::nullopt;
                }
                if (nowMs - pressedAtMs_ >= device::config::kDebounceMs) {
                    state_ = State::Pressed;
                }
                return std::nullopt;
            case State::Pressed:
                if (!isPressed) {
                    state_ = State::Idle;
                    return ButtonEvent {button, ButtonPressType::ShortPress};
                }
                if (nowMs - pressedAtMs_ >= device::config::kLongPressThresholdMs) {
                    state_ = State::LongPressReported;
                    return ButtonEvent {button, ButtonPressType::LongPress};
                }
                return std::nullopt;
            case State::LongPressReported:
                if (!isPressed) {
                    state_ = State::Idle;
                }
                return std::nullopt;
        }

        return std::nullopt;
    }

private:
    enum class State : uint8_t {
        Idle,
        Debouncing,
        Pressed,
        LongPressReported,
    };

    State state_ = State::Idle;
    device::config::Button activeButton_ = device::config::Button::Ok;
    uint32_t pressedAtMs_ = 0;
};

}

#pragma once

#include <array>
#include <optional>

#include <Arduino.h>

#include "board/Pinout.h"
#include "config/DeviceConfig.h"
#include "domain/ButtonInterpreter.h"

namespace device::drivers {

class ButtonDriver {
public:
    void begin() {
        pinMode(device::board::BUTTON_UP_PIN, INPUT_PULLUP);
        pinMode(device::board::BUTTON_DOWN_PIN, INPUT_PULLUP);
        pinMode(device::board::BUTTON_OK_PIN, INPUT_PULLUP);
    }

    std::optional<device::domain::ButtonEvent> poll(uint32_t nowMs) {
        const auto buttonDefs = buttonDefinitions();
        for (std::size_t index = 0; index < buttonDefs.size(); ++index) {
            const bool pressed = digitalRead(buttonDefs[index].pin) == LOW;
            const auto event = interpreters_[index].update(buttonDefs[index].button, pressed, nowMs);
            if (event.has_value()) {
                return event;
            }
        }
        return std::nullopt;
    }

private:
    struct ButtonDefinition {
        device::config::Button button;
        int pin;
    };

    static constexpr std::array<ButtonDefinition, 3> buttonDefinitions() {
        return {{
            {device::config::Button::Up, device::board::BUTTON_UP_PIN},
            {device::config::Button::Down, device::board::BUTTON_DOWN_PIN},
            {device::config::Button::Ok, device::board::BUTTON_OK_PIN},
        }};
    }

    std::array<device::domain::ButtonInterpreter, 3> interpreters_ {};
};

}  // namespace device::drivers

#pragma once

#include <array>
#include <cstdint>

namespace device::board {

constexpr int TFT_CS = 4;
constexpr int TFT_RST = 5;
constexpr int TFT_DC = 6;
constexpr int TFT_MOSI = 7;
constexpr int TFT_SCK = 15;
constexpr int TFT_MISO = 16;

constexpr int PN532_SS = 17;

constexpr int SD_CS = 11;
constexpr int SD_MOSI = 13;
constexpr int SD_SCK = 12;
constexpr int SD_MISO = 14;

constexpr int LED_PIN = 2;
constexpr int ERROR_LED_PIN = 1;
constexpr int BUTTON_UP_PIN = 38;
constexpr int BUTTON_DOWN_PIN = 35;
constexpr int BUTTON_OK_PIN = 21;

constexpr int ETH_MOSI = 37;
constexpr int ETH_SCK  = 36;
constexpr int ETH_CS   = 40;
constexpr int ETH_INT  = 42;
constexpr int ETH_RST  = 41;
constexpr int ETH_MISO = 39;

}

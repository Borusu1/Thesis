#pragma once

#include <Arduino.h>
#include <Ethernet_Generic.h>
#include <SPI.h>

#include "board/Pinout.h"
#include "core/FixedString.h"

namespace device::drivers {

// Manages WIZ820io (W5200) via FSPI with dynamic SPI pin switching.
// TFT+PN532 also use FSPI (pins TFT_SCK/TFT_MISO/TFT_MOSI).
// Call prepareSpi() before any W5200 transaction, restoreSpi() after.
//
// NOTE: Ethernet.linkStatus() is NOT used here.
// On W5200, Ethernet_Generic returns LinkOFF regardless of physical state,
// causing spurious disconnect events. Disconnect is instead detected via
// DHCP lease renewal failure (Ethernet.maintain() return codes).
class EthernetManager {
public:
    // Hardware reset + DHCP. Returns true if IP obtained.
    // Sets hardwarePresent_ even when DHCP fails (cable unplugged at boot),
    // so maintain() can retry DHCP later when the cable is plugged in.
    bool begin() {
        hardwareReset();

        prepareSpi();
        Ethernet.init(device::board::ETH_CS);

        // Derive MAC from eFuse once — cached in mac_[] for reconnect retries.
        const uint64_t efuse = ESP.getEfuseMac();
        mac_[0] = 0x02; mac_[1] = 0x00;
        mac_[2] = static_cast<uint8_t>((efuse >> 32) & 0xFF);
        mac_[3] = static_cast<uint8_t>((efuse >> 16) & 0xFF);
        mac_[4] = static_cast<uint8_t>((efuse >>  8) & 0xFF);
        mac_[5] = static_cast<uint8_t>( efuse        & 0xFF);

        const int result = Ethernet.begin(mac_, 8000, 3000);

        if (Ethernet.hardwareStatus() == EthernetNoHardware) {
            Serial.println("[eth] W5200 not found (check wiring)");
            restoreSpi();
            return false;
        }

        // Remember hardware is present even if DHCP failed (no cable at boot)
        hardwarePresent_ = true;

        const IPAddress ip = Ethernet.localIP();
        restoreSpi();

        if (result == 0 || ip == IPAddress(0, 0, 0, 0)) {
            Serial.println("[eth] DHCP failed — no link or no DHCP server");
            return false;
        }

        hasIp_ = true;
        cachedIp_ = ip;
        Serial.printf("[eth] DHCP ok ip=%s\n", ip.toString().c_str());
        return true;
    }

    // Call periodically.
    // - When connected: renews DHCP lease; clears IP on renewal failure.
    // - When not connected: retries DHCP every kDhcpRetryIntervalMs.
    void maintain(uint32_t nowMs) {
        if (!hardwarePresent_) return;

        // Rate-limit to avoid constant SPI pin-switching
        if (nowMs - lastMaintainAtMs_ < kMaintainIntervalMs) return;
        lastMaintainAtMs_ = nowMs;

        prepareSpi();

        if (hasIp_) {
            // Ethernet.maintain() handles DHCP lease renewal internally.
            // It returns non-zero failure codes only when the lease is expiring
            // and renewal requests get no response — reliable indicator of link loss.
            const int rc = Ethernet.maintain();
            if (rc == DHCP_CHECK_RENEW_FAIL || rc == DHCP_CHECK_REBIND_FAIL) {
                hasIp_ = false;
                cachedIp_ = IPAddress(0, 0, 0, 0);
                Serial.printf("[eth] DHCP renewal failed (rc=%d) — link lost\n", rc);
            }
        } else if (nowMs - lastDhcpAttemptAtMs_ >= kDhcpRetryIntervalMs) {
            // No IP — attempt DHCP. Hardware-reset first: after a link-down event
            // the W5200 PHY state machine must be fully restarted before DHCP works.
            lastDhcpAttemptAtMs_ = nowMs;

            restoreSpi();
            hardwareReset();
            prepareSpi();
            Ethernet.init(device::board::ETH_CS);

            Serial.println("[eth] retrying DHCP...");
            const int result = Ethernet.begin(mac_, 2000, 1000);
            const IPAddress ip = Ethernet.localIP();
            if (result != 0 && ip != IPAddress(0, 0, 0, 0)) {
                hasIp_ = true;
                cachedIp_ = ip;
                Serial.printf("[eth] DHCP ok ip=%s\n", ip.toString().c_str());
            } else {
                Serial.println("[eth] DHCP no response");
            }
        }

        restoreSpi();
    }

    // Force an immediate hardware-reset + DHCP (re)acquire, bypassing the
    // internal retry timer. The caller MUST guarantee a quiet RF window
    // (WiFi radio off) — otherwise WiFi jams the W5200 RX and DHCP fails.
    // Returns true if an IP was obtained.
    bool forceReacquire() {
        if (!hardwarePresent_) return false;

        restoreSpi();
        hardwareReset();
        prepareSpi();
        Ethernet.init(device::board::ETH_CS);

        Serial.println("[eth] retrying DHCP...");
        // Generous window: the PHY just re-negotiated link after the reset and
        // the WiFi radio has only just powered down, so give DHCP time to land.
        const int result = Ethernet.begin(mac_, 5000, 2000);
        const IPAddress ip = Ethernet.localIP();
        restoreSpi();

        if (result != 0 && ip != IPAddress(0, 0, 0, 0)) {
            hasIp_ = true;
            cachedIp_ = ip;
            Serial.printf("[eth] DHCP ok ip=%s\n", ip.toString().c_str());
            return true;
        }
        Serial.println("[eth] DHCP no response");
        return false;
    }

    bool hardwarePresent() const { return hardwarePresent_; }

    bool isConnected() const {
        return hardwarePresent_ && hasIp_;
    }

    IPAddress localIp() const { return cachedIp_; }

    EthernetClient& client() { return client_; }

    // Switch global SPI to ETH pins (36/39/37). Call before any W5200 SPI op.
    void prepareSpi() const {
        SPI.end();
        SPI.begin(device::board::ETH_SCK, device::board::ETH_MISO,
                  device::board::ETH_MOSI, device::board::ETH_CS);
    }

    // Restore global SPI to TFT/PN532 pins (15/16/7).
    static void restoreSpi() {
        SPI.end();
        SPI.begin(device::board::TFT_SCK, device::board::TFT_MISO,
                  device::board::TFT_MOSI, device::board::TFT_CS);
    }

private:
    void hardwareReset() const {
        pinMode(device::board::ETH_RST, OUTPUT);
        digitalWrite(device::board::ETH_RST, LOW);
        delay(25);
        digitalWrite(device::board::ETH_RST, HIGH);
        delay(150);
    }

    static constexpr uint32_t kMaintainIntervalMs  = 3000;  // SPI check every 3s
    static constexpr uint32_t kDhcpRetryIntervalMs = 10000; // DHCP retry every 10s when no IP

    bool hardwarePresent_ = false;
    bool hasIp_           = false;
    uint8_t mac_[6] {};
    IPAddress cachedIp_;
    EthernetClient client_;
    uint32_t lastMaintainAtMs_    = 0;
    uint32_t lastDhcpAttemptAtMs_ = 0;
};

}  // namespace device::drivers

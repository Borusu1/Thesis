#pragma once

#include <Arduino.h>
#include <Ethernet_Generic.h>
#include <SPI.h>

#include "board/Pinout.h"
#include "core/FixedString.h"

namespace device::drivers {

class EthernetManager {
public:

    bool begin() {
        hardwareReset();

        prepareSpi();
        Ethernet.init(device::board::ETH_CS);

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

    void maintain(uint32_t nowMs) {
        if (!hardwarePresent_) return;

        if (nowMs - lastMaintainAtMs_ < kMaintainIntervalMs) return;
        lastMaintainAtMs_ = nowMs;

        prepareSpi();

        if (hasIp_) {

            const int rc = Ethernet.maintain();
            if (rc == DHCP_CHECK_RENEW_FAIL || rc == DHCP_CHECK_REBIND_FAIL) {
                hasIp_ = false;
                cachedIp_ = IPAddress(0, 0, 0, 0);
                Serial.printf("[eth] DHCP renewal failed (rc=%d) — link lost\n", rc);
            }
        } else if (nowMs - lastDhcpAttemptAtMs_ >= kDhcpRetryIntervalMs) {

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

    bool forceReacquire() {
        if (!hardwarePresent_) return false;

        restoreSpi();
        hardwareReset();
        prepareSpi();
        Ethernet.init(device::board::ETH_CS);

        Serial.println("[eth] retrying DHCP...");

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

    void prepareSpi() const {
        SPI.end();
        SPI.begin(device::board::ETH_SCK, device::board::ETH_MISO,
                  device::board::ETH_MOSI, device::board::ETH_CS);
    }

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

    static constexpr uint32_t kMaintainIntervalMs  = 3000;
    static constexpr uint32_t kDhcpRetryIntervalMs = 10000;

    bool hardwarePresent_ = false;
    bool hasIp_           = false;
    uint8_t mac_[6] {};
    IPAddress cachedIp_;
    EthernetClient client_;
    uint32_t lastMaintainAtMs_    = 0;
    uint32_t lastDhcpAttemptAtMs_ = 0;
};

}

#pragma once

#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include "drivers/EthernetManager.h"
#include "drivers/WifiManager.h"
#include "domain/Models.h"
#include "config/DeviceConfig.h"

namespace device::drivers {

class NetworkManager {
public:

    bool beginEthernet() {
        if (!device::config::kEthernetEnabled) return false;
        return eth_.begin();
    }

    void setWifiConfig(const device::domain::DeviceBootstrapConfig& config) {
        wifiConfig_ = &config;
    }

    void maintain(uint32_t nowMs) {
        if (device::config::kEthernetEnabled) {
            if (eth_.isConnected()) {
                if (wifi_.isStarted()) {
                    wifi_.stop();
                }
                eth_.maintain(nowMs);
                return;
            }

            if (eth_.hardwarePresent() &&
                (lastEthProbeAtMs_ == 0 || nowMs - lastEthProbeAtMs_ >= kEthProbeIntervalMs)) {
                lastEthProbeAtMs_ = nowMs;
                if (wifi_.isStarted()) {
                    wifi_.stop();

                    delay(300);
                }
                if (eth_.forceReacquire()) {

                    if (wifiEverStarted_) {
                        Serial.println("[net] ETH recovered after WiFi — restarting for a clean Ethernet stack");
                        Serial.flush();
                        delay(50);
                        ESP.restart();
                    }
                    return;
                }
            }
        }

        // Ethernet disabled or still unavailable — use WiFi as the fallback link, unless
        // WiFi is disabled, in which case stay Ethernet-only / fully offline
        // (never start the radio, never send a request over it).
        if (device::config::kWifiEnabled && wifiConfig_ != nullptr) {
            if (!wifi_.isStarted()) {
                wifi_.begin(*wifiConfig_);
                wifiEverStarted_ = true;
            }
            const uint32_t maintainStartMs = millis();
            wifi_.ensureConnected(nowMs);
            const uint32_t maintainElapsedMs = millis() - maintainStartMs;
            if (maintainElapsedMs >= 20) {
                // ensureConnected is normally a status check (sub-ms); a spike
                // here means it kicked off a reconnect — that's a window where
                // the radio is busy and SPI peripherals can get glitched.
                Serial.printf("[net] wifi maintain took %lums (likely reconnect)\n",
                    static_cast<unsigned long>(maintainElapsedMs));
            }
        }
    }

    bool isConnected() const {
        return eth_.isConnected() || wifi_.isConnected();
    }

    bool isEthernetConnected() const { return eth_.isConnected(); }
    bool isWifiConnected()    const { return wifi_.isConnected(); }

    IPAddress localIp() const {
        if (eth_.isConnected()) return eth_.localIp();
        return wifi_.localIp();
    }

    Client* activeClient() {
        if (eth_.isConnected()) return &eth_.client();
        return &wifiPlainClient_;
    }

    Client* activeSecureClient() {
        if (eth_.isConnected()) return &eth_.client();
        wifiSecureClient_.setInsecure();
        return &wifiSecureClient_;
    }

    void prepareSpi() {
        if (eth_.isConnected()) eth_.prepareSpi();
    }

    void restoreSpi() {
        if (eth_.isConnected()) EthernetManager::restoreSpi();
    }

    EthernetManager& ethernetManager() { return eth_; }

private:
    EthernetManager  eth_;
    WifiManager      wifi_;
    WiFiClient       wifiPlainClient_;
    WiFiClientSecure wifiSecureClient_;
    const device::domain::DeviceBootstrapConfig* wifiConfig_ = nullptr;

    static constexpr uint32_t kEthProbeIntervalMs = 15000;
    uint32_t lastEthProbeAtMs_ = 0;
    bool wifiEverStarted_ = false;
};

}

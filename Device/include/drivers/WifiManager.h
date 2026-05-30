#pragma once

#include <WiFi.h>

#include "domain/Models.h"

namespace device::drivers {

class WifiManager {
public:
    void begin(const device::domain::DeviceBootstrapConfig& config) {
        config_ = config;
        started_ = true;
        WiFi.mode(WIFI_STA);
        WiFi.begin(config_.wifiSsid.c_str(), config_.wifiPassword.c_str());
        lastAttemptAtMs_ = millis();
    }

    void ensureConnected(uint32_t nowMs) {
        if (!started_) {
            return;
        }

        if (WiFi.status() == WL_CONNECTED) {
            return;
        }

        if (nowMs - lastAttemptAtMs_ < 10000UL) {
            return;
        }

        WiFi.disconnect(false, false);
        WiFi.begin(config_.wifiSsid.c_str(), config_.wifiPassword.c_str());
        lastAttemptAtMs_ = nowMs;
    }

    // Fully power down the WiFi radio. Required before any Ethernet (W5200)
    // activity: a running WiFi STA jams the W5200's packet reception.
    void stop() {
        if (!started_) {
            return;
        }
        WiFi.disconnect(true, false);  // disconnect and switch the radio off
        WiFi.mode(WIFI_OFF);
        started_ = false;
    }

    bool isStarted() const {
        return started_;
    }

    bool isConnected() const {
        return started_ && WiFi.status() == WL_CONNECTED;
    }

    IPAddress localIp() const {
        return WiFi.localIP();
    }

private:
    bool started_ = false;
    uint32_t lastAttemptAtMs_ = 0;
    device::domain::DeviceBootstrapConfig config_ {};
};

}  // namespace device::drivers

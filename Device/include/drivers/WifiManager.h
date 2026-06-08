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
        lastLoggedStatus_ = WiFi.status();
        Serial.printf("[wifi] connecting ssid=\"%s\" pass_len=%u status=%s\n",
            config_.wifiSsid.c_str(),
            static_cast<unsigned>(config_.wifiPassword.view().size()),
            statusToString(lastLoggedStatus_));
    }

    void ensureConnected(uint32_t nowMs) {
        if (!started_) {
            return;
        }

        const wl_status_t status = WiFi.status();
        if (status != lastLoggedStatus_) {
            Serial.printf("[wifi] status changed %s -> %s\n",
                statusToString(lastLoggedStatus_), statusToString(status));
            lastLoggedStatus_ = status;
        }

        if (status == WL_CONNECTED) {
            return;
        }

        if (nowMs - lastAttemptAtMs_ < 10000UL) {
            return;
        }

        Serial.printf("[wifi] retrying connect ssid=\"%s\" last_status=%s\n",
            config_.wifiSsid.c_str(), statusToString(status));
        WiFi.disconnect(false, false);
        WiFi.begin(config_.wifiSsid.c_str(), config_.wifiPassword.c_str());
        lastAttemptAtMs_ = nowMs;
    }

    void stop() {
        if (!started_) {
            return;
        }
        WiFi.disconnect(true, false);
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
    static const char* statusToString(wl_status_t status) {
        switch (status) {
            case WL_NO_SHIELD:       return "no_shield";
            case WL_IDLE_STATUS:     return "idle";
            case WL_NO_SSID_AVAIL:   return "no_ssid_avail";
            case WL_SCAN_COMPLETED:  return "scan_completed";
            case WL_CONNECTED:       return "connected";
            case WL_CONNECT_FAILED:  return "connect_failed";
            case WL_CONNECTION_LOST: return "connection_lost";
            case WL_DISCONNECTED:    return "disconnected";
            default:                 return "unknown";
        }
    }

    bool started_ = false;
    uint32_t lastAttemptAtMs_ = 0;
    wl_status_t lastLoggedStatus_ = WL_IDLE_STATUS;
    device::domain::DeviceBootstrapConfig config_ {};
};

}

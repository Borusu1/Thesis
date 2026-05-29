#pragma once

#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include "drivers/EthernetManager.h"
#include "drivers/WifiManager.h"
#include "domain/Models.h"

namespace device::drivers {

// Manages network connectivity: Ethernet first, WiFi as fallback.
// Exposes a single Client* for HTTP requests and handles SPI switching for ETH.
class NetworkManager {
public:
    // Attempt Ethernet (DHCP). Returns true if ETH got an IP.
    bool beginEthernet() {
        return eth_.begin();
    }

    // Start WiFi with credentials from config (fallback if ETH failed).
    void beginWifi(const device::domain::DeviceBootstrapConfig& config) {
        wifiConfig_ = &config;
        wifi_.begin(config);
    }

    // Maintain connections: renew DHCP, reconnect WiFi if needed.
    // WiFi always runs as hot standby — instant failover when ETH cable is pulled.
    void maintain(uint32_t nowMs) {
        eth_.maintain(nowMs);
        if (wifiConfig_ != nullptr) {
            wifi_.ensureConnected(nowMs);
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

    // Returns the Client suited for the active interface.
    Client* activeClient() {
        if (eth_.isConnected()) return &eth_.client();
        return &wifiPlainClient_;
    }

    Client* activeSecureClient() {
        if (eth_.isConnected()) return &eth_.client();  // no TLS over ETH for now
        wifiSecureClient_.setInsecure();
        return &wifiSecureClient_;
    }

    // Configure global SPI for ETH before HTTP calls (no-op for WiFi).
    void prepareSpi() {
        if (eth_.isConnected()) eth_.prepareSpi();
    }

    // Restore global SPI to TFT/PN532 pins after HTTP calls.
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
};

}  // namespace device::drivers

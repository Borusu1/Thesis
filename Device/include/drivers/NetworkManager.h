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

    // Store WiFi credentials for fallback use. Does NOT start WiFi — maintain()
    // brings it up only when Ethernet is unavailable (see note below).
    void setWifiConfig(const device::domain::DeviceBootstrapConfig& config) {
        wifiConfig_ = &config;
    }

    // Maintain connections, enforcing that Ethernet and WiFi never run at the
    // same time: a live/scanning WiFi STA jams the W5200's packet reception, so
    // a concurrent WiFi was preventing Ethernet TCP/DHCP from ever completing.
    //
    // Priority is Ethernet-first:
    //  - ETH connected   -> keep WiFi radio off, just renew the DHCP lease.
    //  - ETH down + W5200 present -> periodically stop WiFi to open a quiet RF
    //    window and force an Ethernet (re)acquire; on success WiFi stays off.
    //  - ETH still down   -> run WiFi as the fallback link.
    void maintain(uint32_t nowMs) {
        if (eth_.isConnected()) {
            if (wifi_.isStarted()) {
                wifi_.stop();  // free the air so the W5200 keeps receiving
            }
            eth_.maintain(nowMs);  // DHCP lease renewal only
            return;
        }

        // Ethernet is down. If the W5200 is present, periodically grab a quiet
        // RF window (WiFi off) and try to (re)acquire Ethernet.
        if (eth_.hardwarePresent() &&
            (lastEthProbeAtMs_ == 0 || nowMs - lastEthProbeAtMs_ >= kEthProbeIntervalMs)) {
            lastEthProbeAtMs_ = nowMs;
            if (wifi_.isStarted()) {
                wifi_.stop();
                // Let the WiFi radio fully power down before touching the W5200,
                // otherwise residual RF activity still jams its reception.
                delay(300);
            }
            if (eth_.forceReacquire()) {
                // Ethernet is back. If the WiFi radio was ever powered on this
                // session, turning it off (WIFI_OFF) frees enough air for DHCP
                // to land, but residual RF/coexistence still corrupts larger
                // TCP payloads (HTTP requests arrive malformed at the server).
                // The only reliable cure is a clean boot with WiFi never
                // initialized, so restart into the known-good Ethernet path.
                // Pending operations are persisted on SD, so nothing is lost.
                if (wifiEverStarted_) {
                    Serial.println("[net] ETH recovered after WiFi — restarting for a clean Ethernet stack");
                    Serial.flush();
                    delay(50);
                    ESP.restart();
                }
                return;  // Ethernet is back; WiFi stays off
            }
        }

        // Ethernet still unavailable — use WiFi as the fallback link.
        if (wifiConfig_ != nullptr) {
            if (!wifi_.isStarted()) {
                wifi_.begin(*wifiConfig_);
                wifiEverStarted_ = true;
            }
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

    // How often to interrupt WiFi fallback to probe for Ethernet recovery.
    static constexpr uint32_t kEthProbeIntervalMs = 15000;
    uint32_t lastEthProbeAtMs_ = 0;
    bool wifiEverStarted_ = false;
};

}  // namespace device::drivers

#include "NetworkTasks.h"
#include "AppGlobals.h"
#include "AppUtils.h"
#include "UiRender.h"
#include "ScreenReceipt.h"
#include <Arduino.h>
#include "config/DeviceConfig.h"

void startNetworkIfConfigured() {
    if (!g_networkStatus.configLoaded || !g_bootstrapConfig.valid) {
        g_networkStatus.detail.assign("No network config");
        g_controller.updateNetworkStatus(g_networkStatus);
        return;
    }

    g_network.setWifiConfig(g_bootstrapConfig);

    if (g_network.isEthernetConnected()) {
        Serial.println("[net] ETH up — WiFi stays off (fallback only)");
    } else {
        Serial.println("[net] ETH not up — WiFi fallback will start, ETH probed periodically");
        g_networkStatus.detail.assign("Connecting...");
    }
    g_controller.updateNetworkStatus(g_networkStatus);
}

bool syncProductsFromBackend(uint32_t nowMs, bool force) {
    if (!g_networkStatus.configLoaded || !g_bootstrapConfig.valid || !g_network.isConnected() || !g_networkStatus.deviceAuthenticated) {
        return false;
    }

    if (!force && g_lastProductSyncAttemptAtMs != 0 &&
        nowMs - g_lastProductSyncAttemptAtMs < g_bootstrapConfig.productSyncIntervalMs) {
        return false;
    }

    g_lastProductSyncAttemptAtMs = nowMs;
    g_networkStatus.syncInProgress = true;
    g_controller.updateNetworkStatus(g_networkStatus);
    Serial.printf("[sync] product sync started force=%d\n", force ? 1 : 0);
    renderCurrentScreen();

    std::array<device::domain::ProductRecord, device::config::kMaxRecentProducts> products {};
    std::size_t count = 0;
    FixedString<48> detail;
    const bool fetchOk = g_apiClient.fetchRecentProducts(
        g_bootstrapConfig.apiBaseUrl.view(),
        g_deviceToken.view(),
        products,
        count,
        detail
    );

    if (!fetchOk) {
        Serial.printf("[sync] product sync failed detail=%s code=%d\n", detail.c_str(), g_apiClient.lastHttpCode());
        g_networkStatus.syncInProgress = false;
        g_networkStatus.productSyncOk = false;
        g_networkStatus.detail.assign(detail.view());
        if (g_apiClient.lastHttpCode() == 401) {
            g_deviceToken.clear();
            g_networkStatus.deviceAuthenticated = false;
        }
        updateControllerStatus();
        return true;
    }

    if (!g_journal.replaceProducts(products.data(), count, g_catalogStatus)) {
        Serial.println("[sync] product sync failed: local DB save");
        g_networkStatus.syncInProgress = false;
        g_networkStatus.productSyncOk = false;
        g_networkStatus.detail.assign("DB product save fail");
        updateControllerStatus();
        return true;
    }

    refreshCatalogStatus();
    g_networkStatus.syncInProgress = false;
    g_networkStatus.productSyncOk = true;
    g_networkStatus.lastSyncAtMs = nowMs;
    g_networkStatus.detail.assign(detail.view());
    Serial.printf("[sync] product sync ok count=%lu detail=%s\n", static_cast<unsigned long>(count), detail.c_str());
    updateControllerStatus();

    if (g_controller.stateMachine().currentScreen() == device::ui::Screen::InventoryReceipt) {
        initializeReceiptSelection();
    }

    return true;
}

bool runNetworkMaintenance(uint32_t nowMs) {
    bool changed = false;
    const auto currentScreen = g_controller.stateMachine().currentScreen();
    const bool allowBackgroundHttp =
        currentScreen == device::ui::Screen::ModeMenu ||
        currentScreen == device::ui::Screen::SyncStatus;

    // Gating to the main menu only matters for Ethernet: its DHCP probe blocks
    // for seconds and would make working modes laggy (see 498c048). WiFi
    // connect/maintain is async and never blocks, so when Ethernet is disabled
    // there is nothing to gate — run it every loop so WiFi comes up and stays
    // up regardless of which screen the operator is on.
    if (currentScreen == device::ui::Screen::ModeMenu || !device::config::kEthernetEnabled) {
        g_network.maintain(nowMs);
    }

    const bool networkConnected = g_network.isConnected();
    const bool ethConnected     = g_network.isEthernetConnected();

    if (networkConnected != g_networkStatus.wifiConnected ||
        ethConnected != g_networkStatus.ethernetConnected) {
        const bool prevEth = g_networkStatus.ethernetConnected;
        g_networkStatus.wifiConnected     = networkConnected;
        g_networkStatus.ethernetConnected = ethConnected;
        if (networkConnected) {
            char buffer[48] {};
            const auto written = ethConnected
                ? snprintf(buffer, sizeof(buffer), "ETH %s",  g_network.localIp().toString().c_str())
                : snprintf(buffer, sizeof(buffer), "WiFi %s", g_network.localIp().toString().c_str());
            g_networkStatus.detail.assign(written > 0 ? buffer
                : (ethConnected ? "ETH connected" : "WiFi connected"));
            if (prevEth && !ethConnected) {
                Serial.printf("[net] ETH lost — failed over to WiFi ip=%s\n",
                    g_network.localIp().toString().c_str());
            } else if (!prevEth && ethConnected) {
                Serial.printf("[net] ETH came up — switched from WiFi ip=%s\n",
                    g_network.localIp().toString().c_str());
            } else {
                Serial.printf("[net] connected interface=%s ip=%s\n",
                    ethConnected ? "eth" : "wifi", g_network.localIp().toString().c_str());
            }
        } else if (g_networkStatus.configLoaded) {
            g_networkStatus.detail.assign("Network reconnecting");
            Serial.println("[net] disconnected, reconnecting");
        }
        changed = true;
    }

    if (allowBackgroundHttp &&
        g_networkStatus.configLoaded &&
        networkConnected &&
        !g_networkStatus.deviceAuthenticated &&
        (g_lastAuthAttemptAtMs == 0 || nowMs - g_lastAuthAttemptAtMs >= 10000UL)) {
        Serial.println("[net] device auth attempt");
        g_lastAuthAttemptAtMs = nowMs;
        g_networkStatus.detail.assign("Authenticating...");
        g_controller.updateNetworkStatus(g_networkStatus);
        renderCurrentScreen();
        FixedString<48> detail;
        const bool previousAuth = g_networkStatus.deviceAuthenticated;
        const auto previousDetail = g_networkStatus.detail;
        if (g_apiClient.authenticateDevice(g_bootstrapConfig, g_deviceToken, detail)) {
            g_networkStatus.deviceAuthenticated = true;
            g_networkStatus.detail.assign(detail.view());
            Serial.printf("[net] device auth ok detail=%s\n", detail.c_str());
        } else {
            g_networkStatus.deviceAuthenticated = false;
            g_networkStatus.productSyncOk = false;
            g_networkStatus.detail.assign(detail.view());
            g_deviceToken.clear();
            Serial.printf("[net] device auth fail detail=%s code=%d\n", detail.c_str(), g_apiClient.lastHttpCode());
        }
        changed = changed ||
            previousAuth != g_networkStatus.deviceAuthenticated ||
            previousDetail.view() != g_networkStatus.detail.view();
    }

    if (allowBackgroundHttp && g_networkStatus.configLoaded && syncProductsFromBackend(nowMs, false)) {
        changed = true;
    }

    if (changed) {
        updateControllerStatus();
    }
    return changed;
}

bool syncNextPendingOperation(uint32_t nowMs, bool force) {
    refreshSyncStatus();
    if (!g_dbStatus.ready || g_syncStatus.pendingCount == 0) {
        return false;
    }

    if (!g_networkStatus.wifiConnected || !g_networkStatus.deviceAuthenticated) {
        g_syncStatus.lastError.assign("Offline");
        updateControllerStatus();
        return false;
    }

    std::array<device::domain::InventoryOperationRecord, 1> operations {};
    std::size_t count = 0;
    if (!g_journal.listPendingOperations(operations, count) || count == 0) {
        refreshSyncStatus();
        updateControllerStatus();
        return false;
    }

    // Remembers whether the previous attempt failed before reaching the backend
    // (connect/timeout, code < 0). Such transport misses are transient network
    // hiccups, not backend rejections, so they retry quickly instead of falling
    // into the exponential backoff meant for a backend that is actually failing.
    static bool s_lastOpSyncTransportError = false;

    const auto& operation = operations[0];
    // Backoff gate first — this runs every loop while an op is pending, so do
    // NOT log before it or the serial floods with one line per iteration.
    const uint32_t cappedAttempts = operation.syncAttemptCount > 4 ? 4 : operation.syncAttemptCount;
    const uint32_t backoffMs = s_lastOpSyncTransportError
        ? device::config::kOperationTransportRetryMs
        : (device::config::kSyncBackoffBaseMs << cappedAttempts);
    if (!force && g_lastOperationSyncAttemptAtMs != 0 &&
        nowMs - g_lastOperationSyncAttemptAtMs < backoffMs) {
        return false;
    }

    g_lastOperationSyncAttemptAtMs = nowMs;
    Serial.printf(
        "[sync] pending op type=%s id=%s attempts=%u pending=%lu force=%d\n",
        device::domain::toString(operation.operationType),
        operation.clientOperationId.c_str(),
        static_cast<unsigned>(operation.syncAttemptCount),
        static_cast<unsigned long>(g_syncStatus.pendingCount),
        force ? 1 : 0
    );
    g_syncStatus.inProgress = true;
    g_syncStatus.lastError.clear();
    updateControllerStatus();
    renderCurrentScreen();

    FixedString<48> detail;
    const bool ok = g_apiClient.syncInventoryOperation(
        g_bootstrapConfig.apiBaseUrl.view(),
        g_deviceToken.view(),
        operation,
        detail
    );

    g_syncStatus.inProgress = false;
    s_lastOpSyncTransportError = !ok && g_apiClient.lastHttpCode() < 0;
    if (ok) {
        g_journal.markOperationSynced(operation.clientOperationId.view(), nowMs);
        Serial.printf("[sync] op synced id=%s\n", operation.clientOperationId.c_str());
        g_syncStatus.lastSyncAtMs = nowMs;
        g_syncStatus.lastSyncedCount = 1;
        g_syncStatus.lastError.assign("Sync applied");
        refreshSyncStatus();
        updateControllerStatus();
        return true;
    }

    if (g_apiClient.lastHttpCode() == 401) {
        g_deviceToken.clear();
        g_networkStatus.deviceAuthenticated = false;
        Serial.println("[sync] device auth expired during sync");
    }

    g_journal.markOperationSyncFailed(operation.clientOperationId.view(), detail.view());
    Serial.printf("[sync] op failed id=%s detail=%s code=%d\n", operation.clientOperationId.c_str(), detail.c_str(), g_apiClient.lastHttpCode());
    g_syncStatus.lastError.assign(detail.view());
    refreshSyncStatus();
    updateControllerStatus();
    return true;
}

#include "AppUtils.h"
#include "AppGlobals.h"
#include <Arduino.h>
#include <ESP.h>
#include "config/DeviceConfig.h"

uint32_t hashStringView(std::string_view value) {
    uint32_t hash = 2166136261UL;
    for (char ch : value) {
        hash ^= static_cast<uint8_t>(ch);
        hash *= 16777619UL;
    }
    return hash;
}

uint32_t modeMenuSignature() {
    uint32_t signature = static_cast<uint32_t>(g_controller.stateMachine().modeMenuSelection());
    signature = signature * 131UL + (g_dbStatus.ready ? 1UL : 0UL);
    signature = signature * 131UL + g_dbStatus.bootId;
    signature = signature * 131UL + g_catalogStatus.totalProducts;
    signature = signature * 131UL + (g_catalogStatus.loaded ? 1UL : 0UL);
    signature = signature * 131UL + (g_networkStatus.configLoaded ? 1UL : 0UL);
    signature = signature * 131UL + (g_networkStatus.wifiConnected ? 1UL : 0UL);
    signature = signature * 131UL + (g_networkStatus.ethernetConnected ? 1UL : 0UL);
    signature = signature * 131UL + (g_networkStatus.deviceAuthenticated ? 1UL : 0UL);
    signature = signature * 131UL + g_syncStatus.pendingCount;
    signature = signature * 131UL + (g_syncStatus.inProgress ? 1UL : 0UL);
    signature = signature * 131UL + (g_networkStatus.syncInProgress ? 1UL : 0UL);
    signature ^= hashStringView(g_networkStatus.detail.view());
    signature ^= hashStringView(g_syncStatus.lastError.view());
    return signature;
}

device::domain::DeviceEventRecord makeEvent(
    device::domain::DeviceEventType type,
    device::domain::DeviceEventSeverity severity,
    const char* screen
) {
    device::domain::DeviceEventRecord event;
    event.bootId = g_dbStatus.bootId;
    event.createdAtMs = millis();
    event.eventType = type;
    event.severity = severity;
    event.screen.assign(screen);
    return event;
}

void logEvent(const device::domain::DeviceEventRecord& event) {
    if (!g_dbStatus.ready) {
        return;
    }
    if (!g_journal.logEvent(event)) {
        g_dbStatus = g_journal.status();
        g_controller.updateDatabaseStatus(g_dbStatus);
    }
}

void logModeEntered(const char* screen) {
    auto event = makeEvent(device::domain::DeviceEventType::ModeEntered, device::domain::DeviceEventSeverity::Info, screen);
    event.message.assign(screen);
    logEvent(event);
}

const char* screenName(device::ui::Screen screen) {
    switch (screen) {
        case device::ui::Screen::BootDiagnostics:
            return "boot_diagnostics";
        case device::ui::Screen::DiagnosticsFailure:
            return "diagnostics_failure";
        case device::ui::Screen::DiagnosticsSuccess:
            return "diagnostics_success";
        case device::ui::Screen::ModeMenu:
            return "mode_menu";
        case device::ui::Screen::SyncStatus:
            return "sync_status";
        case device::ui::Screen::InventoryLookup:
            return "lookup";
        case device::ui::Screen::InventoryReceipt:
            return "receipt";
        case device::ui::Screen::InventoryShipment:
            return "shipment";
        case device::ui::Screen::NfcProvision:
            return "provision";
        case device::ui::Screen::DiagnosticsView:
            return "diagnostics";
    }
    return "unknown";
}

void resolveHardwareDeviceId() {
    const uint64_t efuseMac = ESP.getEfuseMac();
    char buffer[32] {};
    const auto written = snprintf(
        buffer,
        sizeof(buffer),
        "%s%04lx%08lx",
        device::config::kDeviceIdPrefix,
        static_cast<unsigned long>((efuseMac >> 32) & 0xFFFFULL),
        static_cast<unsigned long>(efuseMac & 0xFFFFFFFFULL)
    );
    if (written > 0) {
        g_bootstrapConfig.deviceId.assign(std::string_view(buffer, static_cast<std::size_t>(written)));
    }
}

void refreshSyncStatus() {
    g_syncStatus.pendingCount = static_cast<uint32_t>(g_journal.pendingOperationCount());
    g_networkStatus.pendingSyncCount = g_syncStatus.pendingCount;
    g_controller.updateSyncStatus(g_syncStatus);
}

void updateControllerStatus() {
    g_controller.updateDatabaseStatus(g_dbStatus);
    g_controller.updateCatalogStatus(g_catalogStatus);
    g_controller.updateNetworkStatus(g_networkStatus);
    refreshSyncStatus();
}

void refreshCatalogStatus() {
    g_journal.refreshProductCatalogStatus(g_catalogStatus);
    g_controller.updateCatalogStatus(g_catalogStatus);
}

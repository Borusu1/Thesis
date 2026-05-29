#include <Arduino.h>
#include <ESP.h>

#include "AppGlobals.h"
#include "AppUtils.h"
#include "Boot.h"
#include "NetworkTasks.h"
#include "UiRender.h"
#include "ScreenReceipt.h"
#include "ScreenShipment.h"
#include "ScreenLookup.h"
#include "ScreenProvision.h"

// ---------------------------------------------------------------------------
// Global variable definitions (external linkage — extern'd in AppGlobals.h)
// ---------------------------------------------------------------------------

device::app::AppController g_controller;
device::drivers::ButtonDriver g_buttons;
device::drivers::BootstrapConfigStore g_bootstrapStore;
device::drivers::DeviceApiClient g_apiClient;
device::drivers::DisplayDriver g_display;
device::drivers::EventJournalStore g_journal;
device::drivers::NfcDriver g_nfc;
device::drivers::SdStorage g_sd;
device::drivers::NetworkManager g_network;
device::domain::DiagnosticsSnapshot g_snapshot;
device::domain::NfcTagSnapshot g_tagSnapshot;
device::domain::LookupSnapshot g_lookupSnapshot;
bool g_lookupFrozen = false;
device::domain::ReceiptSnapshot g_receiptSnapshot;
device::domain::ShipmentSnapshot g_shipmentSnapshot;
device::domain::ProvisionSnapshot g_provisionSnapshot;
device::domain::DatabaseStatusSnapshot g_dbStatus;
device::domain::ProductCatalogStatus g_catalogStatus;
device::domain::DeviceBootstrapConfig g_bootstrapConfig;
device::domain::NetworkStatusSnapshot g_networkStatus;
device::domain::SyncStatusSnapshot g_syncStatus;
FixedString<256> g_deviceToken;
uint32_t g_lastProductSyncAttemptAtMs = 0;
uint32_t g_lastAuthAttemptAtMs = 0;
uint32_t g_lastOperationSyncAttemptAtMs = 0;
device::ui::Screen g_lastRenderedScreen = device::ui::Screen::BootDiagnostics;
uint32_t g_lastModeMenuSignature = 0;
device::ui::Screen g_lastLoggedScreen = device::ui::Screen::BootDiagnostics;

// ---------------------------------------------------------------------------
// Sync-status screen button handler (only called from loop)
// ---------------------------------------------------------------------------

static bool handleSyncStatusButtonEvent(const device::domain::ButtonEvent& event) {
    if (event.type == device::domain::ButtonPressType::LongPress && event.button == device::config::Button::Ok) {
        g_controller.setScreen(device::ui::Screen::ModeMenu);
        return true;
    }

    if (event.type != device::domain::ButtonPressType::ShortPress) {
        return false;
    }

    if (event.button == device::config::Button::Up) {
        if (g_journal.clearPendingSyncQueue(millis())) {
            refreshSyncStatus();
            g_syncStatus.lastError.assign("Queue cleared");
        } else {
            g_syncStatus.lastError.assign("Queue clear failed");
        }
        updateControllerStatus();
        return true;
    }

    if (event.button != device::config::Button::Ok) {
        return false;
    }

    syncNextPendingOperation(millis(), true);
    return true;
}

// ---------------------------------------------------------------------------
// setup / loop
// ---------------------------------------------------------------------------

void setup() {
    Serial.begin(115200);
    delay(200);

    Serial.println("\n========================================");
    Serial.printf("  %s  v%s\n", device::config::kFirmwareName, device::config::kFirmwareVersion);
    Serial.println("========================================");
    Serial.printf("[boot] chip: ESP32-S3  cores: %d  freq: %lu MHz\n",
        ESP.getChipCores(), (unsigned long)(ESP.getCpuFreqMHz()));
    Serial.printf("[boot] flash: %lu KB  heap: %lu KB free\n",
        (unsigned long)(ESP.getFlashChipSize() / 1024),
        (unsigned long)(ESP.getFreeHeap() / 1024));
    {
        const uint64_t mac = ESP.getEfuseMac();
        Serial.printf("[boot] efuse mac: %02X:%02X:%02X:%02X:%02X:%02X\n",
            (uint8_t)((mac >> 40) & 0xFF), (uint8_t)((mac >> 32) & 0xFF),
            (uint8_t)((mac >> 24) & 0xFF), (uint8_t)((mac >> 16) & 0xFF),
            (uint8_t)((mac >>  8) & 0xFF), (uint8_t)( mac        & 0xFF));
    }
    Serial.println("[boot] pins: TFT FSPI=7/15/16  ETH FSPI=37/36/39  SD HSPI=12/14/13");
    Serial.println("[boot] pins: BTN up=38 down=35 ok=21  LED=2 errLED=1");
    Serial.println("----------------------------------------");

    setupBoard();
    g_apiClient.setNetworkManager(&g_network);

    Serial.println("[boot] initializing buttons...");
    g_buttons.begin();

    Serial.println("[boot] initializing display (ILI9341 SPI)...");
    const bool displayOk = g_display.begin();
    Serial.printf("[boot] display: %s\n", displayOk ? "OK (320x240 landscape)" : "FAILED");

    g_controller.startDiagnostics();

    g_snapshot = runDiagnostics(displayOk);
    g_controller.finishDiagnostics(g_snapshot);
    updateControllerStatus();

    Serial.println("----------------------------------------");
    Serial.println("[boot] starting network...");
    startNetworkIfConfigured();
    runNetworkMaintenance(millis());

    // Append network result to boot diagnostics screen, then briefly show it
    if (g_network.isEthernetConnected()) {
        char buf[48] {};
        snprintf(buf, sizeof(buf), "ETH OK  %s", g_network.localIp().toString().c_str());
        g_snapshot.addDetailLine(buf);
    } else if (g_network.isWifiConnected()) {
        char buf[48] {};
        snprintf(buf, sizeof(buf), "WiFi OK  %s", g_network.localIp().toString().c_str());
        g_snapshot.addDetailLine(buf);
    } else {
        g_snapshot.addDetailLine("Network: offline");
    }
    g_display.renderDiagnostics(g_snapshot, device::ui::Screen::BootDiagnostics,
        g_snapshot.detailCount > device::config::kVisibleDetailLines
            ? g_snapshot.detailCount - device::config::kVisibleDetailLines : 0);
    delay(600);

    renderCurrentScreen();

    const bool bootOk = (g_snapshot.failureCode == device::domain::ErrorCode::None);
    Serial.println("----------------------------------------");
    Serial.printf("[boot] diagnostics result: %s (code=%d)\n",
        bootOk ? "PASS" : "FAIL",
        static_cast<int>(g_snapshot.failureCode));
    Serial.printf("[boot] network: eth=%s  wifi=%s  auth=%s\n",
        g_networkStatus.ethernetConnected ? "up" : "down",
        g_networkStatus.wifiConnected     ? "up" : "down",
        g_networkStatus.deviceAuthenticated ? "ok" : "pending");
    Serial.printf("[boot] heap after init: %lu KB free\n",
        (unsigned long)(ESP.getFreeHeap() / 1024));
    Serial.println("========================================");
    Serial.println("[boot] entering main loop");
    Serial.println("========================================\n");

    if (g_dbStatus.ready) {
        logEvent(makeEvent(device::domain::DeviceEventType::BootStarted, device::domain::DeviceEventSeverity::Info, "boot"));
        if (bootOk) {
            logEvent(makeEvent(device::domain::DeviceEventType::BootCompleted, device::domain::DeviceEventSeverity::Info, "diagnostics"));
        } else {
            auto event = makeEvent(device::domain::DeviceEventType::DiagnosticsFailed, device::domain::DeviceEventSeverity::Error, "diagnostics");
            event.resultCode.assign(device::domain::toString(g_snapshot.failureCode));
            logEvent(event);
        }
    }
}

void loop() {
    const uint32_t nowMs = millis();

    const auto buttonEvent = g_buttons.poll(millis());
    if (buttonEvent.has_value()) {
        const auto currentScreen = g_controller.stateMachine().currentScreen();
        if (currentScreen == device::ui::Screen::SyncStatus && handleSyncStatusButtonEvent(*buttonEvent)) {
            renderCurrentScreen();
            return;
        }
        if (currentScreen == device::ui::Screen::InventoryReceipt && handleReceiptButtonEvent(*buttonEvent)) {
            renderCurrentScreen();
            return;
        }
        if (currentScreen == device::ui::Screen::InventoryShipment && handleShipmentButtonEvent(*buttonEvent)) {
            renderCurrentScreen();
            return;
        }
        if (currentScreen == device::ui::Screen::InventoryLookup && g_lookupFrozen
                && buttonEvent->button == device::config::Button::Ok) {
            g_lookupFrozen = false;
            g_nfc.resetScanState();
            g_controller.clearLookupSnapshot();
            renderCurrentScreen();
            return;
        }

        const auto action = g_controller.handleButtonEvent(*buttonEvent);
        if (action == device::ui::AppStateMachine::Action::RetryDiagnostics) {
            const bool displayOk = g_display.begin();
            g_snapshot = runDiagnostics(displayOk);
            g_controller.finishDiagnostics(g_snapshot);
            updateControllerStatus();
            startNetworkIfConfigured();
            g_controller.clearNfcSnapshot();
            g_controller.clearLookupSnapshot();
            g_controller.clearReceiptSnapshot();
            g_controller.clearShipmentSnapshot();
            g_controller.clearProvisionSnapshot();
        } else if (action == device::ui::AppStateMachine::Action::EnterModeMenu) {
            g_lookupFrozen = false;
            g_nfc.resetScanState();
            g_controller.clearNfcSnapshot();
            g_controller.clearLookupSnapshot();
            g_controller.clearReceiptSnapshot();
            g_controller.clearShipmentSnapshot();
            g_controller.clearProvisionSnapshot();
            refreshSyncStatus();
            logModeEntered("mode_menu");
        } else if (action == device::ui::AppStateMachine::Action::EnterSyncStatus) {
            refreshSyncStatus();
            logModeEntered("sync_status");
        } else if (action == device::ui::AppStateMachine::Action::EnterLookupMode) {
            g_lookupFrozen = false;
            g_nfc.resetScanState();
            g_controller.clearNfcSnapshot();
            g_controller.clearLookupSnapshot();
            g_controller.clearReceiptSnapshot();
            g_controller.clearShipmentSnapshot();
            g_controller.clearProvisionSnapshot();
            logModeEntered("lookup");
        } else if (action == device::ui::AppStateMachine::Action::EnterReceiptMode) {
            g_nfc.resetScanState();
            g_controller.clearNfcSnapshot();
            g_controller.clearLookupSnapshot();
            g_controller.clearReceiptSnapshot();
            g_controller.clearShipmentSnapshot();
            g_controller.clearProvisionSnapshot();
            initializeReceiptSelection();
            logModeEntered("receipt");
        } else if (action == device::ui::AppStateMachine::Action::EnterShipmentMode) {
            g_nfc.resetScanState();
            g_controller.clearNfcSnapshot();
            g_controller.clearLookupSnapshot();
            g_controller.clearReceiptSnapshot();
            g_controller.clearShipmentSnapshot();
            g_controller.clearProvisionSnapshot();
            initializeShipmentScreen();
            logModeEntered("shipment");
        } else if (action == device::ui::AppStateMachine::Action::EnterProvisionMode) {
            g_nfc.resetScanState();
            g_controller.clearNfcSnapshot();
            g_controller.clearLookupSnapshot();
            g_controller.clearReceiptSnapshot();
            g_controller.clearShipmentSnapshot();
            g_controller.clearProvisionSnapshot();
            logModeEntered("nfc_provision");
        } else if (action == device::ui::AppStateMachine::Action::EnterDiagnosticsView) {
            logModeEntered("diagnostics");
        } else if (action == device::ui::AppStateMachine::Action::ResetLookupScan) {
            g_nfc.resetScanState();
            g_controller.clearNfcSnapshot();
            g_controller.clearLookupSnapshot();
            auto event = makeEvent(device::domain::DeviceEventType::UserResetScan, device::domain::DeviceEventSeverity::Info, "lookup");
            event.message.assign("lookup reset");
            logEvent(event);
        } else if (action == device::ui::AppStateMachine::Action::ResetReceiptFlow) {
            initializeReceiptSelection();
            auto event = makeEvent(device::domain::DeviceEventType::UserResetScan, device::domain::DeviceEventSeverity::Info, "receipt");
            event.message.assign("receipt reset");
            logEvent(event);
        } else if (action == device::ui::AppStateMachine::Action::ResetShipmentFlow) {
            initializeShipmentScreen();
            auto event = makeEvent(device::domain::DeviceEventType::UserResetScan, device::domain::DeviceEventSeverity::Info, "shipment");
            event.message.assign("shipment reset");
            logEvent(event);
        } else if (action == device::ui::AppStateMachine::Action::ResetProvisionFlow) {
            g_nfc.resetScanState();
            g_controller.clearNfcSnapshot();
            g_controller.clearProvisionSnapshot();
            auto event = makeEvent(device::domain::DeviceEventType::UserResetScan, device::domain::DeviceEventSeverity::Info, "nfc_provision");
            event.message.assign("provision reset");
            logEvent(event);
        } else if (action == device::ui::AppStateMachine::Action::ReturnToModeMenu) {
            g_nfc.resetScanState();
            g_controller.clearNfcSnapshot();
            g_controller.clearLookupSnapshot();
            g_controller.clearReceiptSnapshot();
            g_controller.clearShipmentSnapshot();
            g_controller.clearProvisionSnapshot();
            refreshSyncStatus();
            logModeEntered("mode_menu");
        } else if (action == device::ui::AppStateMachine::Action::ReturnToDiagnostics) {
            g_controller.clearLookupSnapshot();
            g_controller.clearReceiptSnapshot();
            g_controller.clearShipmentSnapshot();
            g_controller.clearNfcSnapshot();
            g_controller.clearProvisionSnapshot();
        }

        renderCurrentScreen();
    }

    if (runNetworkMaintenance(nowMs) &&
        g_controller.stateMachine().currentScreen() == device::ui::Screen::ModeMenu) {
        renderCurrentScreen();
    }

    if ((g_controller.stateMachine().currentScreen() == device::ui::Screen::ModeMenu ||
         g_controller.stateMachine().currentScreen() == device::ui::Screen::SyncStatus) &&
        syncNextPendingOperation(nowMs, false)) {
        renderCurrentScreen();
    }

    const auto currentScreen = g_controller.stateMachine().currentScreen();

    if (currentScreen == device::ui::Screen::InventoryLookup) {
        handleLookupNfcLoop();
        return;
    }

    if (currentScreen == device::ui::Screen::InventoryReceipt) {
        handleReceiptNfcLoop();
        return;
    }

    if (currentScreen == device::ui::Screen::InventoryShipment) {
        handleShipmentNfcLoop();
        return;
    }

    if (currentScreen == device::ui::Screen::NfcProvision) {
        handleProvisionNfcLoop();
    }
}

#include "ScreenProvision.h"
#include "AppGlobals.h"
#include "AppUtils.h"
#include "UiRender.h"
#include "Boot.h"
#include "NetworkTasks.h"
#include <Arduino.h>

void handleProvisionNfcLoop() {
    if (g_controller.provisionSnapshot().stage != device::domain::ProvisionStage::WaitingForTag) {
        return;
    }

    if (!g_nfc.pollTag(millis(), g_tagSnapshot) || !g_tagSnapshot.present) {
        return;
    }
    Serial.printf("[provision] tag detected chip=%s\n", g_tagSnapshot.uidHex.c_str());

    g_provisionSnapshot.clear();
    g_provisionSnapshot.uidHex = g_tagSnapshot.uidHex;
    g_provisionSnapshot.stage = device::domain::ProvisionStage::Writing;
    g_provisionSnapshot.inProgress = true;
    g_provisionSnapshot.addDetailLine("Tag detected");
    g_controller.updateProvisionSnapshot(g_provisionSnapshot);
    {
        auto event = makeEvent(device::domain::DeviceEventType::ProvisionStarted, device::domain::DeviceEventSeverity::Info, "nfc_provision");
        event.uidHex.assign(g_provisionSnapshot.uidHex.view());
        logEvent(event);
    }
    renderCurrentScreen();

    const bool writeOk = g_nfc.startProvision(g_provisionSnapshot);
    Serial.printf("[provision] local write result=%d uuid=%s\n", writeOk ? 1 : 0, g_provisionSnapshot.writtenUuid.c_str());
    g_controller.updateProvisionSnapshot(g_provisionSnapshot);
    renderCurrentScreen();

    if (writeOk) {
        g_nfc.finishProvision(g_provisionSnapshot);
        g_controller.updateProvisionSnapshot(g_provisionSnapshot);
    }

    if (g_provisionSnapshot.success) {
        device::domain::InventoryTagRecord inventoryRecord;
        if (!g_journal.storeProvisionedTag(
                g_provisionSnapshot.uidHex.view(),
                g_provisionSnapshot.writtenUuid.view(),
                millis(),
                g_dbStatus.bootId,
                inventoryRecord
            )) {
            Serial.printf("[provision] db store FAILED uuid=%s chip=%s\n",
                g_provisionSnapshot.writtenUuid.c_str(),
                g_provisionSnapshot.uidHex.c_str());
            g_provisionSnapshot.success = false;
            g_provisionSnapshot.stage = device::domain::ProvisionStage::Failure;
            g_provisionSnapshot.errorCode = device::domain::ErrorCode::DbWrite;
            g_provisionSnapshot.clearDetails();
            g_provisionSnapshot.addDetailLine("Tag written but DB save failed");
            g_controller.updateProvisionSnapshot(g_provisionSnapshot);
        }
        Serial.printf(
            "[provision] saved locally uuid=%s chip=%s success=%d\n",
            g_provisionSnapshot.writtenUuid.c_str(),
            g_provisionSnapshot.uidHex.c_str(),
            g_provisionSnapshot.success ? 1 : 0
        );
    }

    if (g_provisionSnapshot.success) {
        refreshSyncStatus();
        if (g_networkStatus.wifiConnected && g_networkStatus.deviceAuthenticated) {
            Serial.println("[provision] attempting immediate backend sync...");
            syncNextPendingOperation(millis(), true);
            refreshSyncStatus();
        } else {
            Serial.printf("[provision] offline — sync skipped (net=%d auth=%d)\n",
                g_networkStatus.wifiConnected ? 1 : 0,
                g_networkStatus.deviceAuthenticated ? 1 : 0);
        }

        device::domain::InventoryUnitRecord provisionRecord;
        if (g_journal.lookupInventoryUnitByTagUuid(g_provisionSnapshot.writtenUuid.view(), provisionRecord)) {
            Serial.printf("[provision] sync result pending=%d uuid=%s\n",
                provisionRecord.pendingSync ? 1 : 0,
                g_provisionSnapshot.writtenUuid.c_str());
            if (provisionRecord.pendingSync) {
                g_provisionSnapshot.addDetailLine("Queued for sync");
                if (!g_networkStatus.wifiConnected || !g_networkStatus.deviceAuthenticated) {
                    g_provisionSnapshot.addDetailLine("WiFi/API required");
                } else if (!g_syncStatus.lastError.empty()) {
                    g_provisionSnapshot.addDetailLine(g_syncStatus.lastError.c_str());
                }
            } else {
                g_provisionSnapshot.addDetailLine(
                    g_syncStatus.lastError.empty() ? "Backend synced" : g_syncStatus.lastError.c_str()
                );
            }
            g_controller.updateProvisionSnapshot(g_provisionSnapshot);
        }
    }

    if (g_provisionSnapshot.success) {
        Serial.printf("[provision] success uuid=%s chip=%s\n",
            g_provisionSnapshot.writtenUuid.c_str(),
            g_provisionSnapshot.uidHex.c_str());
        auto event = makeEvent(device::domain::DeviceEventType::ProvisionSucceeded, device::domain::DeviceEventSeverity::Info, "nfc_provision");
        event.uidHex.assign(g_provisionSnapshot.uidHex.view());
        event.tagUuid.assign(g_provisionSnapshot.writtenUuid.view());
        logEvent(event);
        blinkLed(2);
    } else {
        Serial.printf("[provision] failure code=%d detail=%s\n",
            static_cast<int>(g_provisionSnapshot.errorCode),
            g_provisionSnapshot.detailCount > 0 ? g_provisionSnapshot.detailLines[0].c_str() : "");
        auto event = makeEvent(device::domain::DeviceEventType::ProvisionFailed, device::domain::DeviceEventSeverity::Error, "nfc_provision");
        event.uidHex.assign(g_provisionSnapshot.uidHex.view());
        event.resultCode.assign(device::domain::toString(g_provisionSnapshot.errorCode));
        if (g_provisionSnapshot.detailCount > 0) {
            event.message.assign(g_provisionSnapshot.detailLines[0].view());
        }
        logEvent(event);
        signalProvisionError();
    }

    renderCurrentScreen();
}

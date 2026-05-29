#include "ScreenLookup.h"
#include "AppGlobals.h"
#include "AppUtils.h"
#include "UiRender.h"
#include "Boot.h"
#include <Arduino.h>
#include "config/DeviceConfig.h"

void fillLookupSnapshot(const device::domain::NfcTagSnapshot& tagSnapshot, device::domain::LookupSnapshot& lookupSnapshot) {
    lookupSnapshot.clear();
    lookupSnapshot.present = tagSnapshot.present;
    lookupSnapshot.payloadKind = tagSnapshot.payloadKind;
    lookupSnapshot.errorCode = tagSnapshot.errorCode;
    lookupSnapshot.uidHex = tagSnapshot.uidHex;
    lookupSnapshot.uuid = tagSnapshot.uuid;
    lookupSnapshot.legacyRaw = tagSnapshot.legacyRaw;

    if (!tagSnapshot.present) {
        return;
    }

    Serial.printf("[lookup] tag scanned chip=%s uuid=%s payload=%s\n",
        tagSnapshot.uidHex.c_str(),
        tagSnapshot.uuid.c_str(),
        device::domain::toString(tagSnapshot.payloadKind));

    lookupSnapshot.addDetailLine("Tag detected");
    char payloadLine[48] {};
    const auto payloadWritten = snprintf(payloadLine, sizeof(payloadLine), "Payload: %s", device::domain::toString(tagSnapshot.payloadKind));
    lookupSnapshot.addDetailLine(payloadWritten > 0 ? payloadLine : "Payload");

    if (tagSnapshot.payloadKind != device::domain::NfcPayloadKind::NdefUuid) {
        Serial.printf("[lookup] not UUID payload=%s — skipping DB lookup\n",
            device::domain::toString(tagSnapshot.payloadKind));
        lookupSnapshot.addDetailLine("Local lookup requires UUID tag");
        return;
    }

    Serial.printf("[lookup] db lookup uuid=%s\n", tagSnapshot.uuid.c_str());
    device::domain::InventoryUnitRecord unitRecord;
    if (!g_journal.lookupInventoryUnitByTagUuid(tagSnapshot.uuid.view(), unitRecord)) {
        Serial.println("[lookup] db lookup FAILED");
        lookupSnapshot.errorCode = device::domain::ErrorCode::DbWrite;
        lookupSnapshot.addDetailLine("Local DB lookup failed");
        return;
    }

    if (unitRecord.exists) {
        Serial.printf("[lookup] found sku=%lu qty=%lu/%lu state=%s product=%s pending=%d\n",
            static_cast<unsigned long>(unitRecord.sku),
            static_cast<unsigned long>(unitRecord.quantityCurrent),
            static_cast<unsigned long>(unitRecord.quantityInitial),
            device::domain::toString(unitRecord.state),
            unitRecord.productName.c_str(),
            unitRecord.pendingSync ? 1 : 0);
        lookupSnapshot.knownLocally = true;
        lookupSnapshot.pendingSync = unitRecord.pendingSync;
        lookupSnapshot.unitState = unitRecord.state;
        lookupSnapshot.sku = unitRecord.sku;
        lookupSnapshot.quantityInitial = unitRecord.quantityInitial;
        lookupSnapshot.quantityCurrent = unitRecord.quantityCurrent;
        lookupSnapshot.productName = unitRecord.productName;
        g_journal.touchInventoryTag(tagSnapshot.uuid.view(), tagSnapshot.uidHex.view(), millis());

        char statusLine[48] {};
        const auto statusWritten = snprintf(
            statusLine,
            sizeof(statusLine),
            "%s SKU %lu",
            device::domain::toString(unitRecord.state),
            static_cast<unsigned long>(unitRecord.sku)
        );
        lookupSnapshot.addDetailLine(statusWritten > 0 ? statusLine : "Unit found");
        char quantityLine[48] {};
        const auto quantityWritten = snprintf(
            quantityLine,
            sizeof(quantityLine),
            "Qty %lu/%lu",
            static_cast<unsigned long>(unitRecord.quantityCurrent),
            static_cast<unsigned long>(unitRecord.quantityInitial)
        );
        lookupSnapshot.addDetailLine(quantityWritten > 0 ? quantityLine : "Qty found");
        if (!unitRecord.productName.empty()) {
            lookupSnapshot.addDetailLine(unitRecord.productName.c_str());
        }
        if (unitRecord.pendingSync) {
            lookupSnapshot.addDetailLine("Pending sync");
        }
        return;
    }

    if (unitRecord.provisionedOnly) {
        Serial.printf("[lookup] provisioned only uuid=%s pending=%d\n",
            tagSnapshot.uuid.c_str(), unitRecord.pendingSync ? 1 : 0);
        lookupSnapshot.knownLocally = true;
        lookupSnapshot.provisionedOnly = true;
        lookupSnapshot.pendingSync = unitRecord.pendingSync;
        lookupSnapshot.addDetailLine("Provisioned only");
        lookupSnapshot.addDetailLine("Not received yet");
        return;
    }

    Serial.printf("[lookup] unknown tag chip=%s uuid=%s\n",
        tagSnapshot.uidHex.c_str(), tagSnapshot.uuid.c_str());
    lookupSnapshot.addDetailLine("Unknown tag");
}

void handleLookupNfcLoop() {
    if (!g_lookupFrozen && g_nfc.pollTag(millis(), g_tagSnapshot)) {
        if (g_tagSnapshot.present) {
            Serial.printf("[lookup] tag polled chip=%s uuid=%s present=1\n",
                g_tagSnapshot.uidHex.c_str(), g_tagSnapshot.uuid.c_str());
        }
        fillLookupSnapshot(g_tagSnapshot, g_lookupSnapshot);
        g_controller.updateLookupSnapshot(g_lookupSnapshot);
        if (g_tagSnapshot.present) {
            auto event = makeEvent(device::domain::DeviceEventType::TagRead, device::domain::DeviceEventSeverity::Info, "lookup");
            event.uidHex.assign(g_tagSnapshot.uidHex.view());
            event.payloadKind.assign(device::domain::toString(g_tagSnapshot.payloadKind));
            if (!g_tagSnapshot.uuid.empty()) {
                event.tagUuid.assign(g_tagSnapshot.uuid.view());
            }
            if (g_tagSnapshot.errorCode != device::domain::ErrorCode::None) {
                event.resultCode.assign(device::domain::toString(g_tagSnapshot.errorCode));
            }
            logEvent(event);
            if (g_tagSnapshot.payloadKind == device::domain::NfcPayloadKind::NdefUuid) {
                blinkLed(1);
            }
            g_lookupFrozen = true;
            renderCurrentScreen();
        }
    }
}

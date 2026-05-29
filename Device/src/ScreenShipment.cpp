#include "ScreenShipment.h"
#include "AppGlobals.h"
#include "AppUtils.h"
#include "UiRender.h"
#include "Boot.h"
#include "NetworkTasks.h"
#include "ScreenReceipt.h"
#include <Arduino.h>
#include "config/DeviceConfig.h"

void adjustShipmentQuantityDigit(int delta) {
    const uint32_t factor = quantityDigitFactor(g_shipmentSnapshot.quantityCursor);
    const uint32_t digit = (g_shipmentSnapshot.quantityToShip / factor) % 10U;
    const uint32_t nextDigit = static_cast<uint32_t>((static_cast<int>(digit) + delta + 10) % 10);
    g_shipmentSnapshot.quantityToShip -= digit * factor;
    g_shipmentSnapshot.quantityToShip += nextDigit * factor;
}

void moveShipmentQuantityCursor(int delta) {
    static constexpr int kSelectableItems = static_cast<int>(device::config::kQuantityDigits + 1);
    const int current = static_cast<int>(g_shipmentSnapshot.quantityCursor);
    g_shipmentSnapshot.quantityCursor = static_cast<uint32_t>((current + delta + kSelectableItems) % kSelectableItems);
}

void setShipmentFailure(const char* message, device::domain::ErrorCode code) {
    Serial.printf("[shipment] FAILURE reason=\"%s\" code=%d\n", message, static_cast<int>(code));
    g_shipmentSnapshot.inProgress = false;
    g_shipmentSnapshot.success = false;
    g_shipmentSnapshot.stage = device::domain::ShipmentStage::Failure;
    g_shipmentSnapshot.errorCode = code;
    g_shipmentSnapshot.clearDetails();
    g_shipmentSnapshot.addDetailLine(message);
    g_controller.updateShipmentSnapshot(g_shipmentSnapshot);
}

void initializeShipmentScreen() {
    Serial.println("[shipment] init — waiting for tag scan");
    g_shipmentSnapshot.clear();
    g_shipmentSnapshot.stage = device::domain::ShipmentStage::WaitingForTag;
    g_shipmentSnapshot.quantityCurrent = 0;
    g_shipmentSnapshot.quantityToShip = 0;
    g_shipmentSnapshot.quantityCursor = 0;
    g_shipmentSnapshot.quantityEditing = false;
    g_shipmentSnapshot.addDetailLine("Scan active tag");
    g_controller.updateShipmentSnapshot(g_shipmentSnapshot);
}

bool handleShipmentButtonEvent(const device::domain::ButtonEvent& event) {
    if (event.type != device::domain::ButtonPressType::ShortPress) {
        return false;
    }

    if (g_shipmentSnapshot.stage == device::domain::ShipmentStage::Success ||
        g_shipmentSnapshot.stage == device::domain::ShipmentStage::Failure) {
        if (event.button == device::config::Button::Ok) {
            initializeShipmentScreen();
        }
        return true;
    }

    if (g_shipmentSnapshot.stage != device::domain::ShipmentStage::QuantityInput) {
        return false;
    }

    if (event.button == device::config::Button::Up) {
        if (g_shipmentSnapshot.quantityEditing && g_shipmentSnapshot.quantityCursor < device::config::kQuantityDigits) {
            adjustShipmentQuantityDigit(1);
        } else {
            moveShipmentQuantityCursor(-1);
        }
    } else if (event.button == device::config::Button::Down) {
        if (g_shipmentSnapshot.quantityEditing && g_shipmentSnapshot.quantityCursor < device::config::kQuantityDigits) {
            adjustShipmentQuantityDigit(-1);
        } else {
            moveShipmentQuantityCursor(1);
        }
    } else if (event.button == device::config::Button::Ok) {
        if (g_shipmentSnapshot.quantityCursor >= device::config::kQuantityDigits) {
            if (g_shipmentSnapshot.quantityToShip < device::config::kMinTagQuantity ||
                g_shipmentSnapshot.quantityToShip > g_shipmentSnapshot.quantityCurrent) {
                Serial.printf("[shipment] qty invalid — to_ship=%lu remain=%lu\n",
                    static_cast<unsigned long>(g_shipmentSnapshot.quantityToShip),
                    static_cast<unsigned long>(g_shipmentSnapshot.quantityCurrent));
                setShipmentFailure("Qty exceeds remaining");
                return true;
            }

            Serial.printf("[shipment] confirm qty=%lu/%lu sku=%lu product=%s uuid=%s\n",
                static_cast<unsigned long>(g_shipmentSnapshot.quantityToShip),
                static_cast<unsigned long>(g_shipmentSnapshot.quantityCurrent),
                static_cast<unsigned long>(g_shipmentSnapshot.sku),
                g_shipmentSnapshot.productName.c_str(),
                g_shipmentSnapshot.tagUuid.c_str());
            g_shipmentSnapshot.quantityEditing = false;
            device::domain::InventoryUnitRecord unitRecord;
            if (!g_journal.shipInventoryUnit(
                    g_shipmentSnapshot.uidHex.view(),
                    g_shipmentSnapshot.tagUuid.view(),
                    g_shipmentSnapshot.quantityToShip,
                    millis(),
                    g_dbStatus.bootId,
                    unitRecord
                )) {
                setShipmentFailure("Local shipment failed", device::domain::ErrorCode::DbWrite);
                signalProvisionError();
                return true;
            }
            Serial.printf(
                "[shipment] saved locally uuid=%s shipped=%lu left=%lu pending=%d\n",
                g_shipmentSnapshot.tagUuid.c_str(),
                static_cast<unsigned long>(g_shipmentSnapshot.quantityToShip),
                static_cast<unsigned long>(unitRecord.quantityCurrent),
                unitRecord.pendingSync ? 1 : 0
            );

            g_shipmentSnapshot.inProgress = false;
            g_shipmentSnapshot.success = true;
            g_shipmentSnapshot.pendingSync = unitRecord.pendingSync;
            g_shipmentSnapshot.stage = device::domain::ShipmentStage::Success;
            g_shipmentSnapshot.errorCode = device::domain::ErrorCode::None;
            g_shipmentSnapshot.sku = unitRecord.sku;
            g_shipmentSnapshot.productName = unitRecord.productName;
            g_shipmentSnapshot.quantityCurrent = unitRecord.quantityCurrent;
            g_shipmentSnapshot.clearDetails();
            g_shipmentSnapshot.addDetailLine("Saved locally");
            char quantityLine[48] {};
            const auto quantityWritten = snprintf(
                quantityLine,
                sizeof(quantityLine),
                "Shipped %lu, left %lu",
                static_cast<unsigned long>(g_shipmentSnapshot.quantityToShip),
                static_cast<unsigned long>(unitRecord.quantityCurrent)
            );
            g_shipmentSnapshot.addDetailLine(quantityWritten > 0 ? quantityLine : "Shipment saved");
            if (unitRecord.pendingSync) {
                g_shipmentSnapshot.addDetailLine("Pending sync");
            }
            g_controller.updateShipmentSnapshot(g_shipmentSnapshot);
            refreshSyncStatus();
            if (g_networkStatus.wifiConnected && g_networkStatus.deviceAuthenticated) {
                Serial.println("[shipment] attempting immediate backend sync...");
                if (syncNextPendingOperation(millis(), true)) {
                    refreshSyncStatus();
                    if (g_journal.lookupInventoryUnitByTagUuid(g_shipmentSnapshot.tagUuid.view(), unitRecord)) {
                        g_shipmentSnapshot.pendingSync = unitRecord.pendingSync;
                        g_shipmentSnapshot.quantityCurrent = unitRecord.quantityCurrent;
                    }
                    Serial.printf("[shipment] sync result pending=%d left=%lu\n",
                        g_shipmentSnapshot.pendingSync ? 1 : 0,
                        static_cast<unsigned long>(g_shipmentSnapshot.quantityCurrent));
                    g_shipmentSnapshot.clearDetails();
                    g_shipmentSnapshot.addDetailLine(g_shipmentSnapshot.pendingSync ? "Saved locally" : "Synced");
                    g_shipmentSnapshot.addDetailLine(quantityWritten > 0 ? quantityLine : "Shipment saved");
                    if (g_shipmentSnapshot.pendingSync) {
                        g_shipmentSnapshot.addDetailLine("Pending sync");
                    }
                    g_controller.updateShipmentSnapshot(g_shipmentSnapshot);
                }
            } else {
                Serial.printf("[shipment] offline — sync skipped (net=%d auth=%d)\n",
                    g_networkStatus.wifiConnected ? 1 : 0, g_networkStatus.deviceAuthenticated ? 1 : 0);
            }
            blinkLed(4);
            g_controller.updateShipmentSnapshot(g_shipmentSnapshot);
            return true;
        } else if (!g_shipmentSnapshot.quantityEditing) {
            g_shipmentSnapshot.quantityEditing = true;
        } else {
            g_shipmentSnapshot.quantityEditing = false;
            if (g_shipmentSnapshot.quantityCursor + 1 < device::config::kQuantityDigits + 1) {
                ++g_shipmentSnapshot.quantityCursor;
            }
        }
    }

    g_controller.updateShipmentSnapshot(g_shipmentSnapshot);
    return true;
}

void handleShipmentNfcLoop() {
    if (g_controller.shipmentSnapshot().stage != device::domain::ShipmentStage::WaitingForTag) {
        return;
    }

    if (!g_nfc.pollTag(millis(), g_tagSnapshot) || !g_tagSnapshot.present) {
        return;
    }
    Serial.printf("[shipment] tag detected uuid=%s chip=%s\n", g_tagSnapshot.uuid.c_str(), g_tagSnapshot.uidHex.c_str());

    g_shipmentSnapshot.inProgress = true;
    g_shipmentSnapshot.uidHex = g_tagSnapshot.uidHex;
    g_shipmentSnapshot.tagUuid = g_tagSnapshot.uuid;
    g_controller.updateShipmentSnapshot(g_shipmentSnapshot);
    renderCurrentScreen();

    if (g_tagSnapshot.payloadKind != device::domain::NfcPayloadKind::NdefUuid) {
        setShipmentFailure("Shipment needs UUID tag");
        signalProvisionError();
        renderCurrentScreen();
        return;
    }

    device::domain::InventoryUnitRecord unitRecord;
    if (!g_journal.lookupInventoryUnitByTagUuid(g_tagSnapshot.uuid.view(), unitRecord)) {
        setShipmentFailure("Local lookup failed", device::domain::ErrorCode::DbWrite);
        signalProvisionError();
        renderCurrentScreen();
        return;
    }

    if (!unitRecord.exists) {
        setShipmentFailure(unitRecord.provisionedOnly ? "Tag not received yet" : "Unknown tag");
        signalProvisionError();
        renderCurrentScreen();
        return;
    }

    if (unitRecord.state == device::domain::InventoryUnitState::Shipped) {
        setShipmentFailure("Tag already shipped");
        signalProvisionError();
        renderCurrentScreen();
        return;
    }

    g_shipmentSnapshot.sku = unitRecord.sku;
    g_shipmentSnapshot.productName = unitRecord.productName;
    g_shipmentSnapshot.quantityCurrent = unitRecord.quantityCurrent;
    g_shipmentSnapshot.quantityToShip = unitRecord.quantityCurrent;
    g_shipmentSnapshot.quantityCursor = 0;
    g_shipmentSnapshot.quantityEditing = false;
    g_shipmentSnapshot.stage = device::domain::ShipmentStage::QuantityInput;
    g_shipmentSnapshot.errorCode = device::domain::ErrorCode::None;
    g_shipmentSnapshot.clearDetails();
    g_shipmentSnapshot.addDetailLine("Set ship quantity");
    char quantityLine[48] {};
    const auto quantityWritten = snprintf(
        quantityLine,
        sizeof(quantityLine),
        "Remain %lu",
        static_cast<unsigned long>(unitRecord.quantityCurrent)
    );
    g_shipmentSnapshot.addDetailLine(quantityWritten > 0 ? quantityLine : "Remain set");
    if (!unitRecord.productName.empty()) {
        g_shipmentSnapshot.addDetailLine(unitRecord.productName.c_str());
    }
    g_controller.updateShipmentSnapshot(g_shipmentSnapshot);
    renderCurrentScreen();
}

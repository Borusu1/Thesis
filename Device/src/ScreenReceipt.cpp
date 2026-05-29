#include "ScreenReceipt.h"
#include "AppGlobals.h"
#include "AppUtils.h"
#include "UiRender.h"
#include "Boot.h"
#include "NetworkTasks.h"
#include <Arduino.h>
#include "config/DeviceConfig.h"

uint32_t skuDigitFactor(std::size_t cursor) {
    uint32_t factor = 1;
    for (std::size_t index = cursor + 1; index < device::config::kSkuDigits; ++index) {
        factor *= 10U;
    }
    return factor;
}

uint32_t quantityDigitFactor(std::size_t cursor) {
    uint32_t factor = 1;
    for (std::size_t index = cursor + 1; index < device::config::kQuantityDigits; ++index) {
        factor *= 10U;
    }
    return factor;
}

void adjustReceiptSkuDigit(int delta) {
    const uint32_t factor = skuDigitFactor(g_receiptSnapshot.skuCursor);
    const uint32_t digit = (g_receiptSnapshot.enteredSku / factor) % 10U;
    const uint32_t nextDigit = static_cast<uint32_t>((static_cast<int>(digit) + delta + 10) % 10);
    g_receiptSnapshot.enteredSku -= digit * factor;
    g_receiptSnapshot.enteredSku += nextDigit * factor;
}

void adjustReceiptQuantityDigit(int delta) {
    const uint32_t factor = quantityDigitFactor(g_receiptSnapshot.quantityCursor);
    const uint32_t digit = (g_receiptSnapshot.enteredQuantity / factor) % 10U;
    const uint32_t nextDigit = static_cast<uint32_t>((static_cast<int>(digit) + delta + 10) % 10);
    g_receiptSnapshot.enteredQuantity -= digit * factor;
    g_receiptSnapshot.enteredQuantity += nextDigit * factor;
}

void moveReceiptQuantityCursor(int delta) {
    static constexpr int kSelectableItems = static_cast<int>(device::config::kQuantityDigits + 1);
    const int current = static_cast<int>(g_receiptSnapshot.quantityCursor);
    g_receiptSnapshot.quantityCursor = static_cast<uint32_t>((current + delta + kSelectableItems) % kSelectableItems);
}

bool isValidTagQuantity(uint32_t quantity) {
    return quantity >= device::config::kMinTagQuantity && quantity <= device::config::kMaxTagQuantity;
}

void setReceiptTagWaitingState() {
    g_receiptSnapshot.stage = device::domain::ReceiptStage::WaitingForTag;
    g_receiptSnapshot.quantityEditing = false;
    g_receiptSnapshot.errorCode = device::domain::ErrorCode::None;
    g_receiptSnapshot.clearDetails();
    g_receiptSnapshot.addDetailLine("Present provisioned tag");
    char buffer[48] {};
    const auto written = snprintf(
        buffer,
        sizeof(buffer),
        "SKU %lu Qty %lu",
        static_cast<unsigned long>(g_receiptSnapshot.selectedProduct.sku),
        static_cast<unsigned long>(g_receiptSnapshot.enteredQuantity)
    );
    g_receiptSnapshot.addDetailLine(written > 0 ? buffer : "Product ready");
}

void setReceiptFailure(const char* message, device::domain::ErrorCode code) {
    Serial.printf("[receipt] FAILURE reason=\"%s\" code=%d\n", message, static_cast<int>(code));
    g_receiptSnapshot.inProgress = false;
    g_receiptSnapshot.success = false;
    g_receiptSnapshot.stage = device::domain::ReceiptStage::Failure;
    g_receiptSnapshot.errorCode = code;
    g_receiptSnapshot.clearDetails();
    g_receiptSnapshot.addDetailLine(message);
    g_controller.updateReceiptSnapshot(g_receiptSnapshot);
}

bool refreshProvisionState(std::string_view tagUuid, device::domain::InventoryUnitRecord& unitRecord) {
    return g_journal.lookupInventoryUnitByTagUuid(tagUuid, unitRecord);
}

bool forceProvisionSyncIfPossible(std::string_view tagUuid, device::domain::InventoryUnitRecord& unitRecord) {
    if (!unitRecord.provisionedOnly || !unitRecord.pendingSync) {
        return true;
    }

    if (!g_networkStatus.wifiConnected || !g_networkStatus.deviceAuthenticated) {
        return true;
    }

    syncNextPendingOperation(millis(), true);
    refreshSyncStatus();
    return refreshProvisionState(tagUuid, unitRecord);
}

void initializeReceiptSelection() {
    g_receiptSnapshot.clear();
    g_receiptSnapshot.productsLoaded = g_catalogStatus.loaded;
    g_receiptSnapshot.stage = device::domain::ReceiptStage::SelectProduct;
    g_receiptSnapshot.enteredQuantity = 0;
    g_receiptSnapshot.quantityCursor = 0;
    g_receiptSnapshot.quantityEditing = false;
    if (g_catalogStatus.loaded) {
        std::size_t count = 0;
        g_journal.listProducts(g_receiptSnapshot.products, count);
        g_receiptSnapshot.productCount = count;
        if (count > 0) {
            g_receiptSnapshot.selectedIndex = 0;
            g_receiptSnapshot.addDetailLine("Select product or SKU");
            char buffer[48] {};
            const auto written = snprintf(buffer, sizeof(buffer), "Products: %lu", static_cast<unsigned long>(count));
            g_receiptSnapshot.addDetailLine(written > 0 ? buffer : "Products loaded");
            Serial.printf("[receipt] init — catalog loaded products=%lu\n", static_cast<unsigned long>(count));
        } else {
            g_receiptSnapshot.addDetailLine("No products loaded");
            Serial.println("[receipt] init — catalog empty, no products");
        }
    } else {
        g_receiptSnapshot.addDetailLine("No products loaded");
        Serial.println("[receipt] init — catalog not loaded");
    }
    g_controller.updateReceiptSnapshot(g_receiptSnapshot);
}

bool handleReceiptButtonEvent(const device::domain::ButtonEvent& event) {
    if (event.type != device::domain::ButtonPressType::ShortPress) {
        return false;
    }

    if (g_receiptSnapshot.stage == device::domain::ReceiptStage::Failure ||
        g_receiptSnapshot.stage == device::domain::ReceiptStage::Success) {
        if (event.button == device::config::Button::Ok) {
            initializeReceiptSelection();
            return true;
        }
        return true;
    }

    if (g_receiptSnapshot.stage == device::domain::ReceiptStage::WaitingForTag) {
        if (event.button == device::config::Button::Ok) {
            initializeReceiptSelection();
        }
        return true;
    }

    if (g_receiptSnapshot.stage == device::domain::ReceiptStage::SkuInput) {
        if (event.button == device::config::Button::Up) {
            adjustReceiptSkuDigit(1);
        } else if (event.button == device::config::Button::Down) {
            adjustReceiptSkuDigit(-1);
        } else if (event.button == device::config::Button::Ok) {
            if (g_receiptSnapshot.skuCursor + 1 < device::config::kSkuDigits) {
                ++g_receiptSnapshot.skuCursor;
            } else {
                Serial.printf("[receipt] sku lookup sku=%lu\n",
                    static_cast<unsigned long>(g_receiptSnapshot.enteredSku));
                device::domain::ProductRecord product;
                if (!g_journal.findProductBySku(g_receiptSnapshot.enteredSku, product) || !product.exists || !product.isActive) {
                    Serial.printf("[receipt] sku not found sku=%lu\n",
                        static_cast<unsigned long>(g_receiptSnapshot.enteredSku));
                    setReceiptFailure("SKU not found");
                    return true;
                }
                Serial.printf("[receipt] sku ok sku=%lu name=%s\n",
                    static_cast<unsigned long>(product.sku), product.name.c_str());
                g_receiptSnapshot.skuMode = false;
                g_receiptSnapshot.selectedProduct = product;
                g_receiptSnapshot.stage = device::domain::ReceiptStage::QuantityInput;
                g_receiptSnapshot.enteredQuantity = 0;
                g_receiptSnapshot.quantityCursor = 0;
                g_receiptSnapshot.quantityEditing = false;
                g_receiptSnapshot.clearDetails();
                g_receiptSnapshot.addDetailLine("Set quantity 1-99999");
                char buffer[48] {};
                const auto written = snprintf(buffer, sizeof(buffer), "SKU %lu %s", static_cast<unsigned long>(product.sku), product.name.c_str());
                g_receiptSnapshot.addDetailLine(written > 0 ? buffer : "Product selected");
            }
        }
        g_controller.updateReceiptSnapshot(g_receiptSnapshot);
        return true;
    }

    if (g_receiptSnapshot.stage == device::domain::ReceiptStage::QuantityInput) {
        if (event.button == device::config::Button::Up) {
            if (g_receiptSnapshot.quantityEditing && g_receiptSnapshot.quantityCursor < device::config::kQuantityDigits) {
                adjustReceiptQuantityDigit(1);
            } else {
                moveReceiptQuantityCursor(-1);
            }
        } else if (event.button == device::config::Button::Down) {
            if (g_receiptSnapshot.quantityEditing && g_receiptSnapshot.quantityCursor < device::config::kQuantityDigits) {
                adjustReceiptQuantityDigit(-1);
            } else {
                moveReceiptQuantityCursor(1);
            }
        } else if (event.button == device::config::Button::Ok) {
            if (g_receiptSnapshot.quantityCursor >= device::config::kQuantityDigits) {
                if (!isValidTagQuantity(g_receiptSnapshot.enteredQuantity)) {
                    Serial.printf("[receipt] qty invalid qty=%lu (must be 1-99999)\n",
                        static_cast<unsigned long>(g_receiptSnapshot.enteredQuantity));
                    setReceiptFailure("Qty must be 1-99999");
                    return true;
                }
                Serial.printf("[receipt] qty confirmed qty=%lu sku=%lu product=%s — waiting for tag\n",
                    static_cast<unsigned long>(g_receiptSnapshot.enteredQuantity),
                    static_cast<unsigned long>(g_receiptSnapshot.selectedProduct.sku),
                    g_receiptSnapshot.selectedProduct.name.c_str());
                g_receiptSnapshot.quantityEditing = false;
                setReceiptTagWaitingState();
            } else if (!g_receiptSnapshot.quantityEditing) {
                g_receiptSnapshot.quantityEditing = true;
            } else {
                g_receiptSnapshot.quantityEditing = false;
                if (g_receiptSnapshot.quantityCursor + 1 < device::config::kQuantityDigits + 1) {
                    ++g_receiptSnapshot.quantityCursor;
                }
            }
        }
        g_controller.updateReceiptSnapshot(g_receiptSnapshot);
        return true;
    }

    if (!g_receiptSnapshot.productsLoaded || g_receiptSnapshot.productCount == 0) {
        if (event.button == device::config::Button::Ok) {
            setReceiptFailure("No products loaded");
        }
        return true;
    }

    const std::size_t maxIndex = g_receiptSnapshot.productCount;
    if (event.button == device::config::Button::Up) {
        if (g_receiptSnapshot.selectedIndex == 0) {
            g_receiptSnapshot.selectedIndex = maxIndex;
        } else {
            --g_receiptSnapshot.selectedIndex;
        }
    } else if (event.button == device::config::Button::Down) {
        g_receiptSnapshot.selectedIndex = (g_receiptSnapshot.selectedIndex + 1) % (maxIndex + 1);
    } else if (event.button == device::config::Button::Ok) {
        if (g_receiptSnapshot.selectedIndex == maxIndex) {
            Serial.println("[receipt] entering manual SKU input");
            g_receiptSnapshot.skuMode = true;
            g_receiptSnapshot.stage = device::domain::ReceiptStage::SkuInput;
            g_receiptSnapshot.enteredSku = 0;
            g_receiptSnapshot.skuCursor = 0;
            g_receiptSnapshot.selectedProduct.clear();
            g_receiptSnapshot.clearDetails();
            g_receiptSnapshot.addDetailLine("Enter 4-digit SKU");
        } else {
            g_receiptSnapshot.selectedProduct = g_receiptSnapshot.products[g_receiptSnapshot.selectedIndex];
            Serial.printf("[receipt] product selected sku=%lu name=%s\n",
                static_cast<unsigned long>(g_receiptSnapshot.selectedProduct.sku),
                g_receiptSnapshot.selectedProduct.name.c_str());
            g_receiptSnapshot.skuMode = false;
            g_receiptSnapshot.stage = device::domain::ReceiptStage::QuantityInput;
            g_receiptSnapshot.enteredQuantity = 0;
            g_receiptSnapshot.quantityCursor = 0;
            g_receiptSnapshot.quantityEditing = false;
            g_receiptSnapshot.clearDetails();
            g_receiptSnapshot.addDetailLine("Set quantity 1-99999");
            char buffer[48] {};
            const auto written = snprintf(
                buffer,
                sizeof(buffer),
                "SKU %lu %s",
                static_cast<unsigned long>(g_receiptSnapshot.selectedProduct.sku),
                g_receiptSnapshot.selectedProduct.name.c_str()
            );
            g_receiptSnapshot.addDetailLine(written > 0 ? buffer : "Product selected");
        }
    }

    g_controller.updateReceiptSnapshot(g_receiptSnapshot);
    return true;
}

void handleReceiptNfcLoop() {
    if (g_controller.receiptSnapshot().stage != device::domain::ReceiptStage::WaitingForTag) {
        return;
    }

    if (!g_nfc.pollTag(millis(), g_tagSnapshot) || !g_tagSnapshot.present) {
        return;
    }
    Serial.printf(
        "[receipt] tag detected uuid=%s chip=%s qty=%lu sku=%lu\n",
        g_tagSnapshot.uuid.c_str(),
        g_tagSnapshot.uidHex.c_str(),
        static_cast<unsigned long>(g_receiptSnapshot.enteredQuantity),
        static_cast<unsigned long>(g_receiptSnapshot.selectedProduct.sku)
    );

    g_receiptSnapshot.inProgress = true;
    g_receiptSnapshot.uidHex = g_tagSnapshot.uidHex;
    g_receiptSnapshot.tagUuid = g_tagSnapshot.uuid;
    g_controller.updateReceiptSnapshot(g_receiptSnapshot);
    renderCurrentScreen();

    if (g_tagSnapshot.payloadKind != device::domain::NfcPayloadKind::NdefUuid) {
        Serial.printf("[receipt] tag rejected — payload=%s (need NdefUuid)\n",
            device::domain::toString(g_tagSnapshot.payloadKind));
        setReceiptFailure("Receipt needs UUID tag");
        signalProvisionError();
        renderCurrentScreen();
        return;
    }

    Serial.printf("[receipt] tag uuid lookup uuid=%s\n", g_tagSnapshot.uuid.c_str());
    device::domain::InventoryUnitRecord unitRecord;
    if (!g_journal.lookupInventoryUnitByTagUuid(g_tagSnapshot.uuid.view(), unitRecord)) {
        Serial.println("[receipt] tag db lookup FAILED");
        setReceiptFailure("Local lookup failed", device::domain::ErrorCode::DbWrite);
        signalProvisionError();
        renderCurrentScreen();
        return;
    }

    if (unitRecord.exists) {
        Serial.printf("[receipt] tag rejected — already exists state=%s\n",
            device::domain::toString(unitRecord.state));
        setReceiptFailure(unitRecord.state == device::domain::InventoryUnitState::Active ? "Tag already active" : "Tag already shipped");
        signalProvisionError();
        renderCurrentScreen();
        return;
    }

    if (!unitRecord.provisionedOnly) {
        Serial.println("[receipt] tag rejected — not provisioned, use Provision first");
        setReceiptFailure("Use Provision first");
        signalProvisionError();
        renderCurrentScreen();
        return;
    }

    if (unitRecord.pendingSync) {
        Serial.printf("[receipt] provision pending sync uuid=%s — forcing sync\n", g_tagSnapshot.uuid.c_str());
        if (!forceProvisionSyncIfPossible(g_tagSnapshot.uuid.view(), unitRecord)) {
            Serial.println("[receipt] force sync lookup FAILED");
            setReceiptFailure("Provision sync lookup failed", device::domain::ErrorCode::DbWrite);
            signalProvisionError();
            renderCurrentScreen();
            return;
        }
        if (unitRecord.pendingSync) {
            Serial.printf("[receipt] provision still pending after sync attempt — net=%d auth=%d\n",
                g_networkStatus.wifiConnected ? 1 : 0, g_networkStatus.deviceAuthenticated ? 1 : 0);
            setReceiptFailure("Provision sync required");
            if (!g_networkStatus.wifiConnected || !g_networkStatus.deviceAuthenticated) {
                g_receiptSnapshot.addDetailLine("WiFi/API required");
            } else if (!g_syncStatus.lastError.empty()) {
                g_receiptSnapshot.addDetailLine(g_syncStatus.lastError.c_str());
            }
            g_controller.updateReceiptSnapshot(g_receiptSnapshot);
            signalProvisionError();
            renderCurrentScreen();
            return;
        }
        Serial.println("[receipt] provision sync resolved — proceeding with receipt");
    }

    if (!g_journal.createInventoryUnit(
            g_tagSnapshot.uidHex.view(),
            g_tagSnapshot.uuid.view(),
            g_receiptSnapshot.selectedProduct.productId,
            g_receiptSnapshot.selectedProduct.sku,
            g_receiptSnapshot.enteredQuantity,
            millis(),
            g_dbStatus.bootId,
            unitRecord
        )) {
        setReceiptFailure("Local receipt failed", device::domain::ErrorCode::DbWrite);
        signalProvisionError();
        renderCurrentScreen();
        return;
    }
    Serial.printf(
        "[receipt] saved locally uuid=%s qty=%lu pending=%d\n",
        g_receiptSnapshot.tagUuid.c_str(),
        static_cast<unsigned long>(unitRecord.quantityCurrent),
        unitRecord.pendingSync ? 1 : 0
    );

    g_receiptSnapshot.inProgress = false;
    g_receiptSnapshot.success = true;
    g_receiptSnapshot.pendingSync = unitRecord.pendingSync;
    g_receiptSnapshot.stage = device::domain::ReceiptStage::Success;
    g_receiptSnapshot.errorCode = device::domain::ErrorCode::None;
    g_receiptSnapshot.clearDetails();
    g_receiptSnapshot.addDetailLine("Saved locally");
    char receiptLine[48] {};
    const auto receiptWritten = snprintf(
        receiptLine,
        sizeof(receiptLine),
        "SKU %lu Qty %lu",
        static_cast<unsigned long>(unitRecord.sku),
        static_cast<unsigned long>(unitRecord.quantityCurrent)
    );
    g_receiptSnapshot.addDetailLine(receiptWritten > 0 ? receiptLine : "Lot active");
    if (!unitRecord.productName.empty()) {
        g_receiptSnapshot.addDetailLine(unitRecord.productName.c_str());
    }
    if (unitRecord.pendingSync) {
        g_receiptSnapshot.addDetailLine("Pending sync");
    }
    g_controller.updateReceiptSnapshot(g_receiptSnapshot);

    refreshSyncStatus();
    if (g_networkStatus.wifiConnected && g_networkStatus.deviceAuthenticated) {
        Serial.println("[receipt] attempting immediate backend sync...");
        if (syncNextPendingOperation(millis(), true)) {
            refreshSyncStatus();
            if (g_journal.lookupInventoryUnitByTagUuid(g_receiptSnapshot.tagUuid.view(), unitRecord)) {
                g_receiptSnapshot.pendingSync = unitRecord.pendingSync;
            } else {
                g_receiptSnapshot.pendingSync = g_syncStatus.pendingCount != 0;
            }
            Serial.printf("[receipt] sync result pending=%d\n", g_receiptSnapshot.pendingSync ? 1 : 0);
            g_receiptSnapshot.clearDetails();
            g_receiptSnapshot.addDetailLine(g_receiptSnapshot.pendingSync ? "Saved locally" : "Synced");
            g_receiptSnapshot.addDetailLine(receiptWritten > 0 ? receiptLine : "Lot active");
            if (!unitRecord.productName.empty()) {
                g_receiptSnapshot.addDetailLine(unitRecord.productName.c_str());
            }
            if (g_receiptSnapshot.pendingSync) {
                g_receiptSnapshot.addDetailLine("Pending sync");
            }
            g_controller.updateReceiptSnapshot(g_receiptSnapshot);
        }
    } else {
        Serial.printf("[receipt] offline — sync skipped (net=%d auth=%d)\n",
            g_networkStatus.wifiConnected ? 1 : 0, g_networkStatus.deviceAuthenticated ? 1 : 0);
    }

    Serial.printf("[receipt] done sku=%lu qty=%lu product=%s pending=%d\n",
        static_cast<unsigned long>(unitRecord.sku),
        static_cast<unsigned long>(unitRecord.quantityCurrent),
        unitRecord.productName.c_str(),
        g_receiptSnapshot.pendingSync ? 1 : 0);
    blinkLed(3);
    renderCurrentScreen();
}

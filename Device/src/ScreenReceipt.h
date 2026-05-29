#pragma once

#include "domain/Models.h"
#include "domain/ButtonInterpreter.h"

// Digit/cursor helpers shared with shipment
uint32_t skuDigitFactor(std::size_t cursor);
uint32_t quantityDigitFactor(std::size_t cursor);

// Receipt screen functions
void adjustReceiptSkuDigit(int delta);
void adjustReceiptQuantityDigit(int delta);
void moveReceiptQuantityCursor(int delta);
bool isValidTagQuantity(uint32_t quantity);
void setReceiptTagWaitingState();
void setReceiptFailure(const char* message, device::domain::ErrorCode code = device::domain::ErrorCode::Validation);

// Provision helpers used by both receipt and provision screens
bool refreshProvisionState(std::string_view tagUuid, device::domain::InventoryUnitRecord& unitRecord);
bool forceProvisionSyncIfPossible(std::string_view tagUuid, device::domain::InventoryUnitRecord& unitRecord);

void initializeReceiptSelection();
bool handleReceiptButtonEvent(const device::domain::ButtonEvent& event);
void handleReceiptNfcLoop();

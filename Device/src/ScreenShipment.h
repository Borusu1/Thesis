#pragma once

#include "domain/Models.h"
#include "domain/ButtonInterpreter.h"

void adjustShipmentQuantityDigit(int delta);
void moveShipmentQuantityCursor(int delta);
void setShipmentFailure(const char* message, device::domain::ErrorCode code = device::domain::ErrorCode::Validation);
void initializeShipmentScreen();
bool handleShipmentButtonEvent(const device::domain::ButtonEvent& event);
void handleShipmentNfcLoop();

#pragma once

#include <cstdint>
#include "domain/Models.h"

void setupBoard();
void blinkLed(uint8_t times, uint16_t onMs = 90, uint16_t offMs = 90);
void signalProvisionError();
device::domain::DiagnosticsSnapshot runDiagnostics(bool displayOk);

#pragma once

#include <cstdint>

void startNetworkIfConfigured();
bool syncProductsFromBackend(uint32_t nowMs, bool force);
bool runNetworkMaintenance(uint32_t nowMs);
bool syncNextPendingOperation(uint32_t nowMs, bool force);

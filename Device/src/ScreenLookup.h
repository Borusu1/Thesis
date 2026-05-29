#pragma once

#include "domain/Models.h"

void fillLookupSnapshot(const device::domain::NfcTagSnapshot& tagSnapshot, device::domain::LookupSnapshot& lookupSnapshot);
void handleLookupNfcLoop();

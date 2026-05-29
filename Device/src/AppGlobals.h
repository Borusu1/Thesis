#pragma once

#include <Arduino.h>

#include "app/AppController.h"
#include "config/DeviceConfig.h"
#include "drivers/ButtonDriver.h"
#include "drivers/BootstrapConfigStore.h"
#include "drivers/DeviceApiClient.h"
#include "drivers/DisplayDriver.h"
#include "drivers/EventJournalStore.h"
#include "drivers/NfcDriver.h"
#include "drivers/NetworkManager.h"
#include "drivers/SdStorage.h"
#include "core/FixedString.h"

extern device::app::AppController g_controller;
extern device::drivers::ButtonDriver g_buttons;
extern device::drivers::BootstrapConfigStore g_bootstrapStore;
extern device::drivers::DeviceApiClient g_apiClient;
extern device::drivers::DisplayDriver g_display;
extern device::drivers::EventJournalStore g_journal;
extern device::drivers::NfcDriver g_nfc;
extern device::drivers::SdStorage g_sd;
extern device::drivers::NetworkManager g_network;
extern device::domain::DiagnosticsSnapshot g_snapshot;
extern device::domain::NfcTagSnapshot g_tagSnapshot;
extern device::domain::LookupSnapshot g_lookupSnapshot;
extern bool g_lookupFrozen;
extern device::domain::ReceiptSnapshot g_receiptSnapshot;
extern device::domain::ShipmentSnapshot g_shipmentSnapshot;
extern device::domain::ProvisionSnapshot g_provisionSnapshot;
extern device::domain::DatabaseStatusSnapshot g_dbStatus;
extern device::domain::ProductCatalogStatus g_catalogStatus;
extern device::domain::DeviceBootstrapConfig g_bootstrapConfig;
extern device::domain::NetworkStatusSnapshot g_networkStatus;
extern device::domain::SyncStatusSnapshot g_syncStatus;
extern FixedString<256> g_deviceToken;
extern uint32_t g_lastProductSyncAttemptAtMs;
extern uint32_t g_lastAuthAttemptAtMs;
extern uint32_t g_lastOperationSyncAttemptAtMs;
extern device::ui::Screen g_lastRenderedScreen;
extern uint32_t g_lastModeMenuSignature;
extern device::ui::Screen g_lastLoggedScreen;

#pragma once

#include <string_view>
#include <cstdint>
#include "domain/Models.h"
#include "ui/AppStateMachine.h"

uint32_t hashStringView(std::string_view value);
uint32_t modeMenuSignature();

device::domain::DeviceEventRecord makeEvent(
    device::domain::DeviceEventType type,
    device::domain::DeviceEventSeverity severity,
    const char* screen
);

void logEvent(const device::domain::DeviceEventRecord& event);
void logModeEntered(const char* screen);
const char* screenName(device::ui::Screen screen);

void resolveHardwareDeviceId();
void refreshSyncStatus();
void updateControllerStatus();
void refreshCatalogStatus();

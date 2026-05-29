#pragma once

#include "config/DeviceConfig.h"
#include "domain/Models.h"

namespace device::drivers {

class BootstrapConfigStore {
public:
    bool load(device::domain::DeviceBootstrapConfig& config, FixedString<48>& detail) {
        config.clear();
        detail.clear();
        config.productSyncIntervalMs = device::config::kDefaultProductSyncIntervalMs;

        const bool ok = config.wifiSsid.assign(device::config::kDefaultWifiSsid) &&
                        config.wifiPassword.assign(device::config::kDefaultWifiPassword) &&
                        config.apiBaseUrl.assign(device::config::kDefaultApiBaseUrl);
        if (!ok) {
            detail.assign("Firmware config invalid");
            return false;
        }

        config.valid = true;
        detail.assign("Loaded from firmware");
        return true;
    }
};

}  // namespace device::drivers

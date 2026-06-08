#include "Boot.h"
#include "AppGlobals.h"
#include "AppUtils.h"
#include <Arduino.h>
#include "board/Pinout.h"
#include "config/DeviceConfig.h"

void setupBoard() {
    pinMode(device::board::LED_PIN, OUTPUT);
    pinMode(device::board::ERROR_LED_PIN, OUTPUT);
    digitalWrite(device::board::LED_PIN, LOW);
    digitalWrite(device::board::ERROR_LED_PIN, LOW);
}

void blinkLed(uint8_t times, uint16_t onMs, uint16_t offMs) {
    for (uint8_t index = 0; index < times; ++index) {
        digitalWrite(device::board::LED_PIN, HIGH);
        delay(onMs);
        digitalWrite(device::board::LED_PIN, LOW);
        if (index + 1 < times) {
            delay(offMs);
        }
    }
}

void signalProvisionError() {
    digitalWrite(device::board::ERROR_LED_PIN, HIGH);
    delay(220);
    digitalWrite(device::board::ERROR_LED_PIN, LOW);
    delay(100);
    digitalWrite(device::board::ERROR_LED_PIN, HIGH);
    delay(220);
    digitalWrite(device::board::ERROR_LED_PIN, LOW);
}

device::domain::DiagnosticsSnapshot runDiagnostics(bool displayOk) {
    device::domain::DiagnosticsSnapshot snapshot;
    snapshot.clearDetails();
    snapshot.displayOk = displayOk;
    g_bootstrapConfig.clear();
    g_networkStatus.clear();
    g_catalogStatus.clear();

    Serial.println("[diag] display: checking...");
    if (!displayOk) {
        Serial.println("[diag] display: FAILED — aborting boot");
        snapshot.failureCode = device::domain::ErrorCode::TftInit;
        snapshot.addDetailLine("TFT init failed");
        return snapshot;
    }
    snapshot.addDetailLine("TFT: landscape 320x240");
    Serial.println("[diag] display: OK (ILI9341, CS=4 DC=6 RST=5)");

    Serial.println("[diag] buttons: registering GPIO...");
    snapshot.buttonsOk = true;
    snapshot.addDetailLine("Buttons: UP=38 DOWN=35 OK=21");
    Serial.println("[diag] buttons: OK (UP=38 DOWN=35 OK=21, active-low debounce)");
    g_display.renderDiagnostics(snapshot, device::ui::Screen::BootDiagnostics, 0);

    Serial.println("[diag] pn532: probing via FSPI (PN532 HSU/I2C/SPI)...");
    const auto nfcResult = g_nfc.probe();
    snapshot.addDetailLine(nfcResult.detail.c_str());
    if (!nfcResult.ok) {
        Serial.printf("[diag] pn532: FAILED — %s\n", nfcResult.detail.c_str());
        snapshot.failureCode = snapshot.displayOk ? device::domain::ErrorCode::Pn532Firmware : device::domain::ErrorCode::TftInit;
        return snapshot;
    }
    Serial.printf("[diag] pn532: OK — %s\n", nfcResult.detail.c_str());
    snapshot.pn532Ok = true;
    g_display.renderDiagnostics(snapshot, device::ui::Screen::BootDiagnostics, 0);

    Serial.println("[diag] sd: probing via HSPI (CS=11 SCK=12 MOSI=13 MISO=14)...");
    const auto sdResult = g_sd.probe();
    snapshot.addDetailLine(sdResult.detail.c_str());
    if (!sdResult.mounted) {
        Serial.printf("[diag] sd: mount FAILED — %s\n", sdResult.detail.c_str());
        snapshot.failureCode = device::domain::ErrorCode::SdMount;
        return snapshot;
    }
    if (!sdResult.ok) {
        Serial.printf("[diag] sd: read/write FAILED — %s\n", sdResult.detail.c_str());
        snapshot.failureCode = device::domain::ErrorCode::SdReadWrite;
        return snapshot;
    }
    Serial.printf("[diag] sd: OK — %s\n", sdResult.detail.c_str());
    snapshot.sdOk = true;

    Serial.println("[diag] db: opening SQLite on SD...");
    if (!g_journal.begin(g_dbStatus)) {
        Serial.println("[diag] db: FAILED — could not open/create database");
        snapshot.failureCode = device::domain::ErrorCode::DbInit;
        snapshot.addDetailLine("DB init failed");
        return snapshot;
    }
    snapshot.dbOk = true;
    char dbDetail[48] {};
    const auto written = snprintf(dbDetail, sizeof(dbDetail), "DB ready boot %lu", static_cast<unsigned long>(g_dbStatus.bootId));
    snapshot.addDetailLine(written > 0 ? dbDetail : "DB ready");
    Serial.printf("[diag] db: OK — boot_id=%lu\n", static_cast<unsigned long>(g_dbStatus.bootId));

    Serial.println("[diag] config: loading bootstrap config from SD...");
    FixedString<48> configDetail;
    g_networkStatus.configLoaded = g_bootstrapStore.load(g_bootstrapConfig, configDetail);
    if (g_networkStatus.configLoaded) {
        resolveHardwareDeviceId();
        g_networkStatus.detail.assign(configDetail.view());
        snapshot.addDetailLine(configDetail.c_str());
        char deviceLine[48] {};
        const auto deviceWritten = snprintf(deviceLine, sizeof(deviceLine), "Device: %s", g_bootstrapConfig.deviceId.c_str());
        snapshot.addDetailLine(deviceWritten > 0 ? deviceLine : "Device id ready");
        Serial.printf("[diag] config: OK — device_id=%s\n", g_bootstrapConfig.deviceId.c_str());
        Serial.printf("[diag] config: api=%s\n", g_bootstrapConfig.apiBaseUrl.c_str());
        Serial.printf("[diag] config: ssid=%s  sync_interval=%lu ms\n",
            g_bootstrapConfig.wifiSsid.c_str(),
            static_cast<unsigned long>(g_bootstrapConfig.productSyncIntervalMs));
    } else {
        g_networkStatus.detail.assign(configDetail.empty() ? "No device config" : configDetail.view());
        snapshot.addDetailLine(g_networkStatus.detail.c_str());
        Serial.printf("[diag] config: not found — %s\n",
            configDetail.empty() ? "no config file on SD" : configDetail.c_str());
    }

    g_journal.refreshProductCatalogStatus(g_catalogStatus);
    char productDetail[48] {};
    const auto productWritten = snprintf(
        productDetail,
        sizeof(productDetail),
        "Products: %lu",
        static_cast<unsigned long>(g_catalogStatus.totalProducts)
    );
    snapshot.addDetailLine(productWritten > 0 ? productDetail : "Products: 0");
    Serial.printf("[diag] catalog: %lu product(s) in local DB\n",
        static_cast<unsigned long>(g_catalogStatus.totalProducts));

    if (!device::config::kEthernetEnabled) {
        Serial.println("[diag] eth: disabled in config — skipping probe");
        snapshot.ethernetOk = false;
        snapshot.addDetailLine("ETH: disabled");
        g_display.renderDiagnostics(snapshot, device::ui::Screen::BootDiagnostics, 0);
        return snapshot;
    }

    Serial.println("[diag] eth: probing WIZ820io (W5200) via FSPI...");
    Serial.println("[diag] eth: pins — MOSI=37 SCK=36 MISO=39 CS=40 INT=42 RST=41");
    snapshot.addDetailLine("ETH: probing WIZ820io...");
    g_display.renderDiagnostics(snapshot, device::ui::Screen::BootDiagnostics, 0);

    const uint32_t ethStartMs = millis();
    const bool ethOk = g_network.beginEthernet();

    if (snapshot.detailCount > 0) {
        snapshot.detailLines[snapshot.detailCount - 1].clear();
        --snapshot.detailCount;
    }

    if (ethOk) {
        snapshot.ethernetOk = true;
        char ethDetail[48] {};
        const auto ethWritten = snprintf(ethDetail, sizeof(ethDetail),
            "ETH: %s", g_network.localIp().toString().c_str());
        snapshot.addDetailLine(ethWritten > 0 ? ethDetail : "ETH: connected");
        g_networkStatus.ethernetConnected = true;
        g_networkStatus.wifiConnected     = true;
        g_networkStatus.detail.assign(ethWritten > 0 ? ethDetail : "ETH: connected");
        Serial.printf("[diag] eth: OK — ip=%s  dhcp=%lu ms\n",
            g_network.localIp().toString().c_str(),
            static_cast<unsigned long>(millis() - ethStartMs));
    } else {
        snapshot.ethernetOk = false;
        snapshot.addDetailLine("ETH: no link (WiFi fallback)");
        Serial.printf("[diag] eth: no link after %lu ms — will try WiFi fallback\n",
            static_cast<unsigned long>(millis() - ethStartMs));
    }
    g_display.renderDiagnostics(snapshot, device::ui::Screen::BootDiagnostics, 0);

    return snapshot;
}

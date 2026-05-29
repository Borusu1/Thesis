#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Arduino.h>
#include <SPI.h>

#include "board/Pinout.h"
#include "config/DeviceConfig.h"
#include "domain/Models.h"
#include "ui/AppStateMachine.h"
#include "ui/DiagnosticsPresenter.h"

namespace device::drivers {

class DisplayDriver {
public:
    bool begin() {
        pinMode(device::board::TFT_CS, OUTPUT);
        pinMode(device::board::PN532_SS, OUTPUT);
        digitalWrite(device::board::TFT_CS, HIGH);
        digitalWrite(device::board::PN532_SS, HIGH);

        SPI.begin(device::board::TFT_SCK, device::board::TFT_MISO, device::board::TFT_MOSI, device::board::TFT_CS);
        tft_.begin();
        tft_.setRotation(1);
        tft_.fillScreen(ILI9341_BLACK);
        return true;
    }

    void renderDiagnostics(
        const device::domain::DiagnosticsSnapshot& snapshot,
        device::ui::Screen screen,
        std::size_t detailOffset
    ) {
        tft_.fillScreen(ILI9341_BLACK);
        drawHeader();
        drawStatusRows(snapshot);
        drawDetailLines(snapshot, detailOffset);
        drawFooter(screen);
    }

    void renderModeMenu(device::ui::ModeMenuSelection selection) {
        tft_.fillScreen(ILI9341_BLACK);
        drawModeHeader();
        drawModeItem(0, "Lookup", selection == device::ui::ModeMenuSelection::Lookup);
        drawModeItem(1, "Receipt", selection == device::ui::ModeMenuSelection::Receipt);
        drawModeItem(2, "Shipment", selection == device::ui::ModeMenuSelection::Shipment);
        drawModeItem(3, "Provision", selection == device::ui::ModeMenuSelection::Provision);
        drawModeItem(4, "Sync", selection == device::ui::ModeMenuSelection::Sync);
        drawModeItem(5, "Diagnostics", selection == device::ui::ModeMenuSelection::Diagnostics);
    }

    void renderModeMenu(
        device::ui::ModeMenuSelection selection,
        const device::domain::DatabaseStatusSnapshot& dbStatus,
        const device::domain::ProductCatalogStatus& catalogStatus,
        const device::domain::NetworkStatusSnapshot& networkStatus,
        const device::domain::SyncStatusSnapshot& syncStatus
    ) {
        renderModeMenu(selection);
        drawModeStatus(dbStatus, catalogStatus, networkStatus, syncStatus);
        drawModeFooter();
    }

    void renderBadgeLogin(const device::domain::OperatorSessionSnapshot& snapshot) {
        tft_.fillScreen(ILI9341_BLACK);
        drawBadgeHeader();
        drawBadgeSummary(snapshot);
        drawBadgeFooter(snapshot);
    }

    void renderSyncStatus(
        const device::domain::SyncStatusSnapshot& syncStatus,
        const device::domain::NetworkStatusSnapshot& networkStatus
    ) {
        tft_.fillScreen(ILI9341_BLACK);
        drawSyncHeader();
        drawSyncSummary(syncStatus, networkStatus);
        drawSyncFooter();
    }

    void renderLookup(const device::domain::LookupSnapshot& snapshot, std::size_t detailOffset, bool frozen = false) {
        tft_.fillScreen(ILI9341_BLACK);
        drawLookupHeader();
        drawLookupSummary(snapshot);
        drawLookupValue(snapshot);
        drawLookupDetails(snapshot, detailOffset);
        drawLookupFooter(frozen);
    }

    void renderAssign(const device::domain::AssignSnapshot& snapshot, std::size_t detailOffset) {
        tft_.fillScreen(ILI9341_BLACK);
        drawAssignHeader();
        drawAssignSummary(snapshot);
        drawAssignValue(snapshot);
        drawAssignDetails(snapshot, detailOffset);
        drawAssignFooter();
    }

    void renderReceipt(const device::domain::ReceiptSnapshot& snapshot, std::size_t detailOffset) {
        tft_.fillScreen(ILI9341_BLACK);
        drawReceiptHeader();
        drawReceiptSummary(snapshot);
        drawReceiptValue(snapshot);
        drawReceiptDetails(snapshot, detailOffset);
        drawReceiptFooter(snapshot);
    }

    void renderShipment(const device::domain::ShipmentSnapshot& snapshot, std::size_t detailOffset) {
        tft_.fillScreen(ILI9341_BLACK);
        drawShipmentHeader();
        drawShipmentSummary(snapshot);
        drawShipmentValue(snapshot);
        drawShipmentDetails(snapshot, detailOffset);
        drawShipmentFooter(snapshot);
    }

    void renderProvision(const device::domain::ProvisionSnapshot& snapshot, std::size_t detailOffset) {
        tft_.fillScreen(ILI9341_BLACK);
        drawProvisionHeader();
        drawProvisionSummary(snapshot);
        drawProvisionValue(snapshot);
        drawProvisionDetails(snapshot, detailOffset);
        drawProvisionFooter();
    }

    void renderDisplayFailure(device::domain::ErrorCode failureCode) {
        Serial.printf("Display unavailable. Failure code=%d\n", static_cast<int>(failureCode));
    }

private:
    static constexpr int kHeaderHeight = 30;
    static constexpr int kFooterHeight = 20;
    static constexpr int kBodyTop = 38;
    static constexpr int kDetailTop = 120;

    void drawHeader() {
        tft_.fillRect(0, 0, 320, kHeaderHeight, ILI9341_BLUE);
        tft_.setTextColor(ILI9341_WHITE);
        tft_.setTextSize(2);
        tft_.setCursor(8, 8);
        tft_.print(device::config::kFirmwareName);
        tft_.setTextSize(1);
        tft_.setCursor(230, 10);
        tft_.print(device::config::kFirmwareVersion);
    }

    void drawLiveHeader() {
        tft_.fillRect(0, 0, 320, kHeaderHeight, ILI9341_NAVY);
        tft_.setTextColor(ILI9341_WHITE);
        tft_.setTextSize(2);
        tft_.setCursor(8, 8);
        tft_.print("NFC Live");
        tft_.setTextSize(1);
        tft_.setCursor(222, 10);
        tft_.print(device::config::kFirmwareVersion);
    }

    void drawModeHeader() {
        tft_.fillRect(0, 0, 320, kHeaderHeight, ILI9341_DARKCYAN);
        tft_.setTextColor(ILI9341_WHITE);
        tft_.setTextSize(2);
        tft_.setCursor(8, 8);
        tft_.print("Mode Menu");
    }

    void drawBadgeHeader() {
        tft_.fillRect(0, 0, 320, kHeaderHeight, ILI9341_PURPLE);
        tft_.setTextColor(ILI9341_WHITE);
        tft_.setTextSize(2);
        tft_.setCursor(8, 8);
        tft_.print("Badge Login");
    }

    void drawSyncHeader() {
        tft_.fillRect(0, 0, 320, kHeaderHeight, ILI9341_BLUE);
        tft_.setTextColor(ILI9341_WHITE);
        tft_.setTextSize(2);
        tft_.setCursor(8, 8);
        tft_.print("Sync Status");
    }

    void drawReadHeader() {
        tft_.fillRect(0, 0, 320, kHeaderHeight, ILI9341_NAVY);
        tft_.setTextColor(ILI9341_WHITE);
        tft_.setTextSize(2);
        tft_.setCursor(8, 8);
        tft_.print("NFC Read");
    }

    void drawLookupHeader() {
        tft_.fillRect(0, 0, 320, kHeaderHeight, ILI9341_NAVY);
        tft_.setTextColor(ILI9341_WHITE);
        tft_.setTextSize(2);
        tft_.setCursor(8, 8);
        tft_.print("Lookup");
    }

    void drawAssignHeader() {
        tft_.fillRect(0, 0, 320, kHeaderHeight, ILI9341_DARKCYAN);
        tft_.setTextColor(ILI9341_WHITE);
        tft_.setTextSize(2);
        tft_.setCursor(8, 8);
        tft_.print("Assign");
    }

    void drawProvisionHeader() {
        tft_.fillRect(0, 0, 320, kHeaderHeight, ILI9341_MAROON);
        tft_.setTextColor(ILI9341_WHITE);
        tft_.setTextSize(2);
        tft_.setCursor(8, 8);
        tft_.print("Provision");
    }

    void drawReceiptHeader() {
        tft_.fillRect(0, 0, 320, kHeaderHeight, ILI9341_DARKGREEN);
        tft_.setTextColor(ILI9341_WHITE);
        tft_.setTextSize(2);
        tft_.setCursor(8, 8);
        tft_.print("Receipt");
    }

    void drawShipmentHeader() {
        tft_.fillRect(0, 0, 320, kHeaderHeight, ILI9341_MAROON);
        tft_.setTextColor(ILI9341_WHITE);
        tft_.setTextSize(2);
        tft_.setCursor(8, 8);
        tft_.print("Shipment");
    }

    void drawStatusRows(const device::domain::DiagnosticsSnapshot& snapshot) {
        drawStatusRow(0, "TFT",    device::ui::DiagnosticsPresenter::displayRowState(snapshot));
        drawStatusRow(1, "Buttons",device::ui::DiagnosticsPresenter::buttonsRowState(snapshot));
        drawStatusRow(2, "PN532",  device::ui::DiagnosticsPresenter::pn532RowState(snapshot));
        drawStatusRow(3, "SD",     device::ui::DiagnosticsPresenter::sdRowState(snapshot));
        drawStatusRow(4, "DB",     device::ui::DiagnosticsPresenter::dbRowState(snapshot));
        drawStatusRow(5, "WIZ820", device::ui::DiagnosticsPresenter::ethernetRowState(snapshot));
    }

    void drawStatusRow(int rowIndex, const char* label, device::ui::DiagnosticsRowState state) {
        const int y = kBodyTop + (rowIndex * 16);
        tft_.setCursor(10, y);
        tft_.setTextSize(2);
        tft_.setTextColor(ILI9341_WHITE);
        tft_.print(label);
        tft_.setCursor(180, y);
        tft_.setTextColor(colorForRowState(state));
        tft_.print(textForRowState(state));
    }

    void drawDetailLines(const device::domain::DiagnosticsSnapshot& snapshot, std::size_t detailOffset) {
        // 6 status rows × 16 px = 96 px → divider at kBodyTop+96 = 134
        tft_.drawFastHLine(0, 134, 320, ILI9341_DARKGREY);
        tft_.setTextSize(1);
        tft_.setTextColor(ILI9341_CYAN);
        tft_.setCursor(10, 138);
        tft_.print("Details");

        const auto clampedOffset = device::ui::DiagnosticsPresenter::clampDetailOffset(detailOffset, snapshot.detailCount);
        const auto limit = snapshot.detailCount < device::config::kVisibleDetailLines
            ? snapshot.detailCount
            : clampedOffset + device::config::kVisibleDetailLines;

        int y = 152;
        for (std::size_t index = clampedOffset; index < limit; ++index) {
            tft_.setCursor(10, y);
            tft_.setTextColor(ILI9341_WHITE);
            tft_.print(snapshot.detailLines[index].c_str());
            y += 14;
        }

        if (snapshot.failureCode != device::domain::ErrorCode::None) {
            tft_.setCursor(10, 210);
            tft_.setTextColor(ILI9341_RED);
            tft_.print("Fail code: ");
            tft_.print(errorCodeText(snapshot.failureCode));
        }
    }

    void drawFooter(device::ui::Screen screen) {
        tft_.fillRect(0, 220, 320, kFooterHeight, ILI9341_DARKGREY);
        tft_.setTextColor(ILI9341_WHITE);
        tft_.setTextSize(1);
        tft_.setCursor(10, 226);
        tft_.print("Up/Down scroll");
        tft_.setCursor(180, 226);
        if (screen == device::ui::Screen::DiagnosticsFailure) {
            tft_.print("OK retry");
        } else if (screen == device::ui::Screen::DiagnosticsSuccess) {
            tft_.print("OK menu");
        } else {
            tft_.print("Running...");
        }
    }

    void drawModeItem(int rowIndex, const char* label, bool selected) {
        const int y = 48 + (rowIndex * 24);
        if (selected) {
            tft_.fillRoundRect(20, y - 4, 280, 22, 4, ILI9341_DARKGREY);
        }
        tft_.setTextSize(2);
        tft_.setCursor(32, y);
        tft_.setTextColor(selected ? ILI9341_GREEN : ILI9341_WHITE);
        tft_.print(selected ? "> " : "  ");
        tft_.print(label);
    }

    void drawModeFooter() {
        tft_.fillRect(0, 220, 320, kFooterHeight, ILI9341_DARKGREY);
        tft_.setTextColor(ILI9341_WHITE);
        tft_.setTextSize(1);
        tft_.setCursor(10, 226);
        tft_.print("Up/Down select");
        tft_.setCursor(132, 226);
        tft_.print("OK enter");
        tft_.setCursor(228, 226);
        tft_.print("Hold OK back");
    }

    void drawModeStatus(
        const device::domain::DatabaseStatusSnapshot& dbStatus,
        const device::domain::ProductCatalogStatus& catalogStatus,
        const device::domain::NetworkStatusSnapshot& networkStatus,
        const device::domain::SyncStatusSnapshot& syncStatus
    ) {
        tft_.drawFastHLine(0, 170, 320, ILI9341_DARKGREY);
        tft_.setTextSize(1);
        tft_.setTextColor(ILI9341_CYAN);
        tft_.setCursor(10, 178);
        tft_.print("DB");
        tft_.setTextColor(dbStatus.ready ? ILI9341_GREEN : ILI9341_RED);
        tft_.setCursor(10, 190);
        if (dbStatus.ready) {
            tft_.print("DB ready");
            tft_.setCursor(88, 190);
            tft_.setTextColor(catalogStatus.loaded ? ILI9341_GREEN : ILI9341_YELLOW);
            if (catalogStatus.loaded) {
                tft_.print("Products: ");
                tft_.print(catalogStatus.totalProducts);
            } else {
                tft_.print("No products");
            }
            tft_.setCursor(10, 202);
            tft_.setTextColor(ILI9341_WHITE);
            tft_.print("Boot ID: ");
            tft_.print(dbStatus.bootId);
            tft_.setCursor(96, 202);
            tft_.setTextColor(networkStatus.configLoaded ? ILI9341_GREEN : ILI9341_YELLOW);
            tft_.print(networkStatus.configLoaded ? "CFG" : "NO CFG");
            tft_.setCursor(142, 202);
            tft_.setTextColor(networkStatus.wifiConnected ? ILI9341_GREEN : ILI9341_YELLOW);
            tft_.print(networkStatus.wifiConnected ? "WIFI" : "OFFLINE");
            tft_.setCursor(208, 202);
            tft_.setTextColor(networkStatus.deviceAuthenticated ? ILI9341_GREEN : ILI9341_YELLOW);
            tft_.print(networkStatus.deviceAuthenticated ? "API" : "NO API");
            tft_.setCursor(268, 202);
            tft_.setTextColor(networkStatus.deviceAuthenticated ? ILI9341_GREEN : ILI9341_YELLOW);
            tft_.print(networkStatus.deviceAuthenticated ? "DEV" : "NO DEV");
            tft_.setCursor(10, 214);
            tft_.setTextColor(ILI9341_WHITE);
            tft_.print("Pending sync: ");
            tft_.print(syncStatus.pendingCount);
            if (!syncStatus.lastError.empty()) {
                tft_.setCursor(132, 214);
                tft_.setTextColor(ILI9341_ORANGE);
                tft_.print(syncStatus.lastError.c_str());
            }
        } else {
            tft_.print("DB unavailable");
            tft_.setCursor(10, 202);
            tft_.print(device::domain::toString(dbStatus.lastError));
        }
    }

    void drawBadgeSummary(const device::domain::OperatorSessionSnapshot& snapshot) {
        tft_.setTextColor(ILI9341_WHITE);
        tft_.setTextSize(2);
        tft_.setCursor(10, 46);
        tft_.print(snapshot.authenticated ? "Operator active" : "Scan operator badge");

        tft_.setTextSize(2);
        tft_.setCursor(10, 96);
        tft_.setTextColor(ILI9341_CYAN);
        tft_.print("Badge UID:");
        tft_.setCursor(10, 120);
        tft_.setTextColor(snapshot.badgeUid.empty() ? ILI9341_WHITE : ILI9341_YELLOW);
        tft_.print(snapshot.badgeUid.empty() ? "-- waiting --" : snapshot.badgeUid.c_str());

        tft_.setCursor(10, 156);
        tft_.setTextColor(snapshot.authenticated ? ILI9341_GREEN : ILI9341_WHITE);
        if (snapshot.authenticated) {
            tft_.print(snapshot.operatorName.c_str());
            tft_.setTextSize(1);
            tft_.setCursor(10, 178);
            tft_.print(snapshot.operatorRole.c_str());
        } else if (!snapshot.lastError.empty()) {
            tft_.setTextColor(ILI9341_RED);
            tft_.print(snapshot.lastError.c_str());
        } else {
            tft_.print("Present badge near PN532");
        }
    }

    void drawBadgeFooter(const device::domain::OperatorSessionSnapshot& snapshot) {
        tft_.fillRect(0, 220, 320, kFooterHeight, ILI9341_DARKGREY);
        tft_.setTextColor(ILI9341_WHITE);
        tft_.setTextSize(1);
        tft_.setCursor(10, 226);
        tft_.print("Scan badge");
        tft_.setCursor(202, 226);
        tft_.print(snapshot.authenticated ? "Hold OK logout" : "Hold OK back");
    }

    void drawSyncSummary(
        const device::domain::SyncStatusSnapshot& syncStatus,
        const device::domain::NetworkStatusSnapshot& networkStatus
    ) {
        tft_.setTextSize(2);
        tft_.setCursor(10, 42);
        tft_.setTextColor(syncStatus.inProgress ? ILI9341_YELLOW : ILI9341_WHITE);
        tft_.print(syncStatus.inProgress ? "Sync in progress" : "Ready");

        tft_.setTextSize(1);
        tft_.setTextColor(ILI9341_CYAN);
        tft_.setCursor(10, 72);
        tft_.print("Actor:");
        tft_.setTextColor(networkStatus.deviceAuthenticated ? ILI9341_GREEN : ILI9341_YELLOW);
        tft_.setCursor(68, 72);
        tft_.print(networkStatus.deviceAuthenticated ? "Device" : "No device auth");

        tft_.setTextColor(ILI9341_CYAN);
        tft_.setCursor(10, 88);
        tft_.print("Network:");
        tft_.setTextColor((networkStatus.wifiConnected && networkStatus.deviceAuthenticated) ? ILI9341_GREEN : ILI9341_YELLOW);
        tft_.setCursor(68, 88);
        tft_.print((networkStatus.wifiConnected && networkStatus.deviceAuthenticated) ? "Ready" : "Offline/Auth");

        tft_.setTextColor(ILI9341_CYAN);
        tft_.setCursor(10, 104);
        tft_.print("Pending:");
        tft_.setTextColor(ILI9341_WHITE);
        tft_.setCursor(68, 104);
        tft_.print(syncStatus.pendingCount);

        tft_.setTextColor(ILI9341_CYAN);
        tft_.setCursor(10, 120);
        tft_.print("Last sync:");
        tft_.setTextColor(ILI9341_WHITE);
        tft_.setCursor(68, 120);
        if (syncStatus.lastSyncAtMs == 0) {
            tft_.print("--");
        } else {
            tft_.print(syncStatus.lastSyncAtMs / 1000UL);
            tft_.print(" s");
        }

        tft_.setTextColor(ILI9341_CYAN);
        tft_.setCursor(10, 136);
        tft_.print("Last batch:");
        tft_.setTextColor(ILI9341_WHITE);
        tft_.setCursor(68, 136);
        tft_.print(syncStatus.lastSyncedCount);

        if (!syncStatus.lastError.empty()) {
            tft_.setTextColor(ILI9341_RED);
            tft_.setCursor(10, 170);
            tft_.print(syncStatus.lastError.c_str());
        }
    }

    void drawSyncFooter() {
        tft_.fillRect(0, 220, 320, kFooterHeight, ILI9341_DARKGREY);
        tft_.setTextColor(ILI9341_WHITE);
        tft_.setTextSize(1);
        tft_.setCursor(10, 226);
        tft_.print("OK sync");
        tft_.setCursor(92, 226);
        tft_.print("Up clear q");
        tft_.setCursor(216, 226);
        tft_.print("Hold OK back");
    }

    void drawLookupSummary(const device::domain::LookupSnapshot& snapshot) {
        tft_.setTextSize(2);
        tft_.setCursor(10, 40);
        tft_.setTextColor(snapshot.present ? ILI9341_GREEN : ILI9341_YELLOW);
        tft_.print(snapshot.present ? "Tag detected" : "Waiting for tag");

        tft_.setTextSize(1);
        tft_.setTextColor(ILI9341_CYAN);
        tft_.setCursor(10, 68);
        tft_.print("UID:");
        tft_.setTextColor(ILI9341_WHITE);
        tft_.setCursor(48, 68);
        tft_.print(snapshot.present ? snapshot.uidHex.c_str() : "--");

        tft_.setTextColor(ILI9341_CYAN);
        tft_.setCursor(10, 82);
        tft_.print("Local:");
        tft_.setTextColor(snapshot.knownLocally ? ILI9341_GREEN : ILI9341_ORANGE);
        tft_.setCursor(52, 82);
        if (!snapshot.present) {
            tft_.print("--");
        } else if (!snapshot.knownLocally) {
            tft_.print("Unknown");
        } else {
            tft_.print(toString(snapshot.localStatus));
            if (snapshot.pendingSync) {
                tft_.print(" *");
            }
        }
    }

    void drawLookupValue(const device::domain::LookupSnapshot& snapshot) {
        tft_.drawFastHLine(0, 96, 320, ILI9341_DARKGREY);
        tft_.setTextColor(ILI9341_CYAN);
        tft_.setTextSize(1);
        tft_.setCursor(10, 102);
        tft_.print("Value");

        tft_.setTextColor(ILI9341_WHITE);
        tft_.setCursor(10, 116);
        if (snapshot.payloadKind == device::domain::NfcPayloadKind::NdefUuid) {
            tft_.print(snapshot.uuid.c_str());
        } else if (snapshot.payloadKind == device::domain::NfcPayloadKind::LegacyRaw32) {
            tft_.print(snapshot.legacyRaw.c_str());
        } else if (!snapshot.present) {
            tft_.print("Present a tag to lookup");
        } else {
            tft_.print(textForPayloadKind(snapshot.payloadKind));
        }
    }

    void drawLookupDetails(const device::domain::LookupSnapshot& snapshot, std::size_t detailOffset) {
        tft_.drawFastHLine(0, 132, 320, ILI9341_DARKGREY);
        tft_.setTextSize(1);
        tft_.setTextColor(ILI9341_CYAN);
        tft_.setCursor(10, 138);
        tft_.print("Details");

        const auto clampedOffset = device::ui::DiagnosticsPresenter::clampDetailOffset(detailOffset, snapshot.detailCount);
        const auto limit = snapshot.detailCount < device::config::kVisibleDetailLines
            ? snapshot.detailCount
            : clampedOffset + device::config::kVisibleDetailLines;

        int y = 152;
        for (std::size_t index = clampedOffset; index < limit; ++index) {
            tft_.setCursor(10, y);
            tft_.setTextColor(ILI9341_WHITE);
            tft_.print(snapshot.detailLines[index].c_str());
            y += 16;
        }
    }

    void drawLookupFooter(bool frozen = false) {
        tft_.fillRect(0, 220, 320, kFooterHeight, ILI9341_DARKGREY);
        tft_.setTextColor(ILI9341_WHITE);
        tft_.setTextSize(1);
        tft_.setCursor(10, 226);
        tft_.print("Up/Down scroll");
        tft_.setCursor(120, 226);
        tft_.print(frozen ? "OK next tag" : "OK reset");
        tft_.setCursor(220, 226);
        tft_.print("Hold OK menu");
    }

    void drawAssignSummary(const device::domain::AssignSnapshot& snapshot) {
        tft_.setTextSize(2);
        tft_.setCursor(10, 40);
        tft_.setTextColor(colorForAssignStage(snapshot.stage));
        tft_.print(textForAssignStage(snapshot.stage));

        tft_.setTextSize(1);
        tft_.setTextColor(ILI9341_CYAN);
        tft_.setCursor(10, 68);
        tft_.print("UID:");
        tft_.setTextColor(ILI9341_WHITE);
        tft_.setCursor(48, 68);
        tft_.print(snapshot.uidHex.empty() ? "--" : snapshot.uidHex.c_str());

        tft_.setTextColor(ILI9341_CYAN);
        tft_.setCursor(10, 82);
        tft_.print("Status:");
        tft_.setTextColor(snapshot.success ? ILI9341_GREEN : ILI9341_WHITE);
        tft_.setCursor(58, 82);
        if (snapshot.stage == device::domain::AssignStage::WaitingForTag) {
            tft_.print("Ready");
        } else if (snapshot.success) {
            tft_.print(toString(snapshot.localStatus));
            if (snapshot.pendingSync) {
                tft_.print(" *");
            }
        } else {
            tft_.print(errorCodeText(snapshot.errorCode));
        }
    }

    void drawAssignValue(const device::domain::AssignSnapshot& snapshot) {
        tft_.drawFastHLine(0, 96, 320, ILI9341_DARKGREY);
        tft_.setTextColor(ILI9341_CYAN);
        tft_.setTextSize(1);
        tft_.setCursor(10, 102);
        tft_.print("UUID");

        tft_.setTextColor(ILI9341_WHITE);
        tft_.setCursor(10, 116);
        if (snapshot.tagUuid.empty()) {
            tft_.print("Present a provisioned tag");
        } else {
            tft_.print(snapshot.tagUuid.c_str());
        }
    }

    void drawAssignDetails(const device::domain::AssignSnapshot& snapshot, std::size_t detailOffset) {
        tft_.drawFastHLine(0, 132, 320, ILI9341_DARKGREY);
        tft_.setTextSize(1);
        tft_.setTextColor(ILI9341_CYAN);
        tft_.setCursor(10, 138);
        tft_.print("Details");

        const auto clampedOffset = device::ui::DiagnosticsPresenter::clampDetailOffset(detailOffset, snapshot.detailCount);
        const auto limit = snapshot.detailCount < device::config::kVisibleDetailLines
            ? snapshot.detailCount
            : clampedOffset + device::config::kVisibleDetailLines;

        int y = 152;
        for (std::size_t index = clampedOffset; index < limit; ++index) {
            tft_.setCursor(10, y);
            tft_.setTextColor(ILI9341_WHITE);
            tft_.print(snapshot.detailLines[index].c_str());
            y += 16;
        }
    }

    void drawAssignFooter() {
        tft_.fillRect(0, 220, 320, kFooterHeight, ILI9341_DARKGREY);
        tft_.setTextColor(ILI9341_WHITE);
        tft_.setTextSize(1);
        tft_.setCursor(10, 226);
        tft_.print("Up/Down scroll");
        tft_.setCursor(120, 226);
        tft_.print("OK reset");
        tft_.setCursor(220, 226);
        tft_.print("Hold OK menu");
    }

    void drawReceiptSummary(const device::domain::ReceiptSnapshot& snapshot) {
        tft_.setTextSize(2);
        tft_.setCursor(10, 40);
        tft_.setTextColor(colorForReceiptStage(snapshot.stage));
        tft_.print(textForReceiptStage(snapshot.stage));

        tft_.setTextSize(1);
        tft_.setTextColor(ILI9341_CYAN);
        tft_.setCursor(10, 68);
        tft_.print("Product:");
        tft_.setTextColor(snapshot.productsLoaded ? ILI9341_WHITE : ILI9341_YELLOW);
        tft_.setCursor(58, 68);
        if (!snapshot.productsLoaded) {
            tft_.print("No products loaded");
        } else if (snapshot.selectedProduct.exists) {
            tft_.print(snapshot.selectedProduct.name.c_str());
        } else {
            tft_.print(snapshot.skuMode ? "SKU search" : "Select product");
        }

        tft_.setTextColor(ILI9341_CYAN);
        tft_.setCursor(10, 82);
        tft_.print("SKU:");
        tft_.setTextColor(ILI9341_WHITE);
        tft_.setCursor(40, 82);
        if (snapshot.selectedProduct.exists) {
            tft_.print(snapshot.selectedProduct.sku);
        } else {
            printSku(snapshot.enteredSku);
        }

        tft_.setTextColor(ILI9341_CYAN);
        tft_.setCursor(140, 82);
        tft_.print("Qty:");
        tft_.setTextColor(ILI9341_WHITE);
        tft_.setCursor(170, 82);
        tft_.print(snapshot.enteredQuantity);
    }

    void drawReceiptValue(const device::domain::ReceiptSnapshot& snapshot) {
        tft_.drawFastHLine(0, 96, 320, ILI9341_DARKGREY);
        tft_.setTextColor(ILI9341_CYAN);
        tft_.setTextSize(1);
        tft_.setCursor(10, 102);
        if (snapshot.stage == device::domain::ReceiptStage::QuantityInput) {
            tft_.print("Quantity");
        } else {
            tft_.print(snapshot.skuMode ? "SKU Input" : "Product List");
        }

        tft_.setTextColor(ILI9341_WHITE);
        if (snapshot.stage == device::domain::ReceiptStage::QuantityInput) {
            drawQuantityEditor(snapshot.enteredQuantity, snapshot.quantityCursor, snapshot.quantityEditing, 116);
        } else if (snapshot.skuMode) {
            tft_.setCursor(10, 116);
            tft_.print("Entered SKU: ");
            printSku(snapshot.enteredSku);
        } else {
            int y = 116;
            const std::size_t totalRows = snapshot.productCount + 1;
            const std::size_t visibleCount = totalRows < device::config::kReceiptVisibleProducts
                ? totalRows
                : device::config::kReceiptVisibleProducts;
            for (std::size_t index = 0; index < visibleCount; ++index) {
                tft_.setCursor(10, y);
                tft_.setTextColor(index == snapshot.selectedIndex ? ILI9341_GREEN : ILI9341_WHITE);
                tft_.print(index == snapshot.selectedIndex ? "> " : "  ");
                if (index < snapshot.productCount && snapshot.products[index].exists) {
                    tft_.print(snapshot.products[index].sku);
                    tft_.print(" ");
                    tft_.print(snapshot.products[index].name.c_str());
                } else {
                    tft_.print("Search SKU");
                }
                y += 12;
            }
        }
    }

    void drawReceiptDetails(const device::domain::ReceiptSnapshot& snapshot, std::size_t detailOffset) {
        tft_.drawFastHLine(0, 168, 320, ILI9341_DARKGREY);
        tft_.setTextSize(1);
        tft_.setTextColor(ILI9341_CYAN);
        tft_.setCursor(10, 174);
        tft_.print("Details");

        const auto clampedOffset = device::ui::DiagnosticsPresenter::clampDetailOffset(detailOffset, snapshot.detailCount);
        const auto limit = snapshot.detailCount < 3 ? snapshot.detailCount : clampedOffset + 3;
        int y = 188;
        for (std::size_t index = clampedOffset; index < limit && index < snapshot.detailCount; ++index) {
            tft_.setCursor(10, y);
            tft_.setTextColor(ILI9341_WHITE);
            tft_.print(snapshot.detailLines[index].c_str());
            y += 12;
        }
    }

    void drawReceiptFooter(const device::domain::ReceiptSnapshot& snapshot) {
        tft_.fillRect(0, 220, 320, kFooterHeight, ILI9341_DARKGREY);
        tft_.setTextColor(ILI9341_WHITE);
        tft_.setTextSize(1);
        tft_.setCursor(10, 226);
        if (snapshot.stage == device::domain::ReceiptStage::QuantityInput) {
            tft_.print("Up/Down move/edit");
            tft_.setCursor(118, 226);
            tft_.print("OK select/save");
        } else if (snapshot.skuMode) {
            tft_.print("Up/Down digit");
            tft_.setCursor(118, 226);
            tft_.print("OK next/find");
        } else {
            tft_.print("Up/Down select");
            tft_.setCursor(118, 226);
            tft_.print("OK confirm");
        }
        tft_.setCursor(228, 226);
        tft_.print("Hold OK menu");
    }

    void drawShipmentSummary(const device::domain::ShipmentSnapshot& snapshot) {
        tft_.setTextSize(2);
        tft_.setCursor(10, 40);
        tft_.setTextColor(colorForShipmentStage(snapshot.stage));
        tft_.print(textForShipmentStage(snapshot.stage));

        tft_.setTextSize(1);
        tft_.setTextColor(ILI9341_CYAN);
        tft_.setCursor(10, 68);
        tft_.print("UID:");
        tft_.setTextColor(ILI9341_WHITE);
        tft_.setCursor(40, 68);
        tft_.print(snapshot.uidHex.empty() ? "--" : snapshot.uidHex.c_str());

        tft_.setTextColor(ILI9341_CYAN);
        tft_.setCursor(10, 82);
        tft_.print("SKU:");
        tft_.setTextColor(ILI9341_WHITE);
        tft_.setCursor(40, 82);
        if (snapshot.sku == 0) {
            tft_.print("--");
        } else {
            tft_.print(snapshot.sku);
            tft_.print(" ");
            tft_.print(snapshot.productName.c_str());
        }

        tft_.setTextColor(ILI9341_CYAN);
        tft_.setCursor(10, 92);
        tft_.print("Remain:");
        tft_.setTextColor(ILI9341_WHITE);
        tft_.setCursor(56, 92);
        tft_.print(snapshot.quantityCurrent);
    }

    void drawShipmentValue(const device::domain::ShipmentSnapshot& snapshot) {
        tft_.drawFastHLine(0, 106, 320, ILI9341_DARKGREY);
        tft_.setTextColor(ILI9341_CYAN);
        tft_.setTextSize(1);
        tft_.setCursor(10, 112);
        tft_.print(snapshot.stage == device::domain::ShipmentStage::QuantityInput ? "Ship Qty" : "UUID");

        tft_.setTextColor(ILI9341_WHITE);
        tft_.setCursor(10, 126);
        if (snapshot.stage == device::domain::ShipmentStage::QuantityInput) {
            drawQuantityEditor(snapshot.quantityToShip, snapshot.quantityCursor, snapshot.quantityEditing, 126);
        } else if (snapshot.tagUuid.empty()) {
            tft_.print("Present an active tag");
        } else {
            tft_.print(snapshot.tagUuid.c_str());
        }
    }

    void drawShipmentDetails(const device::domain::ShipmentSnapshot& snapshot, std::size_t detailOffset) {
        tft_.drawFastHLine(0, 142, 320, ILI9341_DARKGREY);
        tft_.setTextSize(1);
        tft_.setTextColor(ILI9341_CYAN);
        tft_.setCursor(10, 148);
        tft_.print("Details");

        const auto clampedOffset = device::ui::DiagnosticsPresenter::clampDetailOffset(detailOffset, snapshot.detailCount);
        const auto limit = snapshot.detailCount < device::config::kVisibleDetailLines
            ? snapshot.detailCount
            : clampedOffset + device::config::kVisibleDetailLines;

        int y = 162;
        for (std::size_t index = clampedOffset; index < limit; ++index) {
            tft_.setCursor(10, y);
            tft_.setTextColor(ILI9341_WHITE);
            tft_.print(snapshot.detailLines[index].c_str());
            y += 16;
        }
    }

    void drawShipmentFooter(const device::domain::ShipmentSnapshot& snapshot) {
        tft_.fillRect(0, 220, 320, kFooterHeight, ILI9341_DARKGREY);
        tft_.setTextColor(ILI9341_WHITE);
        tft_.setTextSize(1);
        tft_.setCursor(10, 226);
        if (snapshot.stage == device::domain::ShipmentStage::QuantityInput) {
            tft_.print("Up/Down move/edit");
            tft_.setCursor(118, 226);
            tft_.print("OK select/save");
        } else {
            tft_.print("Waiting for tag");
        }
        tft_.setCursor(220, 226);
        tft_.print("Hold OK menu");
    }

    void drawLiveSummary(const device::domain::NfcTagSnapshot& snapshot) {
        tft_.setTextSize(2);
        tft_.setCursor(10, 40);
        tft_.setTextColor(snapshot.present ? ILI9341_GREEN : ILI9341_YELLOW);
        tft_.print(snapshot.present ? "Tag detected" : "Waiting for tag");

        tft_.setTextSize(1);
        tft_.setTextColor(ILI9341_CYAN);
        tft_.setCursor(10, 68);
        tft_.print("UID:");
        tft_.setTextColor(ILI9341_WHITE);
        tft_.setCursor(48, 68);
        tft_.print(snapshot.present ? snapshot.uidHex.c_str() : "--");

        tft_.setTextColor(ILI9341_CYAN);
        tft_.setCursor(10, 82);
        tft_.print("Payload:");
        tft_.setTextColor(colorForPayloadKind(snapshot.payloadKind));
        tft_.setCursor(64, 82);
        tft_.print(textForPayloadKind(snapshot.payloadKind));
    }

    void drawLiveValue(const device::domain::NfcTagSnapshot& snapshot) {
        tft_.drawFastHLine(0, 96, 320, ILI9341_DARKGREY);
        tft_.setTextColor(ILI9341_CYAN);
        tft_.setTextSize(1);
        tft_.setCursor(10, 102);
        tft_.print("Value");

        tft_.setTextColor(ILI9341_WHITE);
        tft_.setCursor(10, 116);

        if (snapshot.payloadKind == device::domain::NfcPayloadKind::NdefUuid) {
            tft_.print(snapshot.uuid.c_str());
        } else if (snapshot.payloadKind == device::domain::NfcPayloadKind::LegacyRaw32) {
            tft_.print(snapshot.legacyRaw.c_str());
        } else if (!snapshot.present) {
            tft_.print("Present a tag to inspect payload");
        } else {
            tft_.print(textForPayloadKind(snapshot.payloadKind));
        }
    }

    void drawLiveDetails(const device::domain::NfcTagSnapshot& snapshot, std::size_t detailOffset) {
        tft_.drawFastHLine(0, 132, 320, ILI9341_DARKGREY);
        tft_.setTextSize(1);
        tft_.setTextColor(ILI9341_CYAN);
        tft_.setCursor(10, 138);
        tft_.print("Details");

        const auto clampedOffset = device::ui::DiagnosticsPresenter::clampDetailOffset(detailOffset, snapshot.detailCount);
        const auto limit = snapshot.detailCount < device::config::kVisibleDetailLines
            ? snapshot.detailCount
            : clampedOffset + device::config::kVisibleDetailLines;

        int y = 152;
        for (std::size_t index = clampedOffset; index < limit; ++index) {
            tft_.setCursor(10, y);
            tft_.setTextColor(ILI9341_WHITE);
            tft_.print(snapshot.detailLines[index].c_str());
            y += 16;
        }
    }

    void drawReadFooter() {
        tft_.fillRect(0, 220, 320, kFooterHeight, ILI9341_DARKGREY);
        tft_.setTextColor(ILI9341_WHITE);
        tft_.setTextSize(1);
        tft_.setCursor(10, 226);
        tft_.print("Up/Down scroll");
        tft_.setCursor(120, 226);
        tft_.print("OK reset");
        tft_.setCursor(220, 226);
        tft_.print("Hold OK menu");
    }

    void drawProvisionSummary(const device::domain::ProvisionSnapshot& snapshot) {
        tft_.setTextSize(2);
        tft_.setCursor(10, 40);
        tft_.setTextColor(colorForProvisionStage(snapshot.stage));
        tft_.print(textForProvisionStage(snapshot.stage));

        tft_.setTextSize(1);
        tft_.setTextColor(ILI9341_CYAN);
        tft_.setCursor(10, 68);
        tft_.print("UID:");
        tft_.setTextColor(ILI9341_WHITE);
        tft_.setCursor(48, 68);
        tft_.print(snapshot.uidHex.empty() ? "--" : snapshot.uidHex.c_str());

        tft_.setTextColor(ILI9341_CYAN);
        tft_.setCursor(10, 82);
        tft_.print("Result:");
        tft_.setTextColor(snapshot.success ? ILI9341_GREEN : ILI9341_WHITE);
        tft_.setCursor(60, 82);
        tft_.print(snapshot.success ? "Verified" : errorCodeText(snapshot.errorCode));
    }

    void drawProvisionValue(const device::domain::ProvisionSnapshot& snapshot) {
        tft_.drawFastHLine(0, 96, 320, ILI9341_DARKGREY);
        tft_.setTextColor(ILI9341_CYAN);
        tft_.setTextSize(1);
        tft_.setCursor(10, 102);
        tft_.print("UUID");

        tft_.setTextColor(ILI9341_WHITE);
        tft_.setCursor(10, 116);
        if (snapshot.writtenUuid.empty()) {
            tft_.print("Present a writable tag");
        } else {
            tft_.print(snapshot.writtenUuid.c_str());
        }
    }

    void drawProvisionDetails(const device::domain::ProvisionSnapshot& snapshot, std::size_t detailOffset) {
        tft_.drawFastHLine(0, 132, 320, ILI9341_DARKGREY);
        tft_.setTextSize(1);
        tft_.setTextColor(ILI9341_CYAN);
        tft_.setCursor(10, 138);
        tft_.print("Details");

        const auto clampedOffset = device::ui::DiagnosticsPresenter::clampDetailOffset(detailOffset, snapshot.detailCount);
        const auto limit = snapshot.detailCount < device::config::kVisibleDetailLines
            ? snapshot.detailCount
            : clampedOffset + device::config::kVisibleDetailLines;

        int y = 152;
        for (std::size_t index = clampedOffset; index < limit; ++index) {
            tft_.setCursor(10, y);
            tft_.setTextColor(ILI9341_WHITE);
            tft_.print(snapshot.detailLines[index].c_str());
            y += 16;
        }
    }

    void drawProvisionFooter() {
        tft_.fillRect(0, 220, 320, kFooterHeight, ILI9341_DARKGREY);
        tft_.setTextColor(ILI9341_WHITE);
        tft_.setTextSize(1);
        tft_.setCursor(10, 226);
        tft_.print("Up/Down scroll");
        tft_.setCursor(120, 226);
        tft_.print("OK next");
        tft_.setCursor(220, 226);
        tft_.print("Hold OK menu");
    }

    static uint16_t colorForRowState(device::ui::DiagnosticsRowState state) {
        switch (state) {
            case device::ui::DiagnosticsRowState::Ok:
                return ILI9341_GREEN;
            case device::ui::DiagnosticsRowState::Fail:
                return ILI9341_RED;
            case device::ui::DiagnosticsRowState::Pending:
            default:
                return ILI9341_YELLOW;
        }
    }

    static const char* textForRowState(device::ui::DiagnosticsRowState state) {
        switch (state) {
            case device::ui::DiagnosticsRowState::Ok:
                return "OK";
            case device::ui::DiagnosticsRowState::Fail:
                return "FAIL";
            case device::ui::DiagnosticsRowState::Pending:
            default:
                return "PENDING";
        }
    }

    static const char* errorCodeText(device::domain::ErrorCode code) {
        switch (code) {
            case device::domain::ErrorCode::TftInit:
                return "TFT_INIT";
            case device::domain::ErrorCode::ButtonsInit:
                return "BUTTONS_INIT";
            case device::domain::ErrorCode::Pn532Init:
                return "PN532_INIT";
            case device::domain::ErrorCode::Pn532Firmware:
                return "PN532_FW";
            case device::domain::ErrorCode::NfcReadError:
                return "NFC_READ";
            case device::domain::ErrorCode::NfcUnsupportedPayload:
                return "NFC_UNSUPPORTED";
            case device::domain::ErrorCode::NfcWriteError:
                return "NFC_WRITE";
            case device::domain::ErrorCode::NfcVerifyError:
                return "NFC_VERIFY";
            case device::domain::ErrorCode::SdMount:
                return "SD_MOUNT";
            case device::domain::ErrorCode::SdReadWrite:
                return "SD_RW";
            case device::domain::ErrorCode::DbInit:
                return "DB_INIT";
            case device::domain::ErrorCode::DbWrite:
                return "DB_WRITE";
            default:
                return "NONE";
        }
    }

    static uint16_t colorForPayloadKind(device::domain::NfcPayloadKind kind) {
        switch (kind) {
            case device::domain::NfcPayloadKind::NdefUuid:
                return ILI9341_GREEN;
            case device::domain::NfcPayloadKind::LegacyRaw32:
                return ILI9341_ORANGE;
            case device::domain::NfcPayloadKind::Blank:
                return ILI9341_YELLOW;
            case device::domain::NfcPayloadKind::Unsupported:
            case device::domain::NfcPayloadKind::ReadError:
                return ILI9341_RED;
            case device::domain::NfcPayloadKind::None:
            default:
                return ILI9341_WHITE;
        }
    }

    static const char* textForPayloadKind(device::domain::NfcPayloadKind kind) {
        switch (kind) {
            case device::domain::NfcPayloadKind::Blank:
                return "Blank";
            case device::domain::NfcPayloadKind::NdefUuid:
                return "UUID found";
            case device::domain::NfcPayloadKind::LegacyRaw32:
                return "Legacy raw";
            case device::domain::NfcPayloadKind::Unsupported:
                return "Unsupported";
            case device::domain::NfcPayloadKind::ReadError:
                return "Read error";
            case device::domain::NfcPayloadKind::None:
            default:
                return "None";
        }
    }

    static uint16_t colorForProvisionStage(device::domain::ProvisionStage stage) {
        switch (stage) {
            case device::domain::ProvisionStage::Success:
                return ILI9341_GREEN;
            case device::domain::ProvisionStage::Failure:
                return ILI9341_RED;
            case device::domain::ProvisionStage::Writing:
            case device::domain::ProvisionStage::Verifying:
                return ILI9341_YELLOW;
            case device::domain::ProvisionStage::WaitingForTag:
            default:
                return ILI9341_WHITE;
        }
    }

    static const char* textForProvisionStage(device::domain::ProvisionStage stage) {
        switch (stage) {
            case device::domain::ProvisionStage::WaitingForTag:
                return "Waiting for tag";
            case device::domain::ProvisionStage::Writing:
                return "Writing";
            case device::domain::ProvisionStage::Verifying:
                return "Verifying";
            case device::domain::ProvisionStage::Success:
                return "Success";
            case device::domain::ProvisionStage::Failure:
                return "Failure";
            default:
                return "Unknown";
        }
    }

    static uint16_t colorForAssignStage(device::domain::AssignStage stage) {
        switch (stage) {
            case device::domain::AssignStage::Success:
                return ILI9341_GREEN;
            case device::domain::AssignStage::Failure:
                return ILI9341_RED;
            case device::domain::AssignStage::Assigning:
                return ILI9341_YELLOW;
            case device::domain::AssignStage::WaitingForTag:
            default:
                return ILI9341_WHITE;
        }
    }

    static const char* textForAssignStage(device::domain::AssignStage stage) {
        switch (stage) {
            case device::domain::AssignStage::WaitingForTag:
                return "Waiting for tag";
            case device::domain::AssignStage::Assigning:
                return "Assigning";
            case device::domain::AssignStage::Success:
                return "Assigned";
            case device::domain::AssignStage::Failure:
                return "Failure";
            default:
                return "Unknown";
        }
    }

    static uint16_t colorForReceiptStage(device::domain::ReceiptStage stage) {
        switch (stage) {
            case device::domain::ReceiptStage::Success:
                return ILI9341_GREEN;
            case device::domain::ReceiptStage::Failure:
                return ILI9341_RED;
            case device::domain::ReceiptStage::WaitingForTag:
                return ILI9341_YELLOW;
            case device::domain::ReceiptStage::QuantityInput:
                return ILI9341_CYAN;
            case device::domain::ReceiptStage::SkuInput:
            case device::domain::ReceiptStage::SelectProduct:
            default:
                return ILI9341_WHITE;
        }
    }

    static const char* textForReceiptStage(device::domain::ReceiptStage stage) {
        switch (stage) {
            case device::domain::ReceiptStage::SelectProduct:
                return "Select product";
            case device::domain::ReceiptStage::SkuInput:
                return "Enter SKU";
            case device::domain::ReceiptStage::QuantityInput:
                return "Enter qty";
            case device::domain::ReceiptStage::WaitingForTag:
                return "Scan tag";
            case device::domain::ReceiptStage::Success:
                return "Received";
            case device::domain::ReceiptStage::Failure:
                return "Failure";
            default:
                return "Unknown";
        }
    }

    static uint16_t colorForShipmentStage(device::domain::ShipmentStage stage) {
        switch (stage) {
            case device::domain::ShipmentStage::Success:
                return ILI9341_GREEN;
            case device::domain::ShipmentStage::Failure:
                return ILI9341_RED;
            case device::domain::ShipmentStage::QuantityInput:
                return ILI9341_CYAN;
            case device::domain::ShipmentStage::WaitingForTag:
            default:
                return ILI9341_WHITE;
        }
    }

    static const char* textForShipmentStage(device::domain::ShipmentStage stage) {
        switch (stage) {
            case device::domain::ShipmentStage::WaitingForTag:
                return "Scan tag";
            case device::domain::ShipmentStage::QuantityInput:
                return "Enter qty";
            case device::domain::ShipmentStage::Success:
                return "Shipped";
            case device::domain::ShipmentStage::Failure:
                return "Failure";
            default:
                return "Unknown";
        }
    }

    void printSku(uint32_t sku) {
        char buffer[8] {};
        snprintf(buffer, sizeof(buffer), "%04lu", static_cast<unsigned long>(sku));
        tft_.print(buffer);
    }

    void printQuantity(uint32_t quantity) {
        char buffer[16] {};
        snprintf(buffer, sizeof(buffer), "%05lu", static_cast<unsigned long>(quantity));
        tft_.print(buffer);
    }

    void drawQuantityEditor(uint32_t quantity, uint32_t cursor, bool editing, int topY) {
        char buffer[16] {};
        snprintf(buffer, sizeof(buffer), "%05lu", static_cast<unsigned long>(quantity));

        constexpr int kDigitWidth = 32;
        constexpr int kDigitHeight = 28;
        constexpr int kDigitGap = 6;
        constexpr int kStartX = 12;
        const int saveY = topY + 36;

        tft_.setTextSize(2);
        for (std::size_t index = 0; index < device::config::kQuantityDigits; ++index) {
            const int x = kStartX + static_cast<int>(index) * (kDigitWidth + kDigitGap);
            const bool selected = cursor == index;
            const uint16_t border = selected ? (editing ? ILI9341_GREEN : ILI9341_YELLOW) : ILI9341_DARKGREY;
            const uint16_t fill = editing && selected ? ILI9341_DARKGREEN : ILI9341_BLACK;
            tft_.fillRect(x, topY, kDigitWidth, kDigitHeight, fill);
            tft_.drawRect(x, topY, kDigitWidth, kDigitHeight, border);
            tft_.setTextColor(selected && editing ? ILI9341_WHITE : ILI9341_CYAN);
            tft_.setCursor(x + 10, topY + 7);
            tft_.print(buffer[index]);
        }

        const bool saveSelected = cursor >= device::config::kQuantityDigits;
        const uint16_t saveBorder = saveSelected ? ILI9341_YELLOW : ILI9341_DARKGREY;
        tft_.fillRect(110, saveY, 100, 22, saveSelected ? ILI9341_NAVY : ILI9341_BLACK);
        tft_.drawRect(110, saveY, 100, 22, saveBorder);
        tft_.setTextSize(1);
        tft_.setTextColor(ILI9341_WHITE);
        tft_.setCursor(146, saveY + 7);
        tft_.print("SAVE");
    }

    Adafruit_ILI9341 tft_ {device::board::TFT_CS, device::board::TFT_DC, device::board::TFT_RST};
};

}  // namespace device::drivers

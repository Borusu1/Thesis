#include "UiRender.h"
#include "AppGlobals.h"
#include "AppUtils.h"
#include <Arduino.h>
#include "ui/AppStateMachine.h"

void renderCurrentScreen() {
    if (!g_snapshot.displayOk) {
        g_display.renderDisplayFailure(g_snapshot.failureCode);
        return;
    }

    const auto screen = g_controller.stateMachine().currentScreen();
    if (screen != g_lastLoggedScreen) {
        Serial.printf("[ui] screen=%s\n", screenName(screen));
        g_lastLoggedScreen = screen;
    }
    if (screen == device::ui::Screen::ModeMenu) {
        const auto signature = modeMenuSignature();
        if (g_lastRenderedScreen == device::ui::Screen::ModeMenu && signature == g_lastModeMenuSignature) {
            return;
        }
        g_display.renderModeMenu(
            g_controller.stateMachine().modeMenuSelection(),
            g_controller.databaseStatus(),
            g_controller.catalogStatus(),
            g_controller.networkStatus(),
            g_controller.syncStatus()
        );
        g_lastRenderedScreen = screen;
        g_lastModeMenuSignature = signature;
        return;
    }

    g_lastRenderedScreen = screen;
    g_lastModeMenuSignature = 0;

    if (screen == device::ui::Screen::SyncStatus) {
        g_display.renderSyncStatus(g_controller.syncStatus(), g_controller.networkStatus());
        return;
    }

    if (screen == device::ui::Screen::InventoryLookup) {
        g_display.renderLookup(g_controller.lookupSnapshot(), g_controller.stateMachine().detailScrollOffset(), g_lookupFrozen);
        return;
    }

    if (screen == device::ui::Screen::InventoryReceipt) {
        g_display.renderReceipt(g_controller.receiptSnapshot(), g_controller.stateMachine().detailScrollOffset());
        return;
    }

    if (screen == device::ui::Screen::InventoryShipment) {
        g_display.renderShipment(g_controller.shipmentSnapshot(), g_controller.stateMachine().detailScrollOffset());
        return;
    }

    if (screen == device::ui::Screen::NfcProvision) {
        g_display.renderProvision(g_controller.provisionSnapshot(), g_controller.stateMachine().detailScrollOffset());
        return;
    }

    g_display.renderDiagnostics(g_controller.diagnosticsSnapshot(), screen, g_controller.stateMachine().detailScrollOffset());
}

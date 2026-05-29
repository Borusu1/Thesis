#pragma once

#include "domain/ButtonInterpreter.h"
#include "domain/Models.h"
#include "domain/OfflineQueue.h"
#include "ui/AppStateMachine.h"

namespace device::app {

class AppController {
public:
    void startDiagnostics() {
        stateMachine_.startDiagnostics();
    }

    void finishDiagnostics(const domain::DiagnosticsSnapshot& snapshot) {
        diagnosticsSnapshot_ = snapshot;
        stateMachine_.finishDiagnostics(snapshot.failureCode == domain::ErrorCode::None, snapshot.detailCount);
    }

    const ui::AppStateMachine& stateMachine() const {
        return stateMachine_;
    }

    void setScreen(ui::Screen screen) {
        stateMachine_.setScreen(screen);
    }

    const domain::DiagnosticsSnapshot& diagnosticsSnapshot() const {
        return diagnosticsSnapshot_;
    }

    void updateNfcSnapshot(const domain::NfcTagSnapshot& snapshot) {
        nfcSnapshot_ = snapshot;
    }

    void clearNfcSnapshot() {
        nfcSnapshot_.clear();
    }

    const domain::NfcTagSnapshot& nfcSnapshot() const {
        return nfcSnapshot_;
    }

    void updateLookupSnapshot(const domain::LookupSnapshot& snapshot) {
        lookupSnapshot_ = snapshot;
    }

    void clearLookupSnapshot() {
        lookupSnapshot_.clear();
    }

    const domain::LookupSnapshot& lookupSnapshot() const {
        return lookupSnapshot_;
    }

    void updateAssignSnapshot(const domain::AssignSnapshot& snapshot) {
        assignSnapshot_ = snapshot;
    }

    void clearAssignSnapshot() {
        assignSnapshot_.clear();
    }

    const domain::AssignSnapshot& assignSnapshot() const {
        return assignSnapshot_;
    }

    void updateProvisionSnapshot(const domain::ProvisionSnapshot& snapshot) {
        provisionSnapshot_ = snapshot;
    }

    void clearProvisionSnapshot() {
        provisionSnapshot_.clear();
    }

    const domain::ProvisionSnapshot& provisionSnapshot() const {
        return provisionSnapshot_;
    }

    void updateDatabaseStatus(const domain::DatabaseStatusSnapshot& snapshot) {
        databaseStatus_ = snapshot;
    }

    const domain::DatabaseStatusSnapshot& databaseStatus() const {
        return databaseStatus_;
    }

    void updateCatalogStatus(const domain::ProductCatalogStatus& status) {
        catalogStatus_ = status;
    }

    const domain::ProductCatalogStatus& catalogStatus() const {
        return catalogStatus_;
    }

    void updateNetworkStatus(const domain::NetworkStatusSnapshot& status) {
        networkStatus_ = status;
    }

    const domain::NetworkStatusSnapshot& networkStatus() const {
        return networkStatus_;
    }

    void updateOperatorSession(const domain::OperatorSessionSnapshot& snapshot) {
        operatorSession_ = snapshot;
    }

    const domain::OperatorSessionSnapshot& operatorSession() const {
        return operatorSession_;
    }

    void updateSyncStatus(const domain::SyncStatusSnapshot& snapshot) {
        syncStatus_ = snapshot;
    }

    const domain::SyncStatusSnapshot& syncStatus() const {
        return syncStatus_;
    }

    void updateReceiptSnapshot(const domain::ReceiptSnapshot& snapshot) {
        receiptSnapshot_ = snapshot;
    }

    void clearReceiptSnapshot() {
        receiptSnapshot_.clear();
    }

    const domain::ReceiptSnapshot& receiptSnapshot() const {
        return receiptSnapshot_;
    }

    void updateShipmentSnapshot(const domain::ShipmentSnapshot& snapshot) {
        shipmentSnapshot_ = snapshot;
    }

    void clearShipmentSnapshot() {
        shipmentSnapshot_.clear();
    }

    const domain::ShipmentSnapshot& shipmentSnapshot() const {
        return shipmentSnapshot_;
    }

    ui::AppStateMachine::Action handleButtonEvent(const domain::ButtonEvent& event) {
        std::size_t detailCount = diagnosticsSnapshot_.detailCount;
        if (stateMachine_.currentScreen() == ui::Screen::InventoryLookup) {
            detailCount = lookupSnapshot_.detailCount;
        } else if (stateMachine_.currentScreen() == ui::Screen::InventoryReceipt) {
            detailCount = receiptSnapshot_.detailCount;
        } else if (stateMachine_.currentScreen() == ui::Screen::InventoryShipment) {
            detailCount = shipmentSnapshot_.detailCount;
        } else if (stateMachine_.currentScreen() == ui::Screen::SyncStatus) {
            detailCount = 0;
        } else if (stateMachine_.currentScreen() == ui::Screen::NfcProvision) {
            detailCount = provisionSnapshot_.detailCount;
        }
        return stateMachine_.handleButtonEvent(event, detailCount);
    }

    domain::OfflineQueue& offlineQueue() {
        return offlineQueue_;
    }

private:
    ui::AppStateMachine stateMachine_;
    domain::DiagnosticsSnapshot diagnosticsSnapshot_ {};
    domain::NfcTagSnapshot nfcSnapshot_ {};
    domain::LookupSnapshot lookupSnapshot_ {};
    domain::ReceiptSnapshot receiptSnapshot_ {};
    domain::ShipmentSnapshot shipmentSnapshot_ {};
    domain::AssignSnapshot assignSnapshot_ {};
    domain::ProvisionSnapshot provisionSnapshot_ {};
    domain::DatabaseStatusSnapshot databaseStatus_ {};
    domain::ProductCatalogStatus catalogStatus_ {};
    domain::NetworkStatusSnapshot networkStatus_ {};
    domain::OperatorSessionSnapshot operatorSession_ {};
    domain::SyncStatusSnapshot syncStatus_ {};
    domain::OfflineQueue offlineQueue_;
};

}  // namespace device::app

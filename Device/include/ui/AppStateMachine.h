#pragma once

#include <optional>

#include "config/DeviceConfig.h"
#include "domain/ButtonInterpreter.h"

namespace device::ui {

enum class Screen : uint8_t {
    BootDiagnostics,
    DiagnosticsFailure,
    DiagnosticsSuccess,
    DiagnosticsView,
    ModeMenu,
    SyncStatus,
    InventoryLookup,
    InventoryReceipt,
    InventoryShipment,
    NfcProvision,
};

enum class ModeMenuSelection : uint8_t {
    Lookup,
    Receipt,
    Shipment,
    Provision,
    Sync,
    Diagnostics,
};

class AppStateMachine {
public:
    Screen currentScreen() const {
        return currentScreen_;
    }

    void startDiagnostics() {
        currentScreen_ = Screen::BootDiagnostics;
        detailScrollOffset_ = 0;
    }

    void finishDiagnostics(bool success, std::size_t detailCount) {
        currentScreen_ = success ? Screen::DiagnosticsSuccess : Screen::DiagnosticsFailure;
        detailScrollOffset_ = clampOffset(detailScrollOffset_, detailCount);
    }

    std::size_t detailScrollOffset() const {
        return detailScrollOffset_;
    }

    ModeMenuSelection modeMenuSelection() const {
        return modeMenuSelection_;
    }

    void setScreen(Screen screen) {
        currentScreen_ = screen;
        detailScrollOffset_ = 0;
    }

    enum class Action : uint8_t {
        None,
        RetryDiagnostics,
        EnterModeMenu,
        EnterSyncStatus,
        EnterLookupMode,
        EnterReceiptMode,
        EnterShipmentMode,
        EnterProvisionMode,
        EnterDiagnosticsView,
        ResetLookupScan,
        ResetReceiptFlow,
        ResetShipmentFlow,
        ResetProvisionFlow,
        ReturnToDiagnostics,
        ReturnToModeMenu,
    };

    Action handleButtonEvent(
        const device::domain::ButtonEvent& event,
        std::size_t detailCount
    ) {
        if (currentScreen_ == Screen::ModeMenu) {
            if (event.type == device::domain::ButtonPressType::LongPress && event.button == device::config::Button::Ok) {
                currentScreen_ = Screen::DiagnosticsSuccess;
                detailScrollOffset_ = 0;
                return Action::ReturnToDiagnostics;
            }
            if (event.type != device::domain::ButtonPressType::ShortPress) {
                return Action::None;
            }
            switch (event.button) {
                case device::config::Button::Up:
                    modeMenuSelection_ = previousSelection(modeMenuSelection_);
                    return Action::None;
                case device::config::Button::Down:
                    modeMenuSelection_ = nextSelection(modeMenuSelection_);
                    return Action::None;
                case device::config::Button::Ok:
                    switch (modeMenuSelection_) {
                        case ModeMenuSelection::Lookup:
                            currentScreen_ = Screen::InventoryLookup;
                            detailScrollOffset_ = 0;
                            return Action::EnterLookupMode;
                        case ModeMenuSelection::Receipt:
                            currentScreen_ = Screen::InventoryReceipt;
                            detailScrollOffset_ = 0;
                            return Action::EnterReceiptMode;
                        case ModeMenuSelection::Shipment:
                            currentScreen_ = Screen::InventoryShipment;
                            detailScrollOffset_ = 0;
                            return Action::EnterShipmentMode;
                        case ModeMenuSelection::Provision:
                            currentScreen_ = Screen::NfcProvision;
                            detailScrollOffset_ = 0;
                            return Action::EnterProvisionMode;
                        case ModeMenuSelection::Sync:
                            currentScreen_ = Screen::SyncStatus;
                            detailScrollOffset_ = 0;
                            return Action::EnterSyncStatus;
                        case ModeMenuSelection::Diagnostics:
                            currentScreen_ = Screen::DiagnosticsView;
                            detailScrollOffset_ = 0;
                            return Action::EnterDiagnosticsView;
                    }
                    return Action::None;
            }
        }

        if (currentScreen_ == Screen::SyncStatus) {
            if (event.type == device::domain::ButtonPressType::LongPress && event.button == device::config::Button::Ok) {
                currentScreen_ = Screen::ModeMenu;
                detailScrollOffset_ = 0;
                return Action::ReturnToModeMenu;
            }
            return Action::None;
        }

        if (currentScreen_ == Screen::InventoryLookup) {
            if (event.type == device::domain::ButtonPressType::LongPress && event.button == device::config::Button::Ok) {
                currentScreen_ = Screen::ModeMenu;
                detailScrollOffset_ = 0;
                return Action::ReturnToModeMenu;
            }
            if (event.type != device::domain::ButtonPressType::ShortPress) {
                return Action::None;
            }
            switch (event.button) {
                case device::config::Button::Up:
                    if (detailScrollOffset_ > 0) {
                        --detailScrollOffset_;
                    }
                    return Action::None;
                case device::config::Button::Down:
                    detailScrollOffset_ = clampOffset(detailScrollOffset_ + 1, detailCount);
                    return Action::None;
                case device::config::Button::Ok:
                    detailScrollOffset_ = 0;
                    return Action::ResetLookupScan;
            }
        }

        if (currentScreen_ == Screen::InventoryReceipt) {
            if (event.type == device::domain::ButtonPressType::LongPress && event.button == device::config::Button::Ok) {
                currentScreen_ = Screen::ModeMenu;
                detailScrollOffset_ = 0;
                return Action::ReturnToModeMenu;
            }
            if (event.type != device::domain::ButtonPressType::ShortPress) {
                return Action::None;
            }
            switch (event.button) {
                case device::config::Button::Up:
                    if (detailScrollOffset_ > 0) {
                        --detailScrollOffset_;
                    }
                    return Action::None;
                case device::config::Button::Down:
                    detailScrollOffset_ = clampOffset(detailScrollOffset_ + 1, detailCount);
                    return Action::None;
                case device::config::Button::Ok:
                    detailScrollOffset_ = 0;
                    return Action::ResetReceiptFlow;
            }
        }

        if (currentScreen_ == Screen::InventoryShipment) {
            if (event.type == device::domain::ButtonPressType::LongPress && event.button == device::config::Button::Ok) {
                currentScreen_ = Screen::ModeMenu;
                detailScrollOffset_ = 0;
                return Action::ReturnToModeMenu;
            }
            if (event.type != device::domain::ButtonPressType::ShortPress) {
                return Action::None;
            }
            switch (event.button) {
                case device::config::Button::Up:
                    if (detailScrollOffset_ > 0) {
                        --detailScrollOffset_;
                    }
                    return Action::None;
                case device::config::Button::Down:
                    detailScrollOffset_ = clampOffset(detailScrollOffset_ + 1, detailCount);
                    return Action::None;
                case device::config::Button::Ok:
                    detailScrollOffset_ = 0;
                    return Action::ResetShipmentFlow;
            }
        }

        if (currentScreen_ == Screen::NfcProvision) {
            if (event.type == device::domain::ButtonPressType::LongPress && event.button == device::config::Button::Ok) {
                currentScreen_ = Screen::ModeMenu;
                detailScrollOffset_ = 0;
                return Action::ReturnToModeMenu;
            }
            if (event.type != device::domain::ButtonPressType::ShortPress) {
                return Action::None;
            }
            switch (event.button) {
                case device::config::Button::Up:
                    if (detailScrollOffset_ > 0) {
                        --detailScrollOffset_;
                    }
                    return Action::None;
                case device::config::Button::Down:
                    detailScrollOffset_ = clampOffset(detailScrollOffset_ + 1, detailCount);
                    return Action::None;
                case device::config::Button::Ok:
                    detailScrollOffset_ = 0;
                    return Action::ResetProvisionFlow;
            }
        }

        if (currentScreen_ == Screen::DiagnosticsView) {
            if (event.type == device::domain::ButtonPressType::LongPress && event.button == device::config::Button::Ok) {
                currentScreen_ = Screen::ModeMenu;
                detailScrollOffset_ = 0;
                return Action::ReturnToModeMenu;
            }
            if (event.type != device::domain::ButtonPressType::ShortPress) {
                return Action::None;
            }
            switch (event.button) {
                case device::config::Button::Up:
                    if (detailScrollOffset_ > 0) {
                        --detailScrollOffset_;
                    }
                    return Action::None;
                case device::config::Button::Down:
                    detailScrollOffset_ = clampOffset(detailScrollOffset_ + 1, detailCount);
                    return Action::None;
                case device::config::Button::Ok:
                    currentScreen_ = Screen::ModeMenu;
                    detailScrollOffset_ = 0;
                    return Action::ReturnToModeMenu;
            }
        }

        if (event.type != device::domain::ButtonPressType::ShortPress) {
            return Action::None;
        }

        switch (event.button) {
            case device::config::Button::Up:
                if (detailScrollOffset_ > 0) {
                    --detailScrollOffset_;
                }
                return Action::None;
            case device::config::Button::Down:
                detailScrollOffset_ = clampOffset(detailScrollOffset_ + 1, detailCount);
                return Action::None;
            case device::config::Button::Ok:
                if (currentScreen_ == Screen::DiagnosticsFailure) {
                    currentScreen_ = Screen::BootDiagnostics;
                    detailScrollOffset_ = 0;
                    return Action::RetryDiagnostics;
                }
                if (currentScreen_ == Screen::DiagnosticsSuccess) {
                    currentScreen_ = Screen::ModeMenu;
                    detailScrollOffset_ = 0;
                    return Action::EnterModeMenu;
                }
                return Action::None;
        }

        return Action::None;
    }

private:
    static ModeMenuSelection nextSelection(ModeMenuSelection current) {
        switch (current) {
            case ModeMenuSelection::Lookup:
                return ModeMenuSelection::Receipt;
            case ModeMenuSelection::Receipt:
                return ModeMenuSelection::Shipment;
            case ModeMenuSelection::Shipment:
                return ModeMenuSelection::Provision;
            case ModeMenuSelection::Provision:
                return ModeMenuSelection::Sync;
            case ModeMenuSelection::Sync:
                return ModeMenuSelection::Diagnostics;
            case ModeMenuSelection::Diagnostics:
            default:
                return ModeMenuSelection::Lookup;
        }
    }

    static ModeMenuSelection previousSelection(ModeMenuSelection current) {
        switch (current) {
            case ModeMenuSelection::Lookup:
                return ModeMenuSelection::Diagnostics;
            case ModeMenuSelection::Receipt:
                return ModeMenuSelection::Lookup;
            case ModeMenuSelection::Shipment:
                return ModeMenuSelection::Receipt;
            case ModeMenuSelection::Provision:
                return ModeMenuSelection::Shipment;
            case ModeMenuSelection::Sync:
                return ModeMenuSelection::Provision;
            case ModeMenuSelection::Diagnostics:
            default:
                return ModeMenuSelection::Sync;
        }
    }

    static std::size_t clampOffset(std::size_t requestedOffset, std::size_t detailCount) {
        if (detailCount <= device::config::kVisibleDetailLines) {
            return 0;
        }
        const auto maxOffset = detailCount - device::config::kVisibleDetailLines;
        return requestedOffset > maxOffset ? maxOffset : requestedOffset;
    }

    Screen currentScreen_ = Screen::BootDiagnostics;
    std::size_t detailScrollOffset_ = 0;
    ModeMenuSelection modeMenuSelection_ = ModeMenuSelection::Lookup;
};

}

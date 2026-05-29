#pragma once

#include <cstddef>

#include "config/DeviceConfig.h"
#include "domain/Models.h"

namespace device::ui {

enum class DiagnosticsRowState : uint8_t {
    Pending,
    Ok,
    Fail,
};

class DiagnosticsPresenter {
public:
    static DiagnosticsRowState displayRowState(const device::domain::DiagnosticsSnapshot& snapshot) {
        return snapshot.displayOk ? DiagnosticsRowState::Ok : rowStateForFailure(snapshot, device::domain::ErrorCode::TftInit);
    }

    static DiagnosticsRowState buttonsRowState(const device::domain::DiagnosticsSnapshot& snapshot) {
        return snapshot.buttonsOk ? DiagnosticsRowState::Ok : rowStateForFailure(snapshot, device::domain::ErrorCode::ButtonsInit);
    }

    static DiagnosticsRowState pn532RowState(const device::domain::DiagnosticsSnapshot& snapshot) {
        if (snapshot.pn532Ok) {
            return DiagnosticsRowState::Ok;
        }
        if (
            snapshot.failureCode == device::domain::ErrorCode::Pn532Init ||
            snapshot.failureCode == device::domain::ErrorCode::Pn532Firmware
        ) {
            return DiagnosticsRowState::Fail;
        }
        return DiagnosticsRowState::Pending;
    }

    static DiagnosticsRowState sdRowState(const device::domain::DiagnosticsSnapshot& snapshot) {
        if (snapshot.sdOk) {
            return DiagnosticsRowState::Ok;
        }
        if (
            snapshot.failureCode == device::domain::ErrorCode::SdMount ||
            snapshot.failureCode == device::domain::ErrorCode::SdReadWrite
        ) {
            return DiagnosticsRowState::Fail;
        }
        return DiagnosticsRowState::Pending;
    }

    static DiagnosticsRowState dbRowState(const device::domain::DiagnosticsSnapshot& snapshot) {
        if (snapshot.dbOk) {
            return DiagnosticsRowState::Ok;
        }
        if (
            snapshot.failureCode == device::domain::ErrorCode::DbInit ||
            snapshot.failureCode == device::domain::ErrorCode::DbWrite
        ) {
            return DiagnosticsRowState::Fail;
        }
        return DiagnosticsRowState::Pending;
    }

    static DiagnosticsRowState ethernetRowState(const device::domain::DiagnosticsSnapshot& snapshot) {
        if (snapshot.ethernetOk) {
            return DiagnosticsRowState::Ok;
        }
        // Only mark Fail once DB passed (meaning ETH probe was actually attempted)
        if (snapshot.dbOk) {
            return DiagnosticsRowState::Fail;
        }
        return DiagnosticsRowState::Pending;
    }

    static std::size_t clampDetailOffset(std::size_t requestedOffset, std::size_t detailCount) {
        if (detailCount <= device::config::kVisibleDetailLines) {
            return 0;
        }
        const auto maxOffset = detailCount - device::config::kVisibleDetailLines;
        return requestedOffset > maxOffset ? maxOffset : requestedOffset;
    }

private:
    static DiagnosticsRowState rowStateForFailure(
        const device::domain::DiagnosticsSnapshot& snapshot,
        device::domain::ErrorCode componentFailure
    ) {
        if (snapshot.failureCode == componentFailure) {
            return DiagnosticsRowState::Fail;
        }
        return DiagnosticsRowState::Pending;
    }
};

}  // namespace device::ui

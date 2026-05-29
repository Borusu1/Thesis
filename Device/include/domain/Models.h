#pragma once

#include <array>
#include <cstdint>

#include "config/DeviceConfig.h"
#include "core/FixedString.h"

namespace device::domain {

enum class ErrorCode : uint8_t {
    None,
    TftInit,
    ButtonsInit,
    Pn532Init,
    Pn532Firmware,
    NfcReadError,
    NfcUnsupportedPayload,
    NfcWriteError,
    NfcVerifyError,
    SdMount,
    SdReadWrite,
    DbInit,
    DbWrite,
    Auth,
    Network,
    Nfc,
    Sd,
    Validation,
    SyncConflict,
};

enum class OperationType : uint8_t {
    Receipt,
    ShipmentPartial,
    ShipmentFull,
};

enum class InventoryTagStatus : uint8_t {
    Unknown,
    Free,
    Assigned,
    NeedsSync,
};

enum class InventoryOperationType : uint8_t {
    ProvisionTag,
    AssignTag,
    ReceiptUnit,
    ShipmentPartialUnit,
    ShipmentFullUnit,
};

enum class InventoryUnitState : uint8_t {
    None,
    Active,
    Shipped,
};

enum class NfcPayloadKind : uint8_t {
    None,
    Blank,
    NdefUuid,
    LegacyRaw32,
    Unsupported,
    ReadError,
};

enum class ProvisionStage : uint8_t {
    WaitingForTag,
    Writing,
    Verifying,
    Success,
    Failure,
};

enum class AssignStage : uint8_t {
    WaitingForTag,
    Assigning,
    Success,
    Failure,
};

enum class ReceiptStage : uint8_t {
    SelectProduct,
    SkuInput,
    QuantityInput,
    WaitingForTag,
    Success,
    Failure,
};

enum class ShipmentStage : uint8_t {
    WaitingForTag,
    QuantityInput,
    Success,
    Failure,
};

enum class DeviceEventType : uint8_t {
    BootStarted,
    BootCompleted,
    DiagnosticsFailed,
    ModeEntered,
    TagRead,
    TagRemoved,
    ProvisionStarted,
    ProvisionSucceeded,
    ProvisionFailed,
    UserResetScan,
};

enum class DeviceEventSeverity : uint8_t {
    Info,
    Warn,
    Error,
};

struct ProductSummary {
    uint32_t productId = 0;
    uint32_t sku = 0;
    FixedString<32> name;
};

struct ProductRecord {
    bool exists = false;
    bool isActive = false;
    uint32_t productId = 0;
    uint32_t sku = 0;
    FixedString<32> name;

    void clear() {
        exists = false;
        isActive = false;
        productId = 0;
        sku = 0;
        name.clear();
    }
};

struct OfflineOperationRecord {
    OperationType operationType = OperationType::Receipt;
    uint32_t productId = 0;
    uint32_t quantity = 0;
    FixedString<64> clientOperationId;
    FixedString<36> tagUuid;
    FixedString<64> chipUidHex;
    FixedString<48> note;
};

struct DiagnosticsSnapshot {
    bool displayOk = false;
    bool buttonsOk = false;
    bool pn532Ok = false;
    bool sdOk = false;
    bool dbOk = false;
    bool ethernetOk = false;
    ErrorCode failureCode = ErrorCode::None;
    std::array<FixedString<48>, device::config::kMaxDetailLines> detailLines {};
    std::size_t detailCount = 0;

    void clearDetails() {
        for (auto& line : detailLines) {
            line.clear();
        }
        detailCount = 0;
    }

    bool addDetailLine(const char* text) {
        if (detailCount >= detailLines.size()) {
            return false;
        }
        if (!detailLines[detailCount].assign(text)) {
            return false;
        }
        ++detailCount;
        return true;
    }
};

struct NfcTagSnapshot {
    bool present = false;
    NfcPayloadKind payloadKind = NfcPayloadKind::None;
    ErrorCode errorCode = ErrorCode::None;
    FixedString<16> tagFamily;
    FixedString<32> uidHex;
    FixedString<36> uuid;
    FixedString<32> legacyRaw;
    std::array<FixedString<48>, device::config::kMaxDetailLines> detailLines {};
    std::size_t detailCount = 0;

    void clear() {
        present = false;
        payloadKind = NfcPayloadKind::None;
        errorCode = ErrorCode::None;
        tagFamily.clear();
        uidHex.clear();
        uuid.clear();
        legacyRaw.clear();
        clearDetails();
    }

    void clearDetails() {
        for (auto& line : detailLines) {
            line.clear();
        }
        detailCount = 0;
    }

    bool addDetailLine(const char* text) {
        if (detailCount >= detailLines.size()) {
            return false;
        }
        if (!detailLines[detailCount].assign(text)) {
            return false;
        }
        ++detailCount;
        return true;
    }
};

struct ProvisionSnapshot {
    bool inProgress = false;
    bool success = false;
    ProvisionStage stage = ProvisionStage::WaitingForTag;
    ErrorCode errorCode = ErrorCode::None;
    FixedString<32> uidHex;
    FixedString<36> writtenUuid;
    std::array<FixedString<48>, device::config::kMaxDetailLines> detailLines {};
    std::size_t detailCount = 0;

    void clear() {
        inProgress = false;
        success = false;
        stage = ProvisionStage::WaitingForTag;
        errorCode = ErrorCode::None;
        uidHex.clear();
        writtenUuid.clear();
        clearDetails();
    }

    void clearDetails() {
        for (auto& line : detailLines) {
            line.clear();
        }
        detailCount = 0;
    }

    bool addDetailLine(const char* text) {
        if (detailCount >= detailLines.size()) {
            return false;
        }
        if (!detailLines[detailCount].assign(text)) {
            return false;
        }
        ++detailCount;
        return true;
    }
};

struct DatabaseStatusSnapshot {
    bool ready = false;
    uint32_t bootId = 0;
    ErrorCode lastError = ErrorCode::None;
};

struct ProductCatalogStatus {
    bool loaded = false;
    uint32_t totalProducts = 0;
    FixedString<48> lastError;

    void clear() {
        loaded = false;
        totalProducts = 0;
        lastError.clear();
    }
};

struct DeviceBootstrapConfig {
    bool valid = false;
    FixedString<64> wifiSsid;
    FixedString<64> wifiPassword;
    FixedString<160> apiBaseUrl;
    FixedString<64> deviceId;
    uint32_t productSyncIntervalMs = 300000;

    void clear() {
        valid = false;
        wifiSsid.clear();
        wifiPassword.clear();
        apiBaseUrl.clear();
        deviceId.clear();
        productSyncIntervalMs = 300000;
    }
};

struct NetworkStatusSnapshot {
    bool configLoaded = false;
    bool ethernetConnected = false;
    bool wifiConnected = false;  // true = any network connected (ETH or WiFi)
    bool deviceAuthenticated = false;
    bool productSyncOk = false;
    bool operatorAuthenticated = false;
    bool syncInProgress = false;
    uint32_t lastSyncAtMs = 0;
    uint32_t pendingSyncCount = 0;
    FixedString<48> detail;

    void clear() {
        configLoaded = false;
        ethernetConnected = false;
        wifiConnected = false;
        deviceAuthenticated = false;
        productSyncOk = false;
        operatorAuthenticated = false;
        syncInProgress = false;
        lastSyncAtMs = 0;
        pendingSyncCount = 0;
        detail.clear();
    }
};

struct OperatorSessionSnapshot {
    bool authenticated = false;
    bool loginInProgress = false;
    uint32_t lastActivityAtMs = 0;
    FixedString<32> badgeUid;
    FixedString<256> operatorToken;
    FixedString<48> operatorName;
    FixedString<16> operatorRole;
    FixedString<48> lastError;

    void clear() {
        authenticated = false;
        loginInProgress = false;
        lastActivityAtMs = 0;
        badgeUid.clear();
        operatorToken.clear();
        operatorName.clear();
        operatorRole.clear();
        lastError.clear();
    }
};

struct SyncStatusSnapshot {
    bool inProgress = false;
    uint32_t pendingCount = 0;
    uint32_t lastSyncAtMs = 0;
    uint32_t lastSyncedCount = 0;
    FixedString<48> lastError;

    void clear() {
        inProgress = false;
        pendingCount = 0;
        lastSyncAtMs = 0;
        lastSyncedCount = 0;
        lastError.clear();
    }
};

struct InventoryTagRecord {
    bool exists = false;
    InventoryTagStatus status = InventoryTagStatus::Unknown;
    bool pendingSync = false;
    uint32_t assignmentCount = 0;
    uint32_t lastSeenAtMs = 0;
    FixedString<36> tagUuid;
    FixedString<32> uidHex;
    FixedString<24> lastResultCode;

    void clear() {
        exists = false;
        status = InventoryTagStatus::Unknown;
        pendingSync = false;
        assignmentCount = 0;
        lastSeenAtMs = 0;
        tagUuid.clear();
        uidHex.clear();
        lastResultCode.clear();
    }
};

struct InventoryUnitRecord {
    bool exists = false;
    bool provisionedOnly = false;
    bool pendingSync = false;
    uint32_t sku = 0;
    uint32_t quantityInitial = 0;
    uint32_t quantityCurrent = 0;
    uint32_t receivedAtMs = 0;
    uint32_t shippedAtMs = 0;
    InventoryUnitState state = InventoryUnitState::None;
    FixedString<36> tagUuid;
    FixedString<32> uidHex;
    FixedString<32> productName;
    FixedString<24> lastResultCode;

    void clear() {
        exists = false;
        provisionedOnly = false;
        pendingSync = false;
        sku = 0;
        quantityInitial = 0;
        quantityCurrent = 0;
        receivedAtMs = 0;
        shippedAtMs = 0;
        state = InventoryUnitState::None;
        tagUuid.clear();
        uidHex.clear();
        productName.clear();
        lastResultCode.clear();
    }
};

struct LookupSnapshot {
    bool present = false;
    bool knownLocally = false;
    bool provisionedOnly = false;
    bool pendingSync = false;
    NfcPayloadKind payloadKind = NfcPayloadKind::None;
    InventoryTagStatus localStatus = InventoryTagStatus::Unknown;
    InventoryUnitState unitState = InventoryUnitState::None;
    ErrorCode errorCode = ErrorCode::None;
    uint32_t assignmentCount = 0;
    uint32_t sku = 0;
    uint32_t quantityInitial = 0;
    uint32_t quantityCurrent = 0;
    FixedString<32> uidHex;
    FixedString<36> uuid;
    FixedString<32> productName;
    FixedString<32> legacyRaw;
    std::array<FixedString<48>, device::config::kMaxDetailLines> detailLines {};
    std::size_t detailCount = 0;

    void clear() {
        present = false;
        knownLocally = false;
        provisionedOnly = false;
        pendingSync = false;
        payloadKind = NfcPayloadKind::None;
        localStatus = InventoryTagStatus::Unknown;
        unitState = InventoryUnitState::None;
        errorCode = ErrorCode::None;
        assignmentCount = 0;
        sku = 0;
        quantityInitial = 0;
        quantityCurrent = 0;
        uidHex.clear();
        uuid.clear();
        productName.clear();
        legacyRaw.clear();
        clearDetails();
    }

    void clearDetails() {
        for (auto& line : detailLines) {
            line.clear();
        }
        detailCount = 0;
    }

    bool addDetailLine(const char* text) {
        if (detailCount >= detailLines.size()) {
            return false;
        }
        if (!detailLines[detailCount].assign(text)) {
            return false;
        }
        ++detailCount;
        return true;
    }
};

struct AssignSnapshot {
    bool inProgress = false;
    bool success = false;
    bool pendingSync = false;
    AssignStage stage = AssignStage::WaitingForTag;
    InventoryTagStatus localStatus = InventoryTagStatus::Unknown;
    ErrorCode errorCode = ErrorCode::None;
    uint32_t assignmentCount = 0;
    FixedString<32> uidHex;
    FixedString<36> tagUuid;
    std::array<FixedString<48>, device::config::kMaxDetailLines> detailLines {};
    std::size_t detailCount = 0;

    void clear() {
        inProgress = false;
        success = false;
        pendingSync = false;
        stage = AssignStage::WaitingForTag;
        localStatus = InventoryTagStatus::Unknown;
        errorCode = ErrorCode::None;
        assignmentCount = 0;
        uidHex.clear();
        tagUuid.clear();
        clearDetails();
    }

    void clearDetails() {
        for (auto& line : detailLines) {
            line.clear();
        }
        detailCount = 0;
    }

    bool addDetailLine(const char* text) {
        if (detailCount >= detailLines.size()) {
            return false;
        }
        if (!detailLines[detailCount].assign(text)) {
            return false;
        }
        ++detailCount;
        return true;
    }
};

struct ReceiptSnapshot {
    bool inProgress = false;
    bool success = false;
    bool pendingSync = false;
    bool skuMode = false;
    bool productsLoaded = false;
    ReceiptStage stage = ReceiptStage::SelectProduct;
    ErrorCode errorCode = ErrorCode::None;
    uint32_t selectedIndex = 0;
    uint32_t skuCursor = 0;
    uint32_t enteredSku = 0;
    uint32_t quantityCursor = 0;
    uint32_t enteredQuantity = 0;
    bool quantityEditing = false;
    FixedString<32> uidHex;
    FixedString<36> tagUuid;
    ProductRecord selectedProduct;
    std::array<ProductRecord, device::config::kMaxRecentProducts> products {};
    std::array<FixedString<48>, device::config::kMaxDetailLines> detailLines {};
    std::size_t productCount = 0;
    std::size_t detailCount = 0;

    void clear() {
        inProgress = false;
        success = false;
        pendingSync = false;
        skuMode = false;
        productsLoaded = false;
        stage = ReceiptStage::SelectProduct;
        errorCode = ErrorCode::None;
        selectedIndex = 0;
        skuCursor = 0;
        enteredSku = 0;
        quantityCursor = 0;
        enteredQuantity = 0;
        quantityEditing = false;
        uidHex.clear();
        tagUuid.clear();
        selectedProduct.clear();
        for (auto& product : products) {
            product.clear();
        }
        productCount = 0;
        clearDetails();
    }

    void clearDetails() {
        for (auto& line : detailLines) {
            line.clear();
        }
        detailCount = 0;
    }

    bool addDetailLine(const char* text) {
        if (detailCount >= detailLines.size()) {
            return false;
        }
        if (!detailLines[detailCount].assign(text)) {
            return false;
        }
        ++detailCount;
        return true;
    }
};

struct ShipmentSnapshot {
    bool inProgress = false;
    bool success = false;
    bool pendingSync = false;
    ShipmentStage stage = ShipmentStage::WaitingForTag;
    ErrorCode errorCode = ErrorCode::None;
    uint32_t sku = 0;
    uint32_t quantityCurrent = 0;
    uint32_t quantityToShip = 0;
    uint32_t quantityCursor = 0;
    bool quantityEditing = false;
    FixedString<32> uidHex;
    FixedString<36> tagUuid;
    FixedString<32> productName;
    std::array<FixedString<48>, device::config::kMaxDetailLines> detailLines {};
    std::size_t detailCount = 0;

    void clear() {
        inProgress = false;
        success = false;
        pendingSync = false;
        stage = ShipmentStage::WaitingForTag;
        errorCode = ErrorCode::None;
        sku = 0;
        quantityCurrent = 0;
        quantityToShip = 0;
        quantityCursor = 0;
        quantityEditing = false;
        uidHex.clear();
        tagUuid.clear();
        productName.clear();
        clearDetails();
    }

    void clearDetails() {
        for (auto& line : detailLines) {
            line.clear();
        }
        detailCount = 0;
    }

    bool addDetailLine(const char* text) {
        if (detailCount >= detailLines.size()) {
            return false;
        }
        if (!detailLines[detailCount].assign(text)) {
            return false;
        }
        ++detailCount;
        return true;
    }
};

struct InventoryOperationRecord {
    uint32_t rowId = 0;
    uint32_t bootId = 0;
    uint32_t createdAtMs = 0;
    uint32_t productId = 0;
    uint32_t sku = 0;
    uint32_t quantity = 1;
    uint32_t syncAttemptCount = 0;
    uint32_t syncedAtMs = 0;
    InventoryOperationType operationType = InventoryOperationType::ProvisionTag;
    InventoryTagStatus localStatus = InventoryTagStatus::Unknown;
    bool pendingSync = true;
    FixedString<64> clientOperationId;
    FixedString<36> tagUuid;
    FixedString<32> uidHex;
    FixedString<24> resultCode;
    FixedString<64> lastSyncError;
    FixedString<64> message;
};

struct DeviceEventRecord {
    uint32_t bootId = 0;
    uint32_t createdAtMs = 0;
    DeviceEventType eventType = DeviceEventType::BootStarted;
    DeviceEventSeverity severity = DeviceEventSeverity::Info;
    FixedString<24> screen;
    FixedString<32> uidHex;
    FixedString<36> tagUuid;
    FixedString<24> payloadKind;
    FixedString<24> resultCode;
    FixedString<64> message;
};

inline const char* toString(ErrorCode code) {
    switch (code) {
        case ErrorCode::TftInit:
            return "TFT_INIT";
        case ErrorCode::ButtonsInit:
            return "BUTTONS_INIT";
        case ErrorCode::Pn532Init:
            return "PN532_INIT";
        case ErrorCode::Pn532Firmware:
            return "PN532_FW";
        case ErrorCode::NfcReadError:
            return "NFC_READ";
        case ErrorCode::NfcUnsupportedPayload:
            return "NFC_UNSUPPORTED";
        case ErrorCode::NfcWriteError:
            return "NFC_WRITE";
        case ErrorCode::NfcVerifyError:
            return "NFC_VERIFY";
        case ErrorCode::SdMount:
            return "SD_MOUNT";
        case ErrorCode::SdReadWrite:
            return "SD_RW";
        case ErrorCode::DbInit:
            return "DB_INIT";
        case ErrorCode::DbWrite:
            return "DB_WRITE";
        case ErrorCode::Auth:
            return "AUTH";
        case ErrorCode::Network:
            return "NETWORK";
        case ErrorCode::Nfc:
            return "NFC";
        case ErrorCode::Sd:
            return "SD";
        case ErrorCode::Validation:
            return "VALIDATION";
        case ErrorCode::SyncConflict:
            return "SYNC_CONFLICT";
        case ErrorCode::None:
        default:
            return "NONE";
    }
}

inline const char* toString(NfcPayloadKind kind) {
    switch (kind) {
        case NfcPayloadKind::Blank:
            return "Blank";
        case NfcPayloadKind::NdefUuid:
            return "NdefUuid";
        case NfcPayloadKind::LegacyRaw32:
            return "LegacyRaw32";
        case NfcPayloadKind::Unsupported:
            return "Unsupported";
        case NfcPayloadKind::ReadError:
            return "ReadError";
        case NfcPayloadKind::None:
        default:
            return "None";
    }
}

inline const char* toString(InventoryTagStatus status) {
    switch (status) {
        case InventoryTagStatus::Free:
            return "Free";
        case InventoryTagStatus::Assigned:
            return "Assigned";
        case InventoryTagStatus::NeedsSync:
            return "NeedsSync";
        case InventoryTagStatus::Unknown:
        default:
            return "Unknown";
    }
}

inline const char* toString(InventoryOperationType type) {
    switch (type) {
        case InventoryOperationType::ProvisionTag:
            return "provision_tag";
        case InventoryOperationType::AssignTag:
            return "assign_tag";
        case InventoryOperationType::ReceiptUnit:
            return "receipt_unit";
        case InventoryOperationType::ShipmentPartialUnit:
            return "shipment_partial_unit";
        case InventoryOperationType::ShipmentFullUnit:
            return "shipment_full_unit";
        default:
            return "unknown";
    }
}

inline const char* toSyncOperationType(InventoryOperationType type) {
    switch (type) {
        case InventoryOperationType::ReceiptUnit:
            return "receipt";
        case InventoryOperationType::ShipmentPartialUnit:
            return "shipment_partial";
        case InventoryOperationType::ShipmentFullUnit:
            return "shipment_full";
        default:
            return "";
    }
}

inline const char* toString(InventoryUnitState state) {
    switch (state) {
        case InventoryUnitState::Active:
            return "Active";
        case InventoryUnitState::Shipped:
            return "Shipped";
        case InventoryUnitState::None:
        default:
            return "None";
    }
}

inline const char* toString(DeviceEventType type) {
    switch (type) {
        case DeviceEventType::BootStarted:
            return "boot_started";
        case DeviceEventType::BootCompleted:
            return "boot_completed";
        case DeviceEventType::DiagnosticsFailed:
            return "diagnostics_failed";
        case DeviceEventType::ModeEntered:
            return "mode_entered";
        case DeviceEventType::TagRead:
            return "tag_read";
        case DeviceEventType::TagRemoved:
            return "tag_removed";
        case DeviceEventType::ProvisionStarted:
            return "provision_started";
        case DeviceEventType::ProvisionSucceeded:
            return "provision_succeeded";
        case DeviceEventType::ProvisionFailed:
            return "provision_failed";
        case DeviceEventType::UserResetScan:
            return "user_reset_scan";
        default:
            return "unknown";
    }
}

inline const char* toString(DeviceEventSeverity severity) {
    switch (severity) {
        case DeviceEventSeverity::Warn:
            return "WARN";
        case DeviceEventSeverity::Error:
            return "ERROR";
        case DeviceEventSeverity::Info:
        default:
            return "INFO";
    }
}

}  // namespace device::domain

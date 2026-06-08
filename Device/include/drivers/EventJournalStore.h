#pragma once

#include <sqlite3.h>

#include <SD.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <optional>
#include <string_view>

#include "config/DeviceConfig.h"
#include "domain/Models.h"

namespace device::drivers {

class EventJournalStore {
public:
    bool begin(device::domain::DatabaseStatusSnapshot& status) {
        if (status_.ready && db_ != nullptr) {
            status = status_;
            return true;
        }

        sqlite3_initialize();
        if (!applyDatabaseResetNonce()) {
            setInitError(status, "db reset failed");
            return false;
        }
        if (sqlite3_open(device::config::kDatabasePath, &db_) != SQLITE_OK) {
            setInitError(status, "sqlite open failed");
            return false;
        }

        if (!exec("PRAGMA journal_mode=DELETE;") || !exec("PRAGMA synchronous=FULL;")) {
            setInitError(status, "pragma failed");
            return false;
        }

        if (!ensureBaseSchema()) {
            setInitError(status, "base schema failed");
            return false;
        }

        auto schemaVersion = queryMetaInt("schema_version");
        if (!schemaVersion.has_value()) {
            setInitError(status, "schema version missing");
            return false;
        }

        if (schemaVersion.value() < static_cast<int>(device::config::kDatabaseSchemaVersion)) {
            if (!migrateSchema(schemaVersion.value())) {
                setInitError(status, "schema migration failed");
                return false;
            }
            schemaVersion = queryMetaInt("schema_version");
        }

        if (!schemaVersion.has_value() ||
            schemaVersion.value() != static_cast<int>(device::config::kDatabaseSchemaVersion)) {
            setInitError(status, "schema version mismatch");
            return false;
        }

        if (!ensureWarehouseSchema()) {
            setInitError(status, "warehouse schema failed");
            return false;
        }

        if (!ensureWarehouseCompatibility()) {
            setInitError(status, "warehouse compat failed");
            return false;
        }

        const auto bootCounter = queryMetaInt("boot_counter");
        if (!bootCounter.has_value()) {
            setInitError(status, "boot counter read failed");
            return false;
        }

        const uint32_t nextBootId = static_cast<uint32_t>(bootCounter.value() + 1);
        if (!setMetaInt("boot_counter", nextBootId)) {
            setInitError(status, "boot counter write failed");
            return false;
        }

        status_.ready = true;
        status_.bootId = nextBootId;
        status_.lastError = device::domain::ErrorCode::None;
        if (resetMarkerNeedsPersist_ && !persistDatabaseResetNonce()) {
            setInitError(status, "db reset mark failed");
            return false;
        }
        status = status_;
        return true;
    }

    bool replaceProducts(
        const device::domain::ProductRecord* products,
        std::size_t count,
        device::domain::ProductCatalogStatus& status
    ) {
        status.clear();
        if (!readyForWrite()) {
            status.lastError.assign("DB unavailable");
            return false;
        }

        const uint32_t prevGen = static_cast<uint32_t>(queryMetaInt("product_sync_gen").value_or(0));

        if (prevGen == 0 && !exec("UPDATE products SET updated_at_ms = 0;")) {
            status.lastError.assign("Product migrate failed");
            return false;
        }
        const uint32_t syncGen = prevGen + 1;
        if (!setMetaInt("product_sync_gen", syncGen)) {
            status.lastError.assign("Sync gen write failed");
            return false;
        }

        for (std::size_t index = 0; index < count; ++index) {
            if (!products[index].exists) {
                continue;
            }
            if (!upsertProduct(products[index].productId, products[index].sku, products[index].name.c_str(), products[index].isActive, syncGen)) {
                status.lastError.assign("Product insert failed");
                return false;
            }
            ++status.totalProducts;
        }

        sqlite3_stmt* deactivateStmt = nullptr;
        static constexpr const char* kDeactivateSql =
            "UPDATE products SET is_active = 0 WHERE updated_at_ms < ?;";
        if (sqlite3_prepare_v2(db_, kDeactivateSql, -1, &deactivateStmt, nullptr) != SQLITE_OK) {
            setWriteError("prepare deactivate stale products");
            status.lastError.assign("Product cleanup failed");
            return false;
        }
        sqlite3_bind_int(deactivateStmt, 1, static_cast<int>(syncGen));
        if (!finalizeStep(deactivateStmt)) {
            status.lastError.assign("Product cleanup failed");
            return false;
        }

        status.loaded = status.totalProducts > 0;
        if (!status.loaded) {
            status.lastError.assign("No products loaded");
        }
        return true;
    }

    bool refreshProductCatalogStatus(device::domain::ProductCatalogStatus& status) {
        status.clear();
        if (!status_.ready || db_ == nullptr) {
            status.lastError.assign("DB unavailable");
            return false;
        }

        const auto count = queryCount("SELECT COUNT(*) FROM products WHERE is_active = 1;");
        if (!count.has_value()) {
            status.lastError.assign("Product count failed");
            return false;
        }

        status.totalProducts = static_cast<uint32_t>(count.value());
        status.loaded = status.totalProducts > 0;
        if (!status.loaded) {
            status.lastError.assign("No products loaded");
        }
        return true;
    }

    template <std::size_t N>
    bool listProducts(std::array<device::domain::ProductRecord, N>& products, std::size_t& count) {
        count = 0;
        for (auto& product : products) {
            product.clear();
        }

        if (!status_.ready || db_ == nullptr) {
            return false;
        }

        sqlite3_stmt* stmt = nullptr;
        static constexpr const char* kSql =
            "SELECT product_id, sku, name, is_active FROM products "
            "WHERE is_active = 1 ORDER BY sku ASC LIMIT ?;";
        if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
            setWriteError("prepare listProducts");
            return false;
        }

        sqlite3_bind_int(stmt, 1, static_cast<int>(N));
        while (count < N && sqlite3_step(stmt) == SQLITE_ROW) {
            auto& product = products[count++];
            product.exists = true;
            product.productId = static_cast<uint32_t>(sqlite3_column_int(stmt, 0));
            product.sku = static_cast<uint32_t>(sqlite3_column_int(stmt, 1));
            readColumnText(stmt, 2, product.name);
            product.isActive = sqlite3_column_int(stmt, 3) != 0;
        }
        sqlite3_finalize(stmt);
        return true;
    }

    bool findProductBySku(uint32_t sku, device::domain::ProductRecord& record) {
        record.clear();
        if (!status_.ready || db_ == nullptr) {
            return false;
        }

        sqlite3_stmt* stmt = nullptr;
        static constexpr const char* kSql =
            "SELECT product_id, sku, name, is_active FROM products WHERE sku = ? LIMIT 1;";
        if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
            setWriteError("prepare findProductBySku");
            return false;
        }

        sqlite3_bind_int(stmt, 1, static_cast<int>(sku));
        const int rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            record.exists = true;
            record.productId = static_cast<uint32_t>(sqlite3_column_int(stmt, 0));
            record.sku = static_cast<uint32_t>(sqlite3_column_int(stmt, 1));
            readColumnText(stmt, 2, record.name);
            record.isActive = sqlite3_column_int(stmt, 3) != 0;
            sqlite3_finalize(stmt);
            return true;
        }

        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE;
    }

    bool logEvent(const device::domain::DeviceEventRecord& event) {
        if (!readyForWrite()) {
            return false;
        }

        static constexpr const char* kSql =
            "INSERT INTO device_events "
            "(boot_id, created_at_ms, event_type, severity, screen, uid_hex, tag_uuid, payload_kind, result_code, message) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
            setWriteError("prepare logEvent");
            return false;
        }

        sqlite3_bind_int(stmt, 1, static_cast<int>(event.bootId));
        sqlite3_bind_int(stmt, 2, static_cast<int>(event.createdAtMs));
        sqlite3_bind_text(stmt, 3, device::domain::toString(event.eventType), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, device::domain::toString(event.severity), -1, SQLITE_STATIC);
        bindFixedString(stmt, 5, event.screen);
        bindFixedString(stmt, 6, event.uidHex);
        bindFixedString(stmt, 7, event.tagUuid);
        bindFixedString(stmt, 8, event.payloadKind);
        bindFixedString(stmt, 9, event.resultCode);
        bindFixedString(stmt, 10, event.message);
        return finalizeStep(stmt);
    }

    bool lookupInventoryTag(std::string_view tagUuid, device::domain::InventoryTagRecord& record) {
        record.clear();
        if (!status_.ready || db_ == nullptr) {
            return false;
        }

        sqlite3_stmt* stmt = nullptr;
        static constexpr const char* kSql =
            "SELECT uid_hex, local_status, assignment_count, last_seen_at_ms, pending_sync, last_result_code "
            "FROM inventory_tags WHERE tag_uuid = ?;";
        if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
            setWriteError("prepare lookupInventoryTag");
            return false;
        }

        bindTextView(stmt, 1, tagUuid);
        const int rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            record.exists = true;
            record.tagUuid.assign(tagUuid);
            readColumnText(stmt, 0, record.uidHex);
            record.status = inventoryStatusFromText(columnText(stmt, 1));
            record.assignmentCount = static_cast<uint32_t>(sqlite3_column_int(stmt, 2));
            record.lastSeenAtMs = static_cast<uint32_t>(sqlite3_column_int(stmt, 3));
            record.pendingSync = sqlite3_column_int(stmt, 4) != 0;
            readColumnText(stmt, 5, record.lastResultCode);
            sqlite3_finalize(stmt);
            return true;
        }

        sqlite3_finalize(stmt);
        if (rc == SQLITE_DONE) {
            return true;
        }
        setWriteError("step lookupInventoryTag");
        return false;
    }

    bool touchInventoryTag(std::string_view tagUuid, std::string_view uidHex, uint32_t nowMs) {
        if (!readyForWrite()) {
            return false;
        }

        sqlite3_stmt* stmt = nullptr;
        static constexpr const char* kSql =
            "UPDATE inventory_tags "
            "SET uid_hex = ?, last_seen_at_ms = ?, updated_at_ms = ? "
            "WHERE tag_uuid = ?;";
        if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
            setWriteError("prepare touchInventoryTag");
            return false;
        }

        bindTextView(stmt, 1, uidHex);
        sqlite3_bind_int(stmt, 2, static_cast<int>(nowMs));
        sqlite3_bind_int(stmt, 3, static_cast<int>(nowMs));
        bindTextView(stmt, 4, tagUuid);
        return finalizeStep(stmt);
    }

    bool storeProvisionedTag(
        std::string_view uidHex,
        std::string_view tagUuid,
        uint32_t nowMs,
        uint32_t bootId,
        device::domain::InventoryTagRecord& record
    ) {
        if (!readyForWrite()) {
            return false;
        }

        device::domain::InventoryTagRecord existingRecord;
        if (!lookupInventoryTag(tagUuid, existingRecord)) {
            return false;
        }

        sqlite3_stmt* stmt = nullptr;
        static constexpr const char* kSql =
            "INSERT OR REPLACE INTO inventory_tags "
            "(tag_uuid, uid_hex, local_status, assignment_count, last_seen_at_ms, created_at_ms, updated_at_ms, pending_sync, last_result_code) "
            "VALUES (?, ?, 'Free', 0, ?, ?, ?, 1, 'NONE');";
        if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
            setWriteError("prepare storeProvisionedTag");
            return false;
        }

        bindTextView(stmt, 1, tagUuid);
        bindTextView(stmt, 2, uidHex);
        sqlite3_bind_int(stmt, 3, static_cast<int>(nowMs));
        sqlite3_bind_int(stmt, 4, static_cast<int>(existingRecord.exists ? existingRecord.lastSeenAtMs : nowMs));
        sqlite3_bind_int(stmt, 5, static_cast<int>(nowMs));
        if (!finalizeStep(stmt)) {
            return false;
        }

        device::domain::InventoryOperationRecord operation;
        operation.bootId = bootId;
        operation.createdAtMs = nowMs;
        operation.sku = 0;
        operation.operationType = device::domain::InventoryOperationType::ProvisionTag;
        operation.localStatus = device::domain::InventoryTagStatus::Free;
        operation.pendingSync = true;
        assignClientOperationId(operation);
        operation.uidHex.assign(uidHex);
        operation.tagUuid.assign(tagUuid);
        operation.message.assign("local provision");
        if (!recordInventoryOperation(operation)) {
            return false;
        }

        return lookupInventoryTag(tagUuid, record);
    }

    bool lookupInventoryUnitByTagUuid(std::string_view tagUuid, device::domain::InventoryUnitRecord& record) {
        record.clear();
        if (!status_.ready || db_ == nullptr) {
            return false;
        }

        sqlite3_stmt* stmt = nullptr;
        static constexpr const char* kSql =
            "SELECT u.uid_hex, u.sku, u.quantity_initial, u.quantity_current, u.state, u.received_at_ms, u.shipped_at_ms, u.pending_sync, "
            "p.name "
            "FROM inventory_units u "
            "LEFT JOIN products p ON p.sku = u.sku "
            "WHERE u.tag_uuid = ? LIMIT 1;";
        if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
            setWriteError("prepare lookupInventoryUnit");
            return false;
        }

        bindTextView(stmt, 1, tagUuid);
        const int rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            record.exists = true;
            record.tagUuid.assign(tagUuid);
            readColumnText(stmt, 0, record.uidHex);
            record.sku = static_cast<uint32_t>(sqlite3_column_int(stmt, 1));
            record.quantityInitial = static_cast<uint32_t>(sqlite3_column_int(stmt, 2));
            record.quantityCurrent = static_cast<uint32_t>(sqlite3_column_int(stmt, 3));
            record.state = inventoryUnitStateFromText(columnText(stmt, 4));
            record.receivedAtMs = static_cast<uint32_t>(sqlite3_column_int(stmt, 5));
            record.shippedAtMs = static_cast<uint32_t>(sqlite3_column_int(stmt, 6));
            record.pendingSync = sqlite3_column_int(stmt, 7) != 0;
            readColumnText(stmt, 8, record.productName);
            sqlite3_finalize(stmt);
            return true;
        }
        sqlite3_finalize(stmt);
        if (rc != SQLITE_DONE) {
            setWriteError("step lookupInventoryUnit");
            return false;
        }

        device::domain::InventoryTagRecord tagRecord;
        if (!lookupInventoryTag(tagUuid, tagRecord)) {
            return false;
        }
        if (tagRecord.exists) {
            record.provisionedOnly = true;
            record.tagUuid.assign(tagUuid);
            record.uidHex = tagRecord.uidHex;
            record.pendingSync = tagRecord.pendingSync;
        }
        return true;
    }

    bool createInventoryUnit(
        std::string_view uidHex,
        std::string_view tagUuid,
        uint32_t productId,
        uint32_t sku,
        uint32_t quantity,
        uint32_t nowMs,
        uint32_t bootId,
        device::domain::InventoryUnitRecord& record
    ) {
        if (!readyForWrite()) {
            return false;
        }
        if (quantity < device::config::kMinTagQuantity || quantity > device::config::kMaxTagQuantity) {
            setWriteError("invalid receipt quantity");
            return false;
        }

        sqlite3_stmt* stmt = nullptr;
        static constexpr const char* kSql =
            "INSERT OR REPLACE INTO inventory_units "
            "(tag_uuid, uid_hex, sku, quantity_initial, quantity_current, state, received_at_ms, shipped_at_ms, pending_sync) "
            "VALUES (?, ?, ?, ?, ?, 'Active', ?, 0, 1);";
        if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
            setWriteError("prepare createInventoryUnit");
            return false;
        }

        bindTextView(stmt, 1, tagUuid);
        bindTextView(stmt, 2, uidHex);
        sqlite3_bind_int(stmt, 3, static_cast<int>(sku));
        sqlite3_bind_int(stmt, 4, static_cast<int>(quantity));
        sqlite3_bind_int(stmt, 5, static_cast<int>(quantity));
        sqlite3_bind_int(stmt, 6, static_cast<int>(nowMs));
        if (!finalizeStep(stmt)) {
            return false;
        }

        if (!updateInventoryTagStatus(tagUuid, uidHex, "Assigned", nowMs, true)) {
            return false;
        }

        device::domain::InventoryOperationRecord operation;
        operation.bootId = bootId;
        operation.createdAtMs = nowMs;
        operation.productId = productId;
        operation.sku = sku;
        operation.quantity = quantity;
        operation.operationType = device::domain::InventoryOperationType::ReceiptUnit;
        operation.localStatus = device::domain::InventoryTagStatus::Assigned;
        operation.pendingSync = true;
        assignClientOperationId(operation);
        operation.uidHex.assign(uidHex);
        operation.tagUuid.assign(tagUuid);
        operation.message.assign("local receipt");
        if (!recordInventoryOperation(operation)) {
            return false;
        }

        return lookupInventoryUnitByTagUuid(tagUuid, record);
    }

    bool shipInventoryUnit(
        std::string_view uidHex,
        std::string_view tagUuid,
        uint32_t quantity,
        uint32_t nowMs,
        uint32_t bootId,
        device::domain::InventoryUnitRecord& record
    ) {
        if (!readyForWrite()) {
            return false;
        }
        if (quantity < device::config::kMinTagQuantity) {
            setWriteError("invalid shipment quantity");
            return false;
        }

        device::domain::InventoryUnitRecord currentRecord;
        if (!lookupInventoryUnitByTagUuid(tagUuid, currentRecord)) {
            return false;
        }
        if (!currentRecord.exists || currentRecord.state != device::domain::InventoryUnitState::Active) {
            setWriteError("shipInventoryUnit inactive");
            return false;
        }
        if (quantity > currentRecord.quantityCurrent) {
            setWriteError("shipment quantity exceeds current");
            return false;
        }

        const bool fullShipment = quantity == currentRecord.quantityCurrent;

        sqlite3_stmt* stmt = nullptr;
        static constexpr const char* kFullSql =
            "UPDATE inventory_units "
            "SET quantity_current = 0, state = 'Shipped', shipped_at_ms = ?, pending_sync = 1 "
            "WHERE tag_uuid = ? AND state = 'Active';";
        static constexpr const char* kPartialSql =
            "UPDATE inventory_units "
            "SET quantity_current = quantity_current - ?, pending_sync = 1 "
            "WHERE tag_uuid = ? AND state = 'Active';";
        if (sqlite3_prepare_v2(db_, fullShipment ? kFullSql : kPartialSql, -1, &stmt, nullptr) != SQLITE_OK) {
            setWriteError("prepare shipInventoryUnit");
            return false;
        }

        sqlite3_bind_int(stmt, 1, static_cast<int>(fullShipment ? nowMs : quantity));
        bindTextView(stmt, 2, tagUuid);
        if (!finalizeStep(stmt)) {
            return false;
        }

        if (!updateInventoryTagStatus(tagUuid, uidHex, "NeedsSync", nowMs, true)) {
            return false;
        }

        if (!lookupInventoryUnitByTagUuid(tagUuid, currentRecord)) {
            return false;
        }

        device::domain::InventoryOperationRecord operation;
        operation.bootId = bootId;
        operation.createdAtMs = nowMs;
        operation.productId = 0;
        operation.sku = currentRecord.sku;
        operation.quantity = quantity;
        operation.operationType = fullShipment
            ? device::domain::InventoryOperationType::ShipmentFullUnit
            : device::domain::InventoryOperationType::ShipmentPartialUnit;
        operation.localStatus = device::domain::InventoryTagStatus::NeedsSync;
        operation.pendingSync = true;
        assignClientOperationId(operation);
        operation.uidHex.assign(uidHex);
        operation.tagUuid.assign(tagUuid);
        operation.message.assign(fullShipment ? "local shipment full" : "local shipment partial");
        if (!recordInventoryOperation(operation)) {
            return false;
        }

        record = currentRecord;
        return true;
    }

    std::size_t pendingOperationCount() {
        const auto count = queryCount(
            "SELECT COUNT(*) FROM inventory_operations "
            "WHERE pending_sync = 1 AND operation_type IN ('provision_tag', 'receipt_unit', 'shipment_partial_unit', 'shipment_full_unit', 'shipment_unit');"
        );
        return count.has_value() ? static_cast<std::size_t>(count.value()) : 0;
    }

    template <std::size_t N>
    bool listPendingOperations(std::array<device::domain::InventoryOperationRecord, N>& operations, std::size_t& count) {
        count = 0;
        if (!status_.ready || db_ == nullptr) {
            return false;
        }

        for (auto& operation : operations) {
            operation = {};
        }

        sqlite3_stmt* stmt = nullptr;
        static constexpr const char* kSql =
            "SELECT id, boot_id, created_at_ms, product_id, sku, quantity, operation_type, tag_uuid, uid_hex, "
            "local_status, result_code, message, pending_sync, client_operation_id, sync_attempt_count, "
            "last_sync_error, synced_at_ms "
            "FROM inventory_operations WHERE pending_sync = 1 "
            "AND operation_type IN ('provision_tag', 'receipt_unit', 'shipment_partial_unit', 'shipment_full_unit', 'shipment_unit') "
            "ORDER BY id ASC LIMIT ?;";
        static constexpr const char* kLegacySql =
            "SELECT id, boot_id, created_at_ms, sku, operation_type, tag_uuid, uid_hex, "
            "local_status, result_code, message, pending_sync, client_operation_id, sync_attempt_count, "
            "last_sync_error, synced_at_ms "
            "FROM inventory_operations WHERE pending_sync = 1 "
            "AND operation_type IN ('provision_tag', 'receipt_unit', 'shipment_partial_unit', 'shipment_full_unit', 'shipment_unit') "
            "ORDER BY id ASC LIMIT ?;";
        if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
            const auto* error = sqlite3_errmsg(db_);
            const bool missingProductId = error != nullptr &&
                (std::strstr(error, "no such column: product_id") != nullptr ||
                 std::strstr(error, "no such column: quantity") != nullptr);
            sqlite3_finalize(stmt);
            stmt = nullptr;
            if (!missingProductId || sqlite3_prepare_v2(db_, kLegacySql, -1, &stmt, nullptr) != SQLITE_OK) {
                setWriteError("prepare listPendingOperations");
                return false;
            }

            sqlite3_bind_int(stmt, 1, static_cast<int>(N));
            while (count < N && sqlite3_step(stmt) == SQLITE_ROW) {
                auto& operation = operations[count++];
                operation.rowId = static_cast<uint32_t>(sqlite3_column_int(stmt, 0));
                operation.bootId = static_cast<uint32_t>(sqlite3_column_int(stmt, 1));
                operation.createdAtMs = static_cast<uint32_t>(sqlite3_column_int(stmt, 2));
                operation.productId = 0;
                operation.sku = static_cast<uint32_t>(sqlite3_column_int(stmt, 3));
                operation.operationType = operationTypeFromText(columnText(stmt, 4));
                operation.quantity = 1;
                readColumnText(stmt, 5, operation.tagUuid);
                readColumnText(stmt, 6, operation.uidHex);
                operation.localStatus = inventoryStatusFromText(columnText(stmt, 7));
                readColumnText(stmt, 8, operation.resultCode);
                readColumnText(stmt, 9, operation.message);
                operation.pendingSync = sqlite3_column_int(stmt, 10) != 0;
                readColumnText(stmt, 11, operation.clientOperationId);
                operation.syncAttemptCount = static_cast<uint32_t>(sqlite3_column_int(stmt, 12));
                readColumnText(stmt, 13, operation.lastSyncError);
                operation.syncedAtMs = static_cast<uint32_t>(sqlite3_column_int(stmt, 14));
                if (operation.productId == 0 && operation.sku != 0) {
                    operation.productId = lookupProductIdBySku(operation.sku);
                }
                if (operation.clientOperationId.empty()) {
                    assignClientOperationId(operation);
                    updateOperationClientId(operation.rowId, operation.clientOperationId.view());
                }
            }
            sqlite3_finalize(stmt);
            return true;
        }

        sqlite3_bind_int(stmt, 1, static_cast<int>(N));
        while (count < N && sqlite3_step(stmt) == SQLITE_ROW) {
            auto& operation = operations[count++];
            operation.rowId = static_cast<uint32_t>(sqlite3_column_int(stmt, 0));
            operation.bootId = static_cast<uint32_t>(sqlite3_column_int(stmt, 1));
            operation.createdAtMs = static_cast<uint32_t>(sqlite3_column_int(stmt, 2));
            operation.productId = static_cast<uint32_t>(sqlite3_column_int(stmt, 3));
            operation.sku = static_cast<uint32_t>(sqlite3_column_int(stmt, 4));
            operation.quantity = static_cast<uint32_t>(sqlite3_column_int(stmt, 5));
            operation.operationType = operationTypeFromText(columnText(stmt, 6));
            readColumnText(stmt, 7, operation.tagUuid);
            readColumnText(stmt, 8, operation.uidHex);
            operation.localStatus = inventoryStatusFromText(columnText(stmt, 9));
            readColumnText(stmt, 10, operation.resultCode);
            readColumnText(stmt, 11, operation.message);
            operation.pendingSync = sqlite3_column_int(stmt, 12) != 0;
            readColumnText(stmt, 13, operation.clientOperationId);
            operation.syncAttemptCount = static_cast<uint32_t>(sqlite3_column_int(stmt, 14));
            readColumnText(stmt, 15, operation.lastSyncError);
            operation.syncedAtMs = static_cast<uint32_t>(sqlite3_column_int(stmt, 16));
            if (operation.productId == 0 && operation.sku != 0) {
                operation.productId = lookupProductIdBySku(operation.sku);
            }
            if (operation.clientOperationId.empty()) {
                assignClientOperationId(operation);
                updateOperationClientId(operation.rowId, operation.clientOperationId.view());
            }
        }
        sqlite3_finalize(stmt);
        return true;
    }

    bool markOperationSynced(std::string_view clientOperationId, uint32_t syncedAtMs) {
        FixedString<36> tagUuid;
        if (!lookupOperationTagUuid(clientOperationId, tagUuid)) {
            return false;
        }

        sqlite3_stmt* stmt = nullptr;
        static constexpr const char* kSql =
            "UPDATE inventory_operations SET pending_sync = 0, synced_at_ms = ?, last_sync_error = NULL "
            "WHERE client_operation_id = ?;";
        if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
            setWriteError("prepare markOperationSynced");
            return false;
        }
        sqlite3_bind_int(stmt, 1, static_cast<int>(syncedAtMs));
        bindTextView(stmt, 2, clientOperationId);
        if (!finalizeStep(stmt)) {
            return false;
        }

        return refreshPendingSyncForTag(tagUuid.view());
    }

    bool markOperationSyncFailed(std::string_view clientOperationId, std::string_view lastError) {
        sqlite3_stmt* stmt = nullptr;
        static constexpr const char* kSql =
            "UPDATE inventory_operations "
            "SET pending_sync = 1, sync_attempt_count = sync_attempt_count + 1, last_sync_error = ? "
            "WHERE client_operation_id = ?;";
        if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
            setWriteError("prepare markOperationSyncFailed");
            return false;
        }
        bindTextView(stmt, 1, lastError);
        bindTextView(stmt, 2, clientOperationId);
        return finalizeStep(stmt);
    }

    bool clearPendingSyncQueue(uint32_t clearedAtMs) {
        if (!status_.ready || db_ == nullptr) {
            return false;
        }

        sqlite3_stmt* opStmt = nullptr;
        static constexpr const char* kOpSql =
            "UPDATE inventory_operations "
            "SET pending_sync = 0, synced_at_ms = ?, last_sync_error = 'MANUAL_CLEAR' "
            "WHERE pending_sync = 1 AND operation_type IN ('provision_tag', 'receipt_unit', 'shipment_partial_unit', 'shipment_full_unit', 'shipment_unit');";
        if (sqlite3_prepare_v2(db_, kOpSql, -1, &opStmt, nullptr) != SQLITE_OK) {
            setWriteError("prepare clearPendingSyncQueue operations");
            return false;
        }
        sqlite3_bind_int(opStmt, 1, static_cast<int>(clearedAtMs));
        if (!finalizeStep(opStmt)) {
            return false;
        }

        if (!exec("UPDATE inventory_units SET pending_sync = 0 WHERE pending_sync = 1;")) {
            setWriteError("exec clearPendingSyncQueue units");
            return false;
        }

        if (!exec(
                "UPDATE inventory_tags "
                "SET pending_sync = 0, "
                "local_status = CASE WHEN local_status = 'NeedsSync' THEN 'Assigned' ELSE local_status END "
                "WHERE pending_sync = 1;"
            )) {
            setWriteError("exec clearPendingSyncQueue tags");
            return false;
        }

        return true;
    }

    const device::domain::DatabaseStatusSnapshot& status() const {
        return status_;
    }

private:
    static device::domain::InventoryOperationType operationTypeFromText(std::string_view value) {
        if (value == "receipt_unit") {
            return device::domain::InventoryOperationType::ReceiptUnit;
        }
        if (value == "shipment_unit") {
            return device::domain::InventoryOperationType::ShipmentFullUnit;
        }
        if (value == "shipment_partial_unit") {
            return device::domain::InventoryOperationType::ShipmentPartialUnit;
        }
        if (value == "shipment_full_unit") {
            return device::domain::InventoryOperationType::ShipmentFullUnit;
        }
        if (value == "assign_tag") {
            return device::domain::InventoryOperationType::AssignTag;
        }
        return device::domain::InventoryOperationType::ProvisionTag;
    }

    template <std::size_t Capacity>
    static void bindFixedString(sqlite3_stmt* stmt, int index, const FixedString<Capacity>& value) {
        if (value.empty()) {
            sqlite3_bind_null(stmt, index);
            return;
        }
        sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT);
    }

    static void bindTextView(sqlite3_stmt* stmt, int index, std::string_view value) {
        if (value.empty()) {
            sqlite3_bind_null(stmt, index);
            return;
        }
        sqlite3_bind_text(stmt, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
    }

    template <std::size_t Capacity>
    static void readColumnText(sqlite3_stmt* stmt, int index, FixedString<Capacity>& out) {
        const auto* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, index));
        if (value == nullptr) {
            out.clear();
            return;
        }
        out.assign(reinterpret_cast<const char*>(sqlite3_column_text(stmt, index)));
    }

    static std::string_view columnText(sqlite3_stmt* stmt, int index) {
        const auto* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, index));
        if (value == nullptr) {
            return {};
        }
        return std::string_view(value);
    }

    static device::domain::InventoryTagStatus inventoryStatusFromText(std::string_view value) {
        if (value == "Free") {
            return device::domain::InventoryTagStatus::Free;
        }
        if (value == "Assigned") {
            return device::domain::InventoryTagStatus::Assigned;
        }
        if (value == "NeedsSync") {
            return device::domain::InventoryTagStatus::NeedsSync;
        }
        return device::domain::InventoryTagStatus::Unknown;
    }

    static device::domain::InventoryUnitState inventoryUnitStateFromText(std::string_view value) {
        if (value == "Active") {
            return device::domain::InventoryUnitState::Active;
        }
        if (value == "Shipped") {
            return device::domain::InventoryUnitState::Shipped;
        }
        return device::domain::InventoryUnitState::None;
    }

    bool readyForWrite() {
        if (status_.ready && db_ != nullptr) {
            return true;
        }
        setWriteError("store not ready");
        return false;
    }

    bool finalizeStep(sqlite3_stmt* stmt) {
        const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        if (!ok) {
            setWriteError("sqlite step failed");
        }
        return ok;
    }

    bool ensureBaseSchema() {
        return exec(
                   "CREATE TABLE IF NOT EXISTS meta ("
                   "key TEXT PRIMARY KEY,"
                   "value TEXT NOT NULL"
                   ");"
               ) &&
               exec(
                   "CREATE TABLE IF NOT EXISTS device_events ("
                   "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                   "boot_id INTEGER NOT NULL,"
                   "created_at_ms INTEGER NOT NULL,"
                   "event_type TEXT NOT NULL,"
                   "severity TEXT NOT NULL,"
                   "screen TEXT,"
                   "uid_hex TEXT,"
                   "tag_uuid TEXT,"
                   "payload_kind TEXT,"
                   "result_code TEXT,"
                   "message TEXT"
                   ");"
               ) &&
               exec("CREATE INDEX IF NOT EXISTS idx_device_events_created_at ON device_events(created_at_ms);") &&
               exec("CREATE INDEX IF NOT EXISTS idx_device_events_event_type ON device_events(event_type);") &&
               exec("CREATE INDEX IF NOT EXISTS idx_device_events_uid_hex ON device_events(uid_hex);") &&
               exec("INSERT OR IGNORE INTO meta(key, value) VALUES('schema_version', '1');") &&
               exec("INSERT OR IGNORE INTO meta(key, value) VALUES('boot_counter', '0');");
    }

    bool ensureWarehouseSchema() {
        return exec(
                   "CREATE TABLE IF NOT EXISTS inventory_tags ("
                   "tag_uuid TEXT PRIMARY KEY,"
                   "uid_hex TEXT NOT NULL,"
                   "local_status TEXT NOT NULL,"
                   "assignment_count INTEGER NOT NULL DEFAULT 0,"
                   "last_seen_at_ms INTEGER NOT NULL DEFAULT 0,"
                   "created_at_ms INTEGER NOT NULL,"
                   "updated_at_ms INTEGER NOT NULL,"
                   "pending_sync INTEGER NOT NULL DEFAULT 1,"
                   "last_result_code TEXT"
                   ");"
               ) &&
               exec(
                   "CREATE TABLE IF NOT EXISTS products ("
                   "product_id INTEGER NOT NULL DEFAULT 0,"
                   "sku INTEGER PRIMARY KEY,"
                   "name TEXT NOT NULL,"
                   "is_active INTEGER NOT NULL DEFAULT 1,"
                   "created_at_ms INTEGER NOT NULL,"
                   "updated_at_ms INTEGER NOT NULL"
                   ");"
               ) &&
               exec(
                   "CREATE TABLE IF NOT EXISTS inventory_units ("
                   "tag_uuid TEXT PRIMARY KEY,"
                   "uid_hex TEXT NOT NULL,"
                   "sku INTEGER NOT NULL,"
                   "quantity_initial INTEGER NOT NULL DEFAULT 1,"
                   "quantity_current INTEGER NOT NULL DEFAULT 1,"
                   "state TEXT NOT NULL,"
                   "received_at_ms INTEGER NOT NULL,"
                   "shipped_at_ms INTEGER NOT NULL DEFAULT 0,"
                   "pending_sync INTEGER NOT NULL DEFAULT 1"
                   ");"
               ) &&
               exec(
                   "CREATE TABLE IF NOT EXISTS inventory_operations ("
                   "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                   "boot_id INTEGER NOT NULL,"
                   "created_at_ms INTEGER NOT NULL,"
                   "operation_type TEXT NOT NULL,"
                   "tag_uuid TEXT,"
                   "uid_hex TEXT,"
                   "product_id INTEGER NOT NULL DEFAULT 0,"
                   "sku INTEGER NOT NULL DEFAULT 0,"
                   "quantity INTEGER NOT NULL DEFAULT 1,"
                   "local_status TEXT,"
                   "result_code TEXT,"
                   "message TEXT,"
                   "pending_sync INTEGER NOT NULL DEFAULT 1,"
                   "client_operation_id TEXT,"
                   "sync_attempt_count INTEGER NOT NULL DEFAULT 0,"
                   "last_sync_error TEXT,"
                   "synced_at_ms INTEGER NOT NULL DEFAULT 0"
                   ");"
               ) &&
               exec("CREATE INDEX IF NOT EXISTS idx_inventory_tags_uid_hex ON inventory_tags(uid_hex);") &&
               exec("CREATE INDEX IF NOT EXISTS idx_inventory_tags_status ON inventory_tags(local_status);") &&
               exec("CREATE INDEX IF NOT EXISTS idx_products_active ON products(is_active, sku);") &&
               exec("CREATE INDEX IF NOT EXISTS idx_inventory_units_state ON inventory_units(state);") &&
               exec("CREATE INDEX IF NOT EXISTS idx_inventory_ops_created_at ON inventory_operations(created_at_ms);") &&
               exec("CREATE INDEX IF NOT EXISTS idx_inventory_ops_tag_uuid ON inventory_operations(tag_uuid);");
    }

    bool ensureWarehouseCompatibility() {
        const bool altered = execIgnoreError("ALTER TABLE inventory_tags ADD COLUMN assignment_count INTEGER NOT NULL DEFAULT 0;") &&
                             execIgnoreError("ALTER TABLE inventory_tags ADD COLUMN last_seen_at_ms INTEGER NOT NULL DEFAULT 0;") &&
                             execIgnoreError("ALTER TABLE inventory_tags ADD COLUMN created_at_ms INTEGER NOT NULL DEFAULT 0;") &&
                             execIgnoreError("ALTER TABLE inventory_tags ADD COLUMN updated_at_ms INTEGER NOT NULL DEFAULT 0;") &&
                             execIgnoreError("ALTER TABLE inventory_tags ADD COLUMN pending_sync INTEGER NOT NULL DEFAULT 1;") &&
                             execIgnoreError("ALTER TABLE inventory_tags ADD COLUMN last_result_code TEXT;") &&
                             execIgnoreError("ALTER TABLE products ADD COLUMN is_active INTEGER NOT NULL DEFAULT 1;") &&
                             execIgnoreError("ALTER TABLE products ADD COLUMN product_id INTEGER NOT NULL DEFAULT 0;") &&
                             execIgnoreError("ALTER TABLE products ADD COLUMN created_at_ms INTEGER NOT NULL DEFAULT 0;") &&
                             execIgnoreError("ALTER TABLE products ADD COLUMN updated_at_ms INTEGER NOT NULL DEFAULT 0;") &&
                             execIgnoreError("ALTER TABLE inventory_units ADD COLUMN shipped_at_ms INTEGER NOT NULL DEFAULT 0;") &&
                             execIgnoreError("ALTER TABLE inventory_units ADD COLUMN pending_sync INTEGER NOT NULL DEFAULT 1;") &&
                             execIgnoreError("ALTER TABLE inventory_units ADD COLUMN quantity_initial INTEGER NOT NULL DEFAULT 1;") &&
                             execIgnoreError("ALTER TABLE inventory_units ADD COLUMN quantity_current INTEGER NOT NULL DEFAULT 1;") &&
                             execIgnoreError("ALTER TABLE inventory_operations ADD COLUMN product_id INTEGER NOT NULL DEFAULT 0;") &&
                             execIgnoreError("ALTER TABLE inventory_operations ADD COLUMN sku INTEGER NOT NULL DEFAULT 0;") &&
                             execIgnoreError("ALTER TABLE inventory_operations ADD COLUMN quantity INTEGER NOT NULL DEFAULT 1;") &&
                             execIgnoreError("ALTER TABLE inventory_operations ADD COLUMN local_status TEXT;") &&
                             execIgnoreError("ALTER TABLE inventory_operations ADD COLUMN result_code TEXT;") &&
                             execIgnoreError("ALTER TABLE inventory_operations ADD COLUMN message TEXT;") &&
                             execIgnoreError("ALTER TABLE inventory_operations ADD COLUMN pending_sync INTEGER NOT NULL DEFAULT 1;") &&
                             execIgnoreError("ALTER TABLE inventory_operations ADD COLUMN client_operation_id TEXT;") &&
                             execIgnoreError("ALTER TABLE inventory_operations ADD COLUMN sync_attempt_count INTEGER NOT NULL DEFAULT 0;") &&
                             execIgnoreError("ALTER TABLE inventory_operations ADD COLUMN last_sync_error TEXT;") &&
                             execIgnoreError("ALTER TABLE inventory_operations ADD COLUMN synced_at_ms INTEGER NOT NULL DEFAULT 0;") &&
                             execIgnoreError("UPDATE inventory_operations SET client_operation_id = 'legacy-' || id WHERE client_operation_id IS NULL OR client_operation_id = '';") &&
                             execIgnoreError("CREATE INDEX IF NOT EXISTS idx_inventory_ops_client_op ON inventory_operations(client_operation_id);");
        if (!altered) {
            return false;
        }

        if (!tableHasColumn("products", "product_id") ||
            !tableHasColumn("products", "is_active") ||
            !tableHasColumn("products", "created_at_ms") ||
            !tableHasColumn("products", "updated_at_ms")) {
            if (!rebuildProductsTable()) {
                return false;
            }
        }

        return true;
    }

    bool migrateSchema(int fromVersion) {
        if (fromVersion <= 1) {
            if (!exec(
                    "CREATE TABLE IF NOT EXISTS inventory_tags ("
                    "tag_uuid TEXT PRIMARY KEY,"
                    "uid_hex TEXT NOT NULL,"
                    "local_status TEXT NOT NULL,"
                    "assignment_count INTEGER NOT NULL DEFAULT 0,"
                    "last_seen_at_ms INTEGER NOT NULL DEFAULT 0,"
                    "created_at_ms INTEGER NOT NULL,"
                    "updated_at_ms INTEGER NOT NULL,"
                    "pending_sync INTEGER NOT NULL DEFAULT 1,"
                    "last_result_code TEXT"
                    ");")) {
                return false;
            }
            fromVersion = 2;
            if (!setMetaInt("schema_version", 2)) {
                return false;
            }
        }

        if (fromVersion == 2) {
            if (!ensureWarehouseSchema()) {
                return false;
            }
            if (!exec("ALTER TABLE inventory_operations ADD COLUMN sku INTEGER NOT NULL DEFAULT 0;")) {

            }
            if (!setMetaInt("schema_version", 3)) {
                return false;
            }
            fromVersion = 3;
        }

        if (fromVersion == 3) {
            if (!ensureWarehouseCompatibility()) {
                return false;
            }
            if (!setMetaInt("schema_version", 4)) {
                return false;
            }
            fromVersion = 4;
        }

        if (fromVersion == 4) {
            if (!ensureWarehouseCompatibility()) {
                return false;
            }
            if (!setMetaInt("schema_version", static_cast<uint32_t>(device::config::kDatabaseSchemaVersion))) {
                return false;
            }
            return true;
        }

        return fromVersion == static_cast<int>(device::config::kDatabaseSchemaVersion);
    }

    bool upsertProduct(uint32_t productId, uint32_t sku, const char* name, bool isActive, uint32_t nowMs) {
        sqlite3_stmt* stmt = nullptr;
        static constexpr const char* kSql =
            "INSERT OR REPLACE INTO products (product_id, sku, name, is_active, created_at_ms, updated_at_ms) "
            "VALUES (?, ?, ?, ?, ?, ?);";
        if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
            setWriteError("prepare upsertProduct");
            return false;
        }

        sqlite3_bind_int(stmt, 1, static_cast<int>(productId));
        sqlite3_bind_int(stmt, 2, static_cast<int>(sku));
        sqlite3_bind_text(stmt, 3, name, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 4, isActive ? 1 : 0);
        sqlite3_bind_int(stmt, 5, static_cast<int>(nowMs));
        sqlite3_bind_int(stmt, 6, static_cast<int>(nowMs));
        return finalizeStep(stmt);
    }

    bool updateInventoryTagStatus(std::string_view tagUuid, std::string_view uidHex, const char* statusText, uint32_t nowMs, bool pendingSync) {
        sqlite3_stmt* stmt = nullptr;
        static constexpr const char* kSql =
            "UPDATE inventory_tags "
            "SET uid_hex = ?, local_status = ?, assignment_count = assignment_count + 1, "
            "last_seen_at_ms = ?, updated_at_ms = ?, pending_sync = ?, last_result_code = 'NONE' "
            "WHERE tag_uuid = ?;";
        if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
            setWriteError("prepare updateInventoryTagStatus");
            return false;
        }

        bindTextView(stmt, 1, uidHex);
        sqlite3_bind_text(stmt, 2, statusText, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, static_cast<int>(nowMs));
        sqlite3_bind_int(stmt, 4, static_cast<int>(nowMs));
        sqlite3_bind_int(stmt, 5, pendingSync ? 1 : 0);
        bindTextView(stmt, 6, tagUuid);
        return finalizeStep(stmt);
    }

    bool recordInventoryOperation(const device::domain::InventoryOperationRecord& operation) {
        if (!readyForWrite()) {
            return false;
        }

        static constexpr const char* kSql =
            "INSERT INTO inventory_operations "
            "(boot_id, created_at_ms, operation_type, tag_uuid, uid_hex, product_id, sku, quantity, local_status, result_code, message, pending_sync, client_operation_id, sync_attempt_count, last_sync_error, synced_at_ms) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
        static constexpr const char* kLegacySql =
            "INSERT INTO inventory_operations "
            "(boot_id, created_at_ms, operation_type, tag_uuid, uid_hex, local_status, result_code, message, pending_sync) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
            const auto* error = sqlite3_errmsg(db_);
            const bool legacyColumnError = error != nullptr &&
                (std::strstr(error, "no column named sku") != nullptr ||
                 std::strstr(error, "no column named product_id") != nullptr ||
                 std::strstr(error, "no column named quantity") != nullptr ||
                 std::strstr(error, "no column named client_operation_id") != nullptr ||
                 std::strstr(error, "no column named sync_attempt_count") != nullptr ||
                 std::strstr(error, "no column named last_sync_error") != nullptr ||
                 std::strstr(error, "no column named synced_at_ms") != nullptr);
            if (!legacyColumnError) {
                setWriteError("prepare recordInventoryOperation");
                return false;
            }

            sqlite3_finalize(stmt);
            stmt = nullptr;
            if (sqlite3_prepare_v2(db_, kLegacySql, -1, &stmt, nullptr) != SQLITE_OK) {
                setWriteError("prepare legacy recordInventoryOperation");
                return false;
            }

            sqlite3_bind_int(stmt, 1, static_cast<int>(operation.bootId));
            sqlite3_bind_int(stmt, 2, static_cast<int>(operation.createdAtMs));
            sqlite3_bind_text(stmt, 3, device::domain::toString(operation.operationType), -1, SQLITE_STATIC);
            bindFixedString(stmt, 4, operation.tagUuid);
            bindFixedString(stmt, 5, operation.uidHex);
            sqlite3_bind_text(stmt, 6, device::domain::toString(operation.localStatus), -1, SQLITE_STATIC);
            bindFixedString(stmt, 7, operation.resultCode);
            bindFixedString(stmt, 8, operation.message);
            sqlite3_bind_int(stmt, 9, operation.pendingSync ? 1 : 0);
            return finalizeStep(stmt);
        }

        sqlite3_bind_int(stmt, 1, static_cast<int>(operation.bootId));
        sqlite3_bind_int(stmt, 2, static_cast<int>(operation.createdAtMs));
        sqlite3_bind_text(stmt, 3, device::domain::toString(operation.operationType), -1, SQLITE_STATIC);
        bindFixedString(stmt, 4, operation.tagUuid);
        bindFixedString(stmt, 5, operation.uidHex);
        sqlite3_bind_int(stmt, 6, static_cast<int>(operation.productId));
        sqlite3_bind_int(stmt, 7, static_cast<int>(operation.sku));
        sqlite3_bind_int(stmt, 8, static_cast<int>(operation.quantity));
        sqlite3_bind_text(stmt, 9, device::domain::toString(operation.localStatus), -1, SQLITE_STATIC);
        bindFixedString(stmt, 10, operation.resultCode);
        bindFixedString(stmt, 11, operation.message);
        sqlite3_bind_int(stmt, 12, operation.pendingSync ? 1 : 0);
        bindFixedString(stmt, 13, operation.clientOperationId);
        sqlite3_bind_int(stmt, 14, static_cast<int>(operation.syncAttemptCount));
        bindFixedString(stmt, 15, operation.lastSyncError);
        sqlite3_bind_int(stmt, 16, static_cast<int>(operation.syncedAtMs));
        return finalizeStep(stmt);
    }

    static void assignClientOperationId(device::domain::InventoryOperationRecord& operation) {
        if (!operation.clientOperationId.empty()) {
            return;
        }

        char buffer[64] {};
        const auto written = snprintf(
            buffer,
            sizeof(buffer),
            "op-%lu-%lu-%s",
            static_cast<unsigned long>(operation.bootId),
            static_cast<unsigned long>(operation.createdAtMs),
            device::domain::toString(operation.operationType)
        );
        operation.clientOperationId.assign(written > 0 ? std::string_view(buffer, static_cast<std::size_t>(written)) : "op-fallback");
    }

    bool tableHasColumn(const char* tableName, const char* columnName) {
        if (db_ == nullptr || tableName == nullptr || columnName == nullptr) {
            return false;
        }

        char sql[128] {};
        const int written = snprintf(sql, sizeof(sql), "SELECT \"%s\" FROM \"%s\" LIMIT 0;", columnName, tableName);
        if (written <= 0 || written >= static_cast<int>(sizeof(sql))) {
            return false;
        }

        sqlite3_stmt* stmt = nullptr;
        const bool ok = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK;
        sqlite3_finalize(stmt);
        return ok;
    }

    bool rebuildProductsTable() {
        return exec("DROP TABLE IF EXISTS products;") &&
               exec(
                   "CREATE TABLE products ("
                   "product_id INTEGER NOT NULL DEFAULT 0,"
                   "sku INTEGER PRIMARY KEY,"
                   "name TEXT NOT NULL,"
                   "is_active INTEGER NOT NULL DEFAULT 1,"
                   "created_at_ms INTEGER NOT NULL,"
                   "updated_at_ms INTEGER NOT NULL"
                   ");"
               ) &&
               exec("CREATE INDEX IF NOT EXISTS idx_products_active ON products(is_active, sku);");
    }

    bool applyDatabaseResetNonce() {
        resetMarkerNeedsPersist_ = false;
        const uint32_t storedNonce = readDatabaseResetNonce();
        if (storedNonce == device::config::kDatabaseResetNonce) {
            return true;
        }

        SD.remove(device::config::kDatabaseSdFile);
        resetMarkerNeedsPersist_ = true;
        return true;
    }

    uint32_t readDatabaseResetNonce() {
        File file = SD.open(device::config::kDatabaseResetMarkerFile, FILE_READ);
        if (!file) {
            return 0;
        }

        char buffer[16] {};
        const auto bytesRead = file.readBytes(buffer, sizeof(buffer) - 1);
        file.close();
        if (bytesRead <= 0) {
            return 0;
        }

        buffer[bytesRead] = '\0';
        return static_cast<uint32_t>(std::strtoul(buffer, nullptr, 10));
    }

    bool persistDatabaseResetNonce() {
        File file = SD.open(device::config::kDatabaseResetMarkerFile, FILE_WRITE);
        if (!file) {
            return false;
        }

        const auto written = file.print(device::config::kDatabaseResetNonce);
        file.close();
        resetMarkerNeedsPersist_ = false;
        return written > 0;
    }

    bool updateOperationClientId(uint32_t rowId, std::string_view clientOperationId) {
        sqlite3_stmt* stmt = nullptr;
        static constexpr const char* kSql =
            "UPDATE inventory_operations SET client_operation_id = ? WHERE id = ?;";
        if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
            setWriteError("prepare updateOperationClientId");
            return false;
        }
        bindTextView(stmt, 1, clientOperationId);
        sqlite3_bind_int(stmt, 2, static_cast<int>(rowId));
        return finalizeStep(stmt);
    }

    uint32_t lookupProductIdBySku(uint32_t sku) {
        if (db_ == nullptr || sku == 0) {
            return 0;
        }

        sqlite3_stmt* stmt = nullptr;
        static constexpr const char* kSql = "SELECT product_id FROM products WHERE sku = ? LIMIT 1;";
        if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
            return 0;
        }

        sqlite3_bind_int(stmt, 1, static_cast<int>(sku));
        uint32_t productId = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            productId = static_cast<uint32_t>(sqlite3_column_int(stmt, 0));
        }
        sqlite3_finalize(stmt);
        return productId;
    }

    bool lookupOperationTagUuid(std::string_view clientOperationId, FixedString<36>& tagUuid) {
        tagUuid.clear();
        sqlite3_stmt* stmt = nullptr;
        static constexpr const char* kSql =
            "SELECT tag_uuid FROM inventory_operations WHERE client_operation_id = ? LIMIT 1;";
        if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
            setWriteError("prepare lookupOperationTagUuid");
            return false;
        }
        bindTextView(stmt, 1, clientOperationId);
        const int rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            readColumnText(stmt, 0, tagUuid);
        }
        sqlite3_finalize(stmt);
        return rc == SQLITE_ROW || rc == SQLITE_DONE;
    }

    bool refreshPendingSyncForTag(std::string_view tagUuid) {
        if (tagUuid.empty()) {
            return true;
        }

        sqlite3_stmt* countStmt = nullptr;
        static constexpr const char* kPendingSql =
            "SELECT COUNT(*) FROM inventory_operations "
            "WHERE tag_uuid = ? AND pending_sync = 1 "
            "AND operation_type IN ('provision_tag', 'receipt_unit', 'shipment_partial_unit', 'shipment_full_unit', 'shipment_unit');";
        if (sqlite3_prepare_v2(db_, kPendingSql, -1, &countStmt, nullptr) != SQLITE_OK) {
            setWriteError("prepare refreshPendingSyncForTag count");
            return false;
        }
        bindTextView(countStmt, 1, tagUuid);
        bool hasPending = false;
        if (sqlite3_step(countStmt) == SQLITE_ROW) {
            hasPending = sqlite3_column_int(countStmt, 0) > 0;
        }
        sqlite3_finalize(countStmt);

        sqlite3_stmt* unitStmt = nullptr;
        static constexpr const char* kUnitSql =
            "UPDATE inventory_units SET pending_sync = ? WHERE tag_uuid = ?;";
        if (sqlite3_prepare_v2(db_, kUnitSql, -1, &unitStmt, nullptr) != SQLITE_OK) {
            setWriteError("prepare clearPendingSyncForTag units");
            return false;
        }
        sqlite3_bind_int(unitStmt, 1, hasPending ? 1 : 0);
        bindTextView(unitStmt, 2, tagUuid);
        if (!finalizeStep(unitStmt)) {
            return false;
        }

        sqlite3_stmt* tagStmt = nullptr;
        static constexpr const char* kTagSql =
            "UPDATE inventory_tags SET pending_sync = 0, local_status = CASE "
            "WHEN local_status = 'NeedsSync' THEN 'Assigned' ELSE local_status END "
            "WHERE tag_uuid = ?;";
        static constexpr const char* kTagPendingSql =
            "UPDATE inventory_tags SET pending_sync = 1, local_status = CASE "
            "WHEN local_status = 'Free' THEN 'Free' ELSE 'NeedsSync' END "
            "WHERE tag_uuid = ?;";
        if (sqlite3_prepare_v2(db_, hasPending ? kTagPendingSql : kTagSql, -1, &tagStmt, nullptr) != SQLITE_OK) {
            setWriteError("prepare clearPendingSyncForTag tags");
            return false;
        }
        bindTextView(tagStmt, 1, tagUuid);
        return finalizeStep(tagStmt);
    }

    bool exec(const char* sql) {
        char* errorMessage = nullptr;
        const int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &errorMessage);
        if (rc != SQLITE_OK) {
            if (errorMessage != nullptr) {
                sqlite3_free(errorMessage);
            }
            return false;
        }
        return true;
    }

    bool execIgnoreError(const char* sql) {
        char* errorMessage = nullptr;
        const int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &errorMessage);
        if (errorMessage != nullptr) {
            sqlite3_free(errorMessage);
        }
        return rc == SQLITE_OK || rc == SQLITE_ERROR;
    }

    std::optional<int> queryMetaInt(const char* key) {
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, "SELECT value FROM meta WHERE key = ?;", -1, &stmt, nullptr) != SQLITE_OK) {
            return std::nullopt;
        }

        sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);
        std::optional<int> result;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const auto* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (value != nullptr) {
                result = atoi(value);
            }
        }
        sqlite3_finalize(stmt);
        return result;
    }

    std::optional<int> queryCount(const char* sql) {
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return std::nullopt;
        }

        std::optional<int> result;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            result = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
        return result;
    }

    bool setMetaInt(const char* key, uint32_t value) {
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, "INSERT OR REPLACE INTO meta(key, value) VALUES(?, ?);", -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }

        sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, static_cast<int>(value));
        const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        return ok;
    }

    void setWriteError(const char* context) {
        status_.lastError = device::domain::ErrorCode::DbWrite;
        if (db_ != nullptr) {
            Serial.printf("[db] write error (%s): %s\n", context, sqlite3_errmsg(db_));
        } else {
            Serial.printf("[db] write error (%s): db unavailable\n", context);
        }
    }

    void setInitError(device::domain::DatabaseStatusSnapshot& status, const char* message) {
        status_.ready = false;
        status_.bootId = 0;
        status_.lastError = device::domain::ErrorCode::DbInit;
        status = status_;
        if (db_ != nullptr) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        Serial.printf("[db] init error: %s\n", message);
    }

    sqlite3* db_ = nullptr;
    device::domain::DatabaseStatusSnapshot status_ {};
    bool resetMarkerNeedsPersist_ = false;
};

}

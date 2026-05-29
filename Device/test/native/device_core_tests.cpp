#include <cassert>
#include <iostream>
#include <optional>

#include "app/AppController.h"
#include "config/DeviceConfig.h"
#include "domain/ButtonInterpreter.h"
#include "domain/Models.h"
#include "config/DeviceConfig.h"
#include "domain/OfflineQueue.h"
#include "domain/TagCodec.h"
#include "ui/DiagnosticsPresenter.h"

using device::config::Button;
using device::domain::ButtonPressType;
using device::domain::OfflineOperationRecord;
using device::domain::OfflineQueue;
using device::domain::OperationType;
using device::domain::TagCodec;

namespace {

void test_uuid_normalization_and_ndef_round_trip() {
    FixedString<36> normalized;
    assert(TagCodec::normalizeUuid("123E4567-E89B-12D3-A456-426614174000", normalized));
    assert(normalized.view() == "123e4567-e89b-12d3-a456-426614174000");

    FixedString<96> encoded;
    assert(TagCodec::encodeNdefTextRecord(normalized.view(), encoded));
    const auto decoded = TagCodec::decodeNdefTextRecord(encoded.view());
    assert(decoded.has_value());
    assert(decoded->view() == normalized.view());

    FixedString<96> wrapped;
    char tlv[100] {};
    tlv[0] = static_cast<char>(0x03);
    tlv[1] = static_cast<char>(encoded.size());
    for (std::size_t index = 0; index < encoded.size(); ++index) {
        tlv[index + 2] = encoded.view()[index];
    }
    tlv[encoded.size() + 2] = static_cast<char>(0xFE);
    assert(TagCodec::extractNdefMessage(std::string_view(tlv, encoded.size() + 3), wrapped));
    assert(wrapped.view() == encoded.view());

    FixedString<96> encodedTlv;
    assert(TagCodec::encodeNdefTextTlv(normalized.view(), encodedTlv));
    std::array<uint8_t, 48> packed {};
    assert(TagCodec::packType2Data(encodedTlv.view(), packed));
    FixedString<96> unpacked;
    assert(TagCodec::extractNdefMessage(
        std::string_view(reinterpret_cast<const char*>(packed.data()), packed.size()),
        unpacked
    ));
    assert(unpacked.view() == encoded.view());

    FixedString<36> v4Uuid;
    assert(TagCodec::formatUuidV4({{
        0x12, 0x34, 0x56, 0x78,
        0x9A, 0xBC, 0xDE, 0xF0,
        0x11, 0x22, 0x33, 0x44,
        0x55, 0x66, 0x77, 0x88,
    }}, v4Uuid));
    assert(v4Uuid.size() == 36);
    assert(v4Uuid.view()[14] == '4');
    assert(v4Uuid.view()[19] == '8' || v4Uuid.view()[19] == '9' || v4Uuid.view()[19] == 'a' || v4Uuid.view()[19] == 'b');
}

void test_invalid_uuid_is_rejected() {
    FixedString<36> normalized;
    assert(!TagCodec::normalizeUuid("invalid-uuid", normalized));
}

void test_blank_and_legacy_tag_detection() {
    const std::string blank(48, '\0');
    assert(TagCodec::isBlankType2Data(blank));

    FixedString<32> legacy;
    assert(TagCodec::extractLegacyRaw32("0123456789abcdef0123456789abcdef", legacy));
    assert(legacy.view() == "0123456789abcdef0123456789abcdef");
    assert(!TagCodec::extractLegacyRaw32("0123\x01""56789abcdef0123456789abcdef", legacy));
}

void test_event_and_error_mappings() {
    assert(std::string_view(device::domain::toString(device::domain::DeviceEventType::BootStarted)) == "boot_started");
    assert(std::string_view(device::domain::toString(device::domain::DeviceEventSeverity::Error)) == "ERROR");
    assert(std::string_view(device::domain::toString(device::domain::ErrorCode::DbInit)) == "DB_INIT");
    assert(std::string_view(device::domain::toString(device::domain::NfcPayloadKind::LegacyRaw32)) == "LegacyRaw32");
    assert(std::string_view(device::domain::toSyncOperationType(device::domain::InventoryOperationType::ReceiptUnit)) == "receipt");
    assert(std::string_view(device::domain::toSyncOperationType(device::domain::InventoryOperationType::ShipmentPartialUnit)) == "shipment_partial");
    assert(std::string_view(device::domain::toSyncOperationType(device::domain::InventoryOperationType::ShipmentFullUnit)) == "shipment_full");
}

void test_quantity_snapshots_and_limits() {
    device::domain::ReceiptSnapshot receipt;
    receipt.enteredQuantity = 9999;
    receipt.quantityCursor = 4;
    assert(receipt.enteredQuantity == 9999);
    receipt.clear();
    assert(receipt.enteredQuantity == 0);
    assert(!receipt.quantityEditing);

    device::domain::ShipmentSnapshot shipment;
    shipment.quantityCurrent = 10000;
    shipment.quantityToShip = 250;
    assert(shipment.quantityToShip < shipment.quantityCurrent);
    shipment.clear();
    assert(shipment.quantityCurrent == 0);
    assert(!shipment.quantityEditing);
}

void test_keypad_short_and_long_press() {
    device::domain::ButtonInterpreter interpreter;
    assert(!interpreter.update(Button::Ok, true, 0).has_value());
    assert(!interpreter.update(Button::Ok, true, 100).has_value());
    const auto shortPress = interpreter.update(Button::Ok, false, 150);
    assert(shortPress.has_value());
    assert(shortPress->type == ButtonPressType::ShortPress);

    assert(!interpreter.update(Button::Up, true, 500).has_value());
    assert(!interpreter.update(Button::Up, true, 600).has_value());
    const auto longPress = interpreter.update(Button::Up, true, 1300);
    assert(longPress.has_value());
    assert(longPress->type == ButtonPressType::LongPress);
}

void test_offline_queue_serialization_round_trip() {
    OfflineOperationRecord record;
    record.operationType = OperationType::Receipt;
    record.productId = 1001;
    record.quantity = 5;
    assert(record.clientOperationId.assign("op-1"));
    assert(record.tagUuid.assign("123e4567-e89b-12d3-a456-426614174000"));
    assert(record.chipUidHex.assign("04a1b2c3d4"));
    assert(record.note.assign("queued"));

    const auto serialized = OfflineQueue::serialize(record);
    const auto parsed = OfflineQueue::deserialize(serialized);
    assert(parsed.has_value());
    assert(parsed->productId == record.productId);
    assert(parsed->quantity == record.quantity);
    assert(parsed->clientOperationId.view() == record.clientOperationId.view());
    assert(OfflineQueue::retryBackoffMs(2) == device::config::kSyncBackoffBaseMs * 4);
}

void test_diagnostics_state_transitions_and_scroll() {
    device::app::AppController controller;
    controller.startDiagnostics();
    assert(controller.stateMachine().currentScreen() == device::ui::Screen::BootDiagnostics);

    device::domain::DiagnosticsSnapshot failedSnapshot;
    failedSnapshot.displayOk = true;
    failedSnapshot.buttonsOk = true;
    failedSnapshot.failureCode = device::domain::ErrorCode::Pn532Firmware;
    failedSnapshot.addDetailLine("TFT: landscape 320x240");
    failedSnapshot.addDetailLine("Buttons: UP=9 DOWN=18 OK=21");
    failedSnapshot.addDetailLine("PN532 firmware probe failed");
    controller.finishDiagnostics(failedSnapshot);
    assert(controller.stateMachine().currentScreen() == device::ui::Screen::DiagnosticsFailure);

    const auto retryAction = controller.handleButtonEvent({Button::Ok, ButtonPressType::ShortPress});
    assert(retryAction == device::ui::AppStateMachine::Action::RetryDiagnostics);
    assert(controller.stateMachine().currentScreen() == device::ui::Screen::BootDiagnostics);

    device::domain::DiagnosticsSnapshot successSnapshot;
    successSnapshot.displayOk = true;
    successSnapshot.buttonsOk = true;
    successSnapshot.pn532Ok = true;
    successSnapshot.sdOk = true;
    successSnapshot.dbOk = true;
    successSnapshot.addDetailLine("one");
    successSnapshot.addDetailLine("two");
    successSnapshot.addDetailLine("three");
    successSnapshot.addDetailLine("four");
    successSnapshot.addDetailLine("five");
    controller.finishDiagnostics(successSnapshot);
    assert(controller.stateMachine().currentScreen() == device::ui::Screen::DiagnosticsSuccess);

    const auto scrollAction = controller.handleButtonEvent({Button::Down, ButtonPressType::ShortPress});
    assert(scrollAction == device::ui::AppStateMachine::Action::None);
    assert(controller.stateMachine().detailScrollOffset() == 1);

    const auto continueAction = controller.handleButtonEvent({Button::Ok, ButtonPressType::ShortPress});
    assert(continueAction == device::ui::AppStateMachine::Action::EnterModeMenu);
    assert(controller.stateMachine().currentScreen() == device::ui::Screen::ModeMenu);

    const auto modeMoveAction = controller.handleButtonEvent({Button::Down, ButtonPressType::ShortPress});
    assert(modeMoveAction == device::ui::AppStateMachine::Action::None);
    assert(controller.stateMachine().modeMenuSelection() == device::ui::ModeMenuSelection::Receipt);

    const auto modeMoveProvisionAction = controller.handleButtonEvent({Button::Down, ButtonPressType::ShortPress});
    assert(modeMoveProvisionAction == device::ui::AppStateMachine::Action::None);
    assert(controller.stateMachine().modeMenuSelection() == device::ui::ModeMenuSelection::Shipment);

    const auto modeMoveProvisionAction2 = controller.handleButtonEvent({Button::Down, ButtonPressType::ShortPress});
    assert(modeMoveProvisionAction2 == device::ui::AppStateMachine::Action::None);
    assert(controller.stateMachine().modeMenuSelection() == device::ui::ModeMenuSelection::Provision);

    const auto enterProvisionAction = controller.handleButtonEvent({Button::Ok, ButtonPressType::ShortPress});
    assert(enterProvisionAction == device::ui::AppStateMachine::Action::EnterProvisionMode);
    assert(controller.stateMachine().currentScreen() == device::ui::Screen::NfcProvision);

    device::domain::ProvisionSnapshot provisionSnapshot;
    provisionSnapshot.stage = device::domain::ProvisionStage::Success;
    provisionSnapshot.success = true;
    provisionSnapshot.addDetailLine("Provision complete");
    provisionSnapshot.addDetailLine("UUID: 123e4567-e89b-12d3-a456-426614174000");
    provisionSnapshot.addDetailLine("extra");
    provisionSnapshot.addDetailLine("extra2");
    provisionSnapshot.addDetailLine("extra3");
    controller.updateProvisionSnapshot(provisionSnapshot);

    const auto provisionScrollAction = controller.handleButtonEvent({Button::Down, ButtonPressType::ShortPress});
    assert(provisionScrollAction == device::ui::AppStateMachine::Action::None);
    assert(controller.stateMachine().detailScrollOffset() == 1);

    const auto resetProvisionAction = controller.handleButtonEvent({Button::Ok, ButtonPressType::ShortPress});
    assert(resetProvisionAction == device::ui::AppStateMachine::Action::ResetProvisionFlow);

    const auto provisionBackAction = controller.handleButtonEvent({Button::Ok, ButtonPressType::LongPress});
    assert(provisionBackAction == device::ui::AppStateMachine::Action::ReturnToModeMenu);
    assert(controller.stateMachine().currentScreen() == device::ui::Screen::ModeMenu);

    const auto modeMoveBackAction = controller.handleButtonEvent({Button::Up, ButtonPressType::ShortPress});
    assert(modeMoveBackAction == device::ui::AppStateMachine::Action::None);
    assert(controller.stateMachine().modeMenuSelection() == device::ui::ModeMenuSelection::Shipment);

    const auto enterReadAction = controller.handleButtonEvent({Button::Ok, ButtonPressType::ShortPress});
    assert(enterReadAction == device::ui::AppStateMachine::Action::EnterShipmentMode);
    assert(controller.stateMachine().currentScreen() == device::ui::Screen::InventoryShipment);

    device::domain::ShipmentSnapshot shipmentSnapshot;
    shipmentSnapshot.stage = device::domain::ShipmentStage::Success;
    shipmentSnapshot.success = true;
    shipmentSnapshot.addDetailLine("Local shipment saved");
    shipmentSnapshot.addDetailLine("SKU 1001 Item");
    shipmentSnapshot.addDetailLine("extra");
    shipmentSnapshot.addDetailLine("extra2");
    shipmentSnapshot.addDetailLine("extra3");
    controller.updateShipmentSnapshot(shipmentSnapshot);

    const auto liveScrollAction = controller.handleButtonEvent({Button::Down, ButtonPressType::ShortPress});
    assert(liveScrollAction == device::ui::AppStateMachine::Action::None);
    assert(controller.stateMachine().detailScrollOffset() == 1);

    const auto resetAction = controller.handleButtonEvent({Button::Ok, ButtonPressType::ShortPress});
    assert(resetAction == device::ui::AppStateMachine::Action::ResetShipmentFlow);

    const auto backAction = controller.handleButtonEvent({Button::Ok, ButtonPressType::LongPress});
    assert(backAction == device::ui::AppStateMachine::Action::ReturnToModeMenu);
    assert(controller.stateMachine().currentScreen() == device::ui::Screen::ModeMenu);
}

void test_diagnostics_presenter_row_mapping() {
    device::domain::DiagnosticsSnapshot snapshot;
    assert(device::ui::DiagnosticsPresenter::displayRowState(snapshot) == device::ui::DiagnosticsRowState::Pending);
    snapshot.displayOk = true;
    snapshot.buttonsOk = true;
    snapshot.dbOk = true;
    assert(device::ui::DiagnosticsPresenter::displayRowState(snapshot) == device::ui::DiagnosticsRowState::Ok);
    snapshot.failureCode = device::domain::ErrorCode::SdMount;
    assert(device::ui::DiagnosticsPresenter::sdRowState(snapshot) == device::ui::DiagnosticsRowState::Fail);
    assert(device::ui::DiagnosticsPresenter::pn532RowState(snapshot) == device::ui::DiagnosticsRowState::Pending);
    snapshot.failureCode = device::domain::ErrorCode::DbInit;
    snapshot.dbOk = false;
    assert(device::ui::DiagnosticsPresenter::dbRowState(snapshot) == device::ui::DiagnosticsRowState::Fail);
    assert(device::ui::DiagnosticsPresenter::clampDetailOffset(5, 3) == 0);
    assert(device::ui::DiagnosticsPresenter::clampDetailOffset(5, 8) == 4);
}

void test_sync_state_transitions() {
    device::app::AppController controller;
    controller.startDiagnostics();

    device::domain::DiagnosticsSnapshot successSnapshot;
    successSnapshot.displayOk = true;
    successSnapshot.buttonsOk = true;
    successSnapshot.pn532Ok = true;
    successSnapshot.sdOk = true;
    successSnapshot.dbOk = true;
    controller.finishDiagnostics(successSnapshot);

    assert(controller.handleButtonEvent({Button::Ok, ButtonPressType::ShortPress}) == device::ui::AppStateMachine::Action::EnterModeMenu);
    assert(controller.stateMachine().currentScreen() == device::ui::Screen::ModeMenu);

    assert(controller.handleButtonEvent({Button::Down, ButtonPressType::ShortPress}) == device::ui::AppStateMachine::Action::None);
    assert(controller.handleButtonEvent({Button::Down, ButtonPressType::ShortPress}) == device::ui::AppStateMachine::Action::None);
    assert(controller.handleButtonEvent({Button::Down, ButtonPressType::ShortPress}) == device::ui::AppStateMachine::Action::None);
    assert(controller.handleButtonEvent({Button::Down, ButtonPressType::ShortPress}) == device::ui::AppStateMachine::Action::None);
    assert(controller.stateMachine().modeMenuSelection() == device::ui::ModeMenuSelection::Sync);

    assert(controller.handleButtonEvent({Button::Ok, ButtonPressType::ShortPress}) == device::ui::AppStateMachine::Action::EnterSyncStatus);
    assert(controller.stateMachine().currentScreen() == device::ui::Screen::SyncStatus);

    assert(controller.handleButtonEvent({Button::Ok, ButtonPressType::ShortPress}) == device::ui::AppStateMachine::Action::None);
    assert(controller.stateMachine().currentScreen() == device::ui::Screen::SyncStatus);

    assert(controller.handleButtonEvent({Button::Ok, ButtonPressType::LongPress}) == device::ui::AppStateMachine::Action::ReturnToModeMenu);
    assert(controller.stateMachine().currentScreen() == device::ui::Screen::ModeMenu);
}

}  // namespace

int main() {
    test_uuid_normalization_and_ndef_round_trip();
    test_invalid_uuid_is_rejected();
    test_blank_and_legacy_tag_detection();
    test_event_and_error_mappings();
    test_quantity_snapshots_and_limits();
    test_keypad_short_and_long_press();
    test_offline_queue_serialization_round_trip();
    test_diagnostics_state_transitions_and_scroll();
    test_diagnostics_presenter_row_mapping();
    test_sync_state_transitions();

    std::cout << "device core tests passed\n";
    return 0;
}

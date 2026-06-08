#pragma once

#include <array>
#include <cstdint>

namespace device::config {

constexpr const char* kFirmwareName = "warehouse-terminal";
constexpr const char* kFirmwareVersion = "0.6.2";

// Master switch for the WiFi fallback link. When false the device never
// starts the WiFi radio and never issues requests over WiFi — it works over
// Ethernet only (or fully offline). Set back to true to re-enable WiFi.
constexpr bool kWifiEnabled = false;

constexpr const char* kDefaultWifiSsid = "off_Bodnar_iot";
constexpr const char* kDefaultWifiPassword = "0664995352";
constexpr const char* kDefaultApiBaseUrl = "http://192.168.110.187:8000/api/v1";
constexpr const char* kDeviceIdPrefix = "esp32s3-";
constexpr uint32_t kDefaultProductSyncIntervalMs = 300000UL;
constexpr uint32_t kDefaultOperationSyncIntervalMs = 15000UL;
constexpr uint32_t kHttpConnectTimeoutMs = 900UL;
constexpr uint32_t kHttpRequestTimeoutMs = 1200UL;
// The W5200 TCP handshake is flaky on this wiring/path: a single connect often
// misses. Retry the connect this many times within one request so it reliably
// lands and the request actually reaches the backend.
constexpr uint32_t kHttpConnectAttempts = 12UL;
constexpr uint32_t kHttpConnectAttemptTimeoutMs = 800UL;

constexpr uint32_t kOperatorSessionTimeoutMs = 5UL * 60UL * 1000UL;
constexpr uint32_t kLongPressThresholdMs = 700UL;
constexpr uint32_t kDebounceMs = 50UL;
constexpr uint32_t kSyncBackoffBaseMs = 2000UL;
// Retry interval after a transport miss (connect/timeout) — fast, because the
// request never reached the backend; the exponential backoff above is reserved
// for genuine backend rejections.
constexpr uint32_t kOperationTransportRetryMs = 3000UL;
constexpr uint32_t kHeartbeatIntervalMs = 500UL;
constexpr uint32_t kNfcPollTimeoutMs = 40UL;
constexpr uint8_t kNfcPageWriteRetries = 3;
constexpr uint32_t kDatabaseSchemaVersion = 5UL;
constexpr uint32_t kDatabaseResetNonce = 20260329UL;
constexpr std::size_t kMaxRecentProducts = 6;
constexpr std::size_t kReceiptVisibleProducts = 5;
constexpr std::size_t kSkuDigits = 4;
constexpr std::size_t kQuantityDigits = 5;
constexpr uint32_t kMinTagQuantity = 1UL;
constexpr uint32_t kMaxTagQuantity = 99999UL;
constexpr std::size_t kMaxOfflineOperations = 32;
constexpr std::size_t kMaxPayloadBytes = 128;
constexpr std::size_t kMaxDetailLines = 10;
constexpr std::size_t kVisibleDetailLines = 4;
constexpr uint8_t kType2StartPage = 4;
constexpr uint8_t kType2EndPage = 15;
constexpr uint8_t kLegacyEndPage = 11;
constexpr const char* kDatabasePath = "/sd/warehouse.db";
constexpr const char* kDatabaseSdFile = "/warehouse.db";
constexpr const char* kDatabaseResetMarkerFile = "/db_reset_nonce.txt";

enum class Button : uint8_t {
    Up,
    Down,
    Ok,
};

}

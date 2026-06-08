#pragma once

#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>

#include <array>
#include <cstdio>
#include <string_view>

#include "config/DeviceConfig.h"
#include "domain/Models.h"
#include "drivers/NetworkManager.h"

namespace device::drivers {

class DeviceApiClient {
public:
    void setNetworkManager(NetworkManager* network) { network_ = network; }

    int lastHttpCode() const { return lastHttpCode_; }

    bool authenticateDevice(
        const device::domain::DeviceBootstrapConfig& config,
        FixedString<256>& accessToken,
        FixedString<48>& detail
    ) {
        const uint32_t startedAtMs = millis();
        detail.clear();
        accessToken.clear();

        JsonDocument request;
        request["device_id"] = config.deviceId.c_str();
        String requestBody;
        serializeJson(request, requestBody);

        String responseBody;
        const int httpCode = postJson(
            config.apiBaseUrl.view(), "/device/auth/login",
            requestBody, responseBody, {});

        Serial.printf("[net] device auth code=%d duration=%lu ms\n",
            httpCode, static_cast<unsigned long>(millis() - startedAtMs));

        if (httpCode != 200) {
            detail.assign(httpCode < 0 ? "Device auth unavailable" : "Device auth failed");
            return false;
        }

        JsonDocument response;
        if (deserializeJson(response, responseBody) != DeserializationError::Ok) {
            detail.assign("Device auth parse failed");
            return false;
        }

        const char* token = response["access_token"] | "";
        if (token == nullptr || *token == '\0' || !accessToken.assign(token)) {
            detail.assign("Device token missing");
            return false;
        }

        detail.assign("Device auth OK");
        return true;
    }

    template <std::size_t N>
    bool fetchRecentProducts(
        std::string_view apiBaseUrl,
        std::string_view deviceToken,
        std::array<device::domain::ProductRecord, N>& products,
        std::size_t& count,
        FixedString<48>& detail
    ) {
        const uint32_t startedAtMs = millis();
        count = 0;
        detail.clear();
        for (auto& p : products) p.clear();

        String responseBody;
        const int httpCode = getJson(
            apiBaseUrl, "/device/products/recent?limit=6",
            responseBody, deviceToken);

        Serial.printf("[net] product sync code=%d duration=%lu ms\n",
            httpCode, static_cast<unsigned long>(millis() - startedAtMs));

        if (httpCode != 200) {
            detail.assign(httpCode == 401 ? "Device token expired"
                : (httpCode < 0 ? "Product sync unavailable" : "Product sync failed"));
            return false;
        }

        JsonDocument response;
        if (deserializeJson(response, responseBody) != DeserializationError::Ok
                || !response["products"].is<JsonArray>()) {
            detail.assign("Product sync parse fail");
            return false;
        }

        JsonArray items = response["products"].as<JsonArray>();
        for (JsonVariant item : items) {
            if (count >= N || !item.is<JsonObject>()) break;
            auto obj = item.as<JsonObject>();
            auto& product = products[count];
            product.productId = obj["id"] | 0U;
            const uint32_t sku = obj["sku"] | 0U;
            const char* name = obj["name"] | "";
            if (product.productId == 0 || sku == 0 || name == nullptr || *name == '\0') continue;
            product.exists   = true;
            product.isActive = true;
            product.sku      = sku;
            if (!product.name.assign(name)) continue;
            ++count;
        }

        detail.assign(count == 0 ? "Products synced: 0" : "Products synced");
        return true;
    }

    bool syncInventoryOperation(
        std::string_view apiBaseUrl,
        std::string_view deviceToken,
        const device::domain::InventoryOperationRecord& operation,
        FixedString<48>& detail
    ) {
        const uint32_t startedAtMs = millis();
        detail.clear();

        if (operation.operationType == device::domain::InventoryOperationType::ProvisionTag) {
            JsonDocument request;
            request["tag_uid"]      = operation.tagUuid.c_str();
            request["chip_uid_hex"] = operation.uidHex.c_str();
            String requestBody;
            serializeJson(request, requestBody);

            String responseBody;
            const int httpCode = postJson(
                apiBaseUrl, "/device/tags/provision",
                requestBody, responseBody, deviceToken);

            Serial.printf("[net] provision sync code=%d duration=%lu ms\n",
                httpCode, static_cast<unsigned long>(millis() - startedAtMs));

            if (httpCode != 200) {
                if (httpCode == 401)      detail.assign("Device auth expired");
                else if (httpCode == 409) detail.assign("Provision conflict");
                else if (httpCode == 422) detail.assign("Provision validation");
                else detail.assign(httpCode < 0 ? "Provision sync down" : "Provision sync failed");
                return false;
            }

            JsonDocument response;
            if (deserializeJson(response, responseBody) != DeserializationError::Ok) {
                detail.assign("Provision parse fail");
                return false;
            }

            detail.assign((response["reissued"] | false) ? "Provision reissued" : "Provision synced");
            return true;
        }

        const char* operationType = device::domain::toSyncOperationType(operation.operationType);
        if (operationType == nullptr || *operationType == '\0') {
            detail.assign("Unsupported sync op");
            lastHttpCode_ = 422;
            return false;
        }

        JsonDocument request;
        JsonArray operations = request["operations"].to<JsonArray>();
        JsonObject item      = operations.add<JsonObject>();
        item["client_operation_id"] = operation.clientOperationId.c_str();
        item["operation_type"]      = operationType;
        JsonObject payload          = item["payload"].to<JsonObject>();
        payload["tag_uid"]          = operation.tagUuid.c_str();
        if (operation.operationType == device::domain::InventoryOperationType::ReceiptUnit) {
            payload["product_id"]   = operation.productId;
            payload["quantity"]     = operation.quantity;
            payload["chip_uid_hex"] = operation.uidHex.c_str();
        } else if (operation.operationType == device::domain::InventoryOperationType::ShipmentPartialUnit) {
            payload["quantity"] = operation.quantity;
            payload["note"]     = operation.message.c_str();
        } else {
            payload["note"] = operation.message.c_str();
        }

        String requestBody;
        serializeJson(request, requestBody);

        String responseBody;
        const int httpCode = postJson(
            apiBaseUrl, "/device/operations/sync",
            requestBody, responseBody, deviceToken);

        Serial.printf("[net] operations sync code=%d duration=%lu ms\n",
            httpCode, static_cast<unsigned long>(millis() - startedAtMs));

        if (httpCode != 200) {
            if (httpCode == 401)      detail.assign("Device auth expired");
            else if (httpCode == 404) detail.assign("Sync target missing");
            else if (httpCode == 409) detail.assign("Sync conflict");
            else if (httpCode == 422) detail.assign("Sync validation");
            else detail.assign(httpCode < 0 ? "Sync unavailable" : "Sync failed");
            return false;
        }

        JsonDocument response;
        if (deserializeJson(response, responseBody) != DeserializationError::Ok
                || !response["results"].is<JsonArray>()) {
            detail.assign("Sync parse fail");
            return false;
        }

        detail.assign("Sync applied");
        return true;
    }

private:
    NetworkManager* network_ = nullptr;
    int lastHttpCode_ = 0;

    struct ParsedUrl {
        String host;
        uint16_t port = 80;
        String basePath;
        bool https = false;
    };

    // Read the response body without relying on the library's Content-Length
    // handling: uvicorn sends "content-length" in lowercase and
    // ArduinoHttpClient matches it case-sensitively, so responseBody() never
    // learns the length and blocks for the full read timeout (~2 s per call).
    // Every request is sent with "Connection: close", so the server closes the
    // socket right after the body — read until that close instead, with a short
    // idle guard only as a safety net.
    static void readResponseBody(HttpClient& http, String& out) {
        http.skipResponseHeaders();
        out = "";
        uint32_t lastDataMs = millis();
        while ((http.connected() || http.available()) && (millis() - lastDataMs) < 500) {
            if (http.available()) {
                out += static_cast<char>(http.read());
                lastDataMs = millis();
            }
        }
    }

    static ParsedUrl parseBaseUrl(std::string_view url) {
        ParsedUrl result;
        const String s(url.data(), static_cast<unsigned int>(url.size()));

        int hostStart = 0;
        if (s.startsWith("https://")) {
            hostStart    = 8;
            result.port  = 443;
            result.https = true;
        } else if (s.startsWith("http://")) {
            hostStart = 7;
        }

        const int slashPos = s.indexOf('/', hostStart);
        const String hostPort = slashPos >= 0
            ? s.substring(hostStart, slashPos)
            : s.substring(hostStart);
        result.basePath = slashPos >= 0 ? s.substring(slashPos) : String("/");
        if (result.basePath.endsWith("/")) {
            result.basePath.remove(result.basePath.length() - 1);
        }

        const int colonPos = hostPort.lastIndexOf(':');
        if (colonPos >= 0) {
            result.host = hostPort.substring(0, colonPos);
            result.port = static_cast<uint16_t>(hostPort.substring(colonPos + 1).toInt());
        } else {
            result.host = hostPort;
        }

        return result;
    }

    int getJson(std::string_view apiBaseUrl, const char* suffix,
                String& responseBody, std::string_view bearerToken) {
        if (!network_) { lastHttpCode_ = -1; return -1; }

        const auto parsed = parseBaseUrl(apiBaseUrl);
        const String path = parsed.basePath + suffix;

        network_->prepareSpi();
        Client* client = parsed.https
            ? network_->activeSecureClient()
            : network_->activeClient();

        // The W5200 TCP handshake misses often on this wiring/path, so a single
        // connect frequently fails even though the network is fine (DHCP works
        // because it retries internally for 8s). Retry the whole request until
        // the connect lands and the backend answers. Each request uses a fresh
        // "Connection: close" connection so the body read ends cleanly on EOF.
        client->setTimeout(device::config::kHttpConnectAttemptTimeoutMs);
        lastHttpCode_ = -1;
        for (uint32_t attempt = 0; attempt < device::config::kHttpConnectAttempts; ++attempt) {
            HttpClient http(*client, parsed.host, parsed.port);
            http.setTimeout(2000);
            http.setHttpResponseTimeout(5000);

            http.beginRequest();
            if (http.get(path) != 0) {        // connect miss — retry
                http.stop();
                delay(40);
                continue;
            }
            if (!bearerToken.empty()) {
                http.sendHeader("Authorization",
                    String("Bearer ") + String(bearerToken.data(), bearerToken.size()));
            }
            http.endRequest();

            lastHttpCode_ = http.responseStatusCode();
            if (lastHttpCode_ > 0) readResponseBody(http, responseBody);
            http.stop();

            if (lastHttpCode_ > 0) break;     // reached the backend — done
            delay(40);                        // transport error — retry
        }
        network_->restoreSpi();
        return lastHttpCode_;
    }

    int postJson(std::string_view apiBaseUrl, const char* suffix,
                 const String& requestBody, String& responseBody,
                 std::string_view bearerToken) {
        if (!network_) { lastHttpCode_ = -1; return -1; }

        const auto parsed = parseBaseUrl(apiBaseUrl);
        const String path = parsed.basePath + suffix;

        network_->prepareSpi();
        Client* client = parsed.https
            ? network_->activeSecureClient()
            : network_->activeClient();

        // No keep-alive: each request uses a fresh connection with
        // "Connection: close". On the LAN a TCP handshake costs ~1 ms, while
        // keep-alive caused the server-closed socket to look half-open to the
        // W5200 — every request then either stalled for the full stream timeout
        // (~2 s waiting for an EOF that never came) or hung 30 s on a dead
        // socket. With close, the server ends the response and we get EOF at once.
        // Give the W5200 up to ~3.5s to finish the TCP handshake instead of the
        // 1s default — on a lossy/slow path the connect needs several SYN
        // retransmissions to land (DHCP survives for the same reason).
        client->setTimeout(3500);
        HttpClient http(*client, parsed.host, parsed.port);
        http.setTimeout(2000);
        http.setHttpResponseTimeout(5000);  // bound a stalled response (was 30s)

        http.beginRequest();
        const int reqErr = http.post(path);
        if (reqErr != 0) {
            // Connect failed — fail fast instead of waiting out the response
            // timeout. The W5200 connect intermittently misses (e.g. ARP not yet
            // resolved); the op stays queued and the next retry usually lands.
            http.stop();
            network_->restoreSpi();
            lastHttpCode_ = reqErr;
            return reqErr;
        }
        http.sendHeader("Content-Type", "application/json");
        http.sendHeader("Content-Length", String(requestBody.length()));
        if (!bearerToken.empty()) {
            http.sendHeader("Authorization",
                String("Bearer ") + String(bearerToken.data(), bearerToken.size()));
        }
        http.beginBody();
        http.print(requestBody);
        http.endRequest();

        lastHttpCode_ = http.responseStatusCode();
        if (lastHttpCode_ > 0) readResponseBody(http, responseBody);

        http.stop();  // always close — frees the W5200 socket, no leak
        network_->restoreSpi();

        return lastHttpCode_;
    }
};

}

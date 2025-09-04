#include "ota.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>
#include "mqtt.h"
#include "config.h"    // Para FIRMWARE_VERSION
#include "helpers.h"   // Funções auxiliares

const char* versionURL  = "https://raw.githubusercontent.com/sergio-isidoro/Wake-on-LAN_ESP32C3/main/firmware/version.txt";
const char* firmwareURL = "https://raw.githubusercontent.com/sergio-isidoro/Wake-on-LAN_ESP32C3/main/firmware/WOL_ESP32C3.bin";
const char* shaURL      = "https://raw.githubusercontent.com/sergio-isidoro/Wake-on-LAN_ESP32C3/main/firmware/WOL_ESP32C3.bin.sha256";

// ----------------------------
// SHA256 calculation function
// ----------------------------
String calculateSHA256(const char* path) {
    if (!SPIFFS.exists(path)) return "";
    File f = SPIFFS.open(path, "r");
    if (!f) return "";

    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);

    uint8_t buf[1024];
    while (f.available()) {
        size_t len = f.read(buf, sizeof(buf));
        mbedtls_sha256_update(&ctx, buf, len);
    }
    f.close();

    uint8_t hash[32];
    mbedtls_sha256_finish(&ctx, hash);
    mbedtls_sha256_free(&ctx);

    String shaStr;
    for (int i = 0; i < 32; i++) {
        if (hash[i] < 16) shaStr += "0";
        shaStr += String(hash[i], HEX);
    }
    shaStr.toUpperCase();
    return shaStr;
}

// ----------------------------
// OTA function
// ----------------------------
void performOTA() {
    mqttPublish("OTA: Checking for updates...");

    if (!SPIFFS.begin(true)) {
        mqttPublish("OTA: SPIFFS init failed!");
        return;
    }

    // ----------------------------
    // 1. Fetch remote version
    // ----------------------------
    String remoteVer;
    bool versionFetched = false;
    for (int attempt = 1; attempt <= MAX_OTA_RETRIES; attempt++) {
        WiFiClientSecure clientVer;
        clientVer.setInsecure();
        HTTPClient httpVer;
        httpVer.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        if (httpVer.begin(clientVer, versionURL)) {
            int code = httpVer.GET();
            if (code == HTTP_CODE_OK) {
                remoteVer = httpVer.getString();
                remoteVer.trim();
                mqttPublish(("OTA: Remote version found: " + remoteVer).c_str());
                versionFetched = true;
                httpVer.end();
                break;
            } else {
                mqttPublish(("OTA: Failed to get version (attempt " + String(attempt) + "), HTTP code: " + String(code)).c_str());
            }
            httpVer.end();
        } else {
            mqttPublish("OTA: HTTP begin failed for version.");
        }
        delay(1000);
    }

    if (!versionFetched) {
        mqttPublish("OTA: Cannot fetch remote version, aborting.");
        return;
    }

    if (remoteVer == FIRMWARE_VERSION) {
        mqttPublish("OTA: Already at latest version.");
        return;
    }

    mqttPublish(("OTA: New version available: " + remoteVer).c_str());

    // ----------------------------
    // 2. Download firmware to SPIFFS
    // ----------------------------
    for (int attempt = 1; attempt <= MAX_OTA_RETRIES; attempt++) {
        WiFiClientSecure client;
        client.setInsecure();
        HTTPClient http;
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        http.begin(client, firmwareURL);
        http.setTimeout(15000);

        int httpCode = http.GET();
        if (httpCode != HTTP_CODE_OK) {
            mqttPublish(("OTA: Firmware download failed (attempt " + String(attempt) + "), HTTP code: " + String(httpCode)).c_str());
            http.end();
            delay(2000);
            continue;
        }

        int contentLength = http.getSize();
        if (contentLength <= 0) {
            mqttPublish("OTA: Invalid firmware size.");
            http.end();
            delay(2000);
            continue;
        }

        mqttPublish(("OTA: Firmware size: " + String(contentLength) + " bytes").c_str());

        // Remove old file if exists
        if (SPIFFS.exists(OTA_FILE_PATH)) SPIFFS.remove(OTA_FILE_PATH);

        File f = SPIFFS.open(OTA_FILE_PATH, "w");
        if (!f) {
            mqttPublish("OTA: Failed to open file for writing.");
            http.end();
            return;
        }

        auto* stream = http.getStreamPtr();
        uint8_t buf[1024];
        size_t totalWritten = 0;
        int lastPercent = -1;

        while (totalWritten < contentLength) {
            int len = stream->read(buf, sizeof(buf));
            if (len < 0) break;
            if (len == 0) { delay(100); continue; }

            if (f.write(buf, len) != len) {
                mqttPublish("OTA: File write failed.");
                break;
            }

            totalWritten += len;
            int percent = (totalWritten * 100) / contentLength;
            if (percent > lastPercent) {
                lastPercent = percent;
                mqttPublish(("OTA: Download progress " + String(percent) + "%").c_str());
            }
            delay(1);
        }
        f.close();
        http.end();

        if (totalWritten != contentLength) {
            mqttPublish(("OTA: Incomplete download (" + String(totalWritten) + " of " + String(contentLength) + " bytes), retrying...").c_str());
            SPIFFS.remove(OTA_FILE_PATH);
            delay(2000);
            continue;
        }

        mqttPublish("OTA: Firmware downloaded successfully.");
        break;
    }

    // ----------------------------
    // 3. Verify SHA256
    // ----------------------------
    for (int attempt = 1; attempt <= MAX_OTA_RETRIES; attempt++) {
        // Download SHA256 file
        WiFiClientSecure client;
        client.setInsecure();
        HTTPClient http;
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        http.begin(client, shaURL);
        int code = http.GET();
        if (code != HTTP_CODE_OK) {
            mqttPublish(("OTA: Failed to download SHA256 (attempt " + String(attempt) + "), code: " + String(code)).c_str());
            http.end();
            delay(2000);
            continue;
        }

        String remoteSHA = http.getString();
        remoteSHA.trim();
        http.end();

        String localSHA = calculateSHA256(OTA_FILE_PATH);

        if (localSHA != remoteSHA) {
            mqttPublish(("OTA: SHA256 mismatch! Local: " + localSHA + ", Remote: " + remoteSHA).c_str());
            SPIFFS.remove(OTA_FILE_PATH);
            delay(2000);
            continue;
        }

        mqttPublish("OTA: SHA256 verified successfully.");
        break;
    }

    // ----------------------------
    // 4. Apply OTA
    // ----------------------------
    File fOTA = SPIFFS.open(OTA_FILE_PATH, "r");
    if (!fOTA) {
        mqttPublish("OTA: Cannot open firmware for OTA.");
        return;
    }

    const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        mqttPublish("OTA: OTA partition not found.");
        return;
    }

    esp_ota_handle_t ota_handle;
    if (esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle) != ESP_OK) {
        mqttPublish("OTA: esp_ota_begin failed.");
        return;
    }

    uint8_t buf[1024];
    size_t written = 0;
    while (fOTA.available()) {
        int len = fOTA.read(buf, sizeof(buf));
        if (len <= 0) break;
        if (esp_ota_write(ota_handle, buf, len) != ESP_OK) {
            mqttPublish("OTA: esp_ota_write failed.");
            esp_ota_abort(ota_handle);
            fOTA.close();
            return;
        }
        written += len;
        int percent = (written * 100) / fOTA.size();
        mqttPublish(("OTA: Writing to flash " + String(percent) + "%").c_str());
        delay(1);
    }
    fOTA.close();

    if (esp_ota_end(ota_handle) != ESP_OK) {
        mqttPublish("OTA: esp_ota_end failed.");
        return;
    }

    if (esp_ota_set_boot_partition(update_partition) != ESP_OK) {
        mqttPublish("OTA: Failed to set boot partition.");
        return;
    }

    mqttPublish("OTA: Update applied successfully! Restarting...");
    SPIFFS.remove(OTA_FILE_PATH); // Clean up
    delay(2000);
    ESP.restart();
}

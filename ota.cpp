#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <esp_ota_ops.h>
#include <FS.h>
#include <SPIFFS.h>
#include <mbedtls/sha256.h>
#include "mqtt.h"
#include "config.h"
#include "helpers.h"

#define OTA_FILE_PATH "/firmware.bin"

String remoteSHA256 = ""; // Para armazenar SHA remoto
String localSHA256  = ""; // Para armazenar SHA calculado localmente

const char* versionURL  = "https://raw.githubusercontent.com/sergio-isidoro/Wake-on-LAN_ESP32C3/main/firmware/version.txt";
const char* firmwareURL = "https://raw.githubusercontent.com/sergio-isidoro/Wake-on-LAN_ESP32C3/main/firmware/WOL_ESP32C3.bin";
const char* sha256URL   = "https://raw.githubusercontent.com/sergio-isidoro/Wake-on-LAN_ESP32C3/main/firmware/WOL_ESP32C3.sha256";

#define MAX_OTA_RETRIES 3

String calculateSHA256(const char* path) {
    if (!SPIFFS.begin(true)) return "";

    File f = SPIFFS.open(path, FILE_READ);
    if (!f) return "";

    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0); // 0 = SHA256

    uint8_t buf[1024];
    while (f.available()) {
        size_t len = f.read(buf, sizeof(buf));
        mbedtls_sha256_update(&ctx, buf, len);
    }

    uint8_t hash[32];
    mbedtls_sha256_finish(&ctx, hash);
    mbedtls_sha256_free(&ctx);
    f.close();

    String hashStr;
    for (int i = 0; i < 32; i++) {
        if (hash[i] < 16) hashStr += "0";
        hashStr += String(hash[i], HEX);
    }
    hashStr.toUpperCase();
    return hashStr;
}

void performOTA() {
    mqttPublish("OTA: Checking for updates...");

    // ---------------------------
    // 1. Get remote version
    // ---------------------------
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
                mqttPublish(("OTA: Remote version: " + remoteVer).c_str());
                versionFetched = true;
                httpVer.end();
                break;
            } else {
                mqttPublish(("OTA: Failed to get version, HTTP code: " + String(code)).c_str());
            }
            httpVer.end();
        } else {
            mqttPublish("OTA: Failed to start HTTP for version check.");
        }
        delay(1000);
    }
    if (!versionFetched) {
        mqttPublish("OTA: Cannot fetch remote version. Aborting.");
        return;
    }

    if (remoteVer == FIRMWARE_VERSION) {
        mqttPublish("OTA: Firmware already up to date.");
        return;
    }

    mqttPublish(("OTA: New version available: " + remoteVer).c_str());

    // ---------------------------
    // 2. Download firmware to SPIFFS
    // ---------------------------
    for (int attempt = 1; attempt <= MAX_OTA_RETRIES; attempt++) {
        WiFiClientSecure client;
        client.setInsecure();

        HTTPClient http;
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        http.begin(client, firmwareURL);
        http.setTimeout(15000);

        int httpCode = http.GET();
        if (httpCode != HTTP_CODE_OK) {
            mqttPublish(("OTA: Download failed, HTTP code: " + String(httpCode)).c_str());
            http.end();
            delay(2000);
            continue;
        }

        int contentLength = http.getSize();
        if (contentLength <= 0) {
            mqttPublish("OTA: Invalid content length.");
            http.end();
            delay(2000);
            continue;
        }

        mqttPublish(("OTA: Firmware size: " + String(contentLength) + " bytes").c_str());

        // Remove old file if exists
        if (SPIFFS.exists(OTA_FILE_PATH)) SPIFFS.remove(OTA_FILE_PATH);

        File fw = SPIFFS.open(OTA_FILE_PATH, FILE_WRITE);
        if (!fw) {
            mqttPublish("OTA: Cannot open file for writing.");
            http.end();
            return;
        }

        // Stream download and compute SHA256
        mbedtls_sha256_context shaCtx;
        mbedtls_sha256_init(&shaCtx);
        mbedtls_sha256_starts(&shaCtx, 0);

        auto* stream = http.getStreamPtr();
        uint8_t buf[1024];
        size_t totalWritten = 0;
        int lastPercent = -1;

        while (totalWritten < contentLength) {
            int len = stream->read(buf, sizeof(buf));
            if (len < 0) {
                mqttPublish("OTA: Error reading stream.");
                break;
            }
            if (len == 0) {
                delay(1);
                continue;
            }

            fw.write(buf, len);
            mbedtls_sha256_update(&shaCtx, buf, len);
            totalWritten += len;

            int percent = (totalWritten * 100) / contentLength;
            if (percent / 5 > lastPercent / 5) { // log every 5%
                lastPercent = percent;
                mqttPublish(("OTA: Downloaded " + String(totalWritten) + " bytes... (" + String(percent) + "%)").c_str());
            }
            delay(1);
        }

        fw.close();
        http.end();

        if (totalWritten != contentLength) {
            mqttPublish(("OTA: Incomplete download, retrying...").c_str());
            delay(2000);
            continue;
        }

        // Finish SHA256
        uint8_t hash[32];
        mbedtls_sha256_finish(&shaCtx, hash);
        mbedtls_sha256_free(&shaCtx);

        localSHA256 = "";
        for (int i = 0; i < 32; i++) {
            if (hash[i] < 16) localSHA256 += "0";
            localSHA256 += String(hash[i], HEX);
        }
        localSHA256.toUpperCase();

        mqttPublish(("OTA: Finished download. Local SHA256: " + localSHA256).c_str());

        // ---------------------------
        // 3. Apply OTA
        // ---------------------------
        const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);
        if (!update_partition) {
            mqttPublish("OTA: Cannot find OTA partition.");
            return;
        }

        File fwFile = SPIFFS.open(OTA_FILE_PATH, FILE_READ);
        if (!fwFile) {
            mqttPublish("OTA: Cannot open SPIFFS firmware file.");
            return;
        }

        esp_ota_handle_t ota_handle;
        if (esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle) != ESP_OK) {
            mqttPublish("OTA: esp_ota_begin failed.");
            return;
        }

        while (fwFile.available()) {
            int len = fwFile.read(buf, sizeof(buf));
            if (esp_ota_write(ota_handle, buf, len) != ESP_OK) {
                mqttPublish("OTA: esp_ota_write failed.");
                esp_ota_abort(ota_handle);
                fwFile.close();
                return;
            }
        }

        fwFile.close();

        if (esp_ota_end(ota_handle) != ESP_OK) {
            mqttPublish("OTA: esp_ota_end failed.");
            return;
        }

        if (esp_ota_set_boot_partition(update_partition) != ESP_OK) {
            mqttPublish("OTA: esp_ota_set_boot_partition failed.");
            return;
        }

        mqttPublish("OTA: Update complete. Restarting...");
        delay(2000);
        ESP.restart();
        return;
    }

    mqttPublish("OTA: Update failed after max retries.");
}

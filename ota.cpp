#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <esp_ota_ops.h>
#include <SPIFFS.h>
#include "ota.h"
#include "mqtt.h"
#include "helpers.h"
#include "config.h"

const char* versionURL  = "https://raw.githubusercontent.com/sergio-isidoro/Wake-on-LAN_ESP32C3/main/firmware/version.txt";
const char* firmwareURL = "https://raw.githubusercontent.com/sergio-isidoro/Wake-on-LAN_ESP32C3/main/firmware/WOL_ESP32C3.bin";
const char* shaURL      = "https://raw.githubusercontent.com/sergio-isidoro/Wake-on-LAN_ESP32C3/main/firmware/WOL_ESP32C3.bin.sha256";

#define MAX_OTA_RETRIES 3
#define OTA_FILE_PATH "/firmware.bin"
#define SHA_FILE_PATH "/firmware.sha256"

void performOTA() {
    mqttPublish("OTA: Checking for updates...");

    if (!SPIFFS.begin(true)) {
        mqttPublish("OTA: SPIFFS mount failed.");
        return;
    }

    // -----------------------
    // 1️⃣ Get remote version
    // -----------------------
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
            mqttPublish("OTA: Failed to start HTTP for version check.");
        }
        delay(1000);
    }

    if (!versionFetched) {
        mqttPublish("OTA: Could not get remote version. Aborting.");
        return;
    }

    if (remoteVer == FIRMWARE_VERSION) {
        mqttPublish("OTA: Firmware is already up to date.");
        return;
    }

    mqttPublish(("OTA: New version " + remoteVer + " available. Preparing download...").c_str());

    // -----------------------
    // 2️⃣ Download firmware to SPIFFS
    // -----------------------
    for (int attempt = 1; attempt <= MAX_OTA_RETRIES; attempt++) {
        // Remove incomplete files
        if (SPIFFS.exists(OTA_FILE_PATH)) SPIFFS.remove(OTA_FILE_PATH);
        if (SPIFFS.exists(SHA_FILE_PATH)) SPIFFS.remove(SHA_FILE_PATH);

        WiFiClientSecure client;
        client.setInsecure();
        HTTPClient http;
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        http.begin(client, firmwareURL);
        http.setTimeout(15000);

        int httpCode = http.GET();
        if (httpCode != HTTP_CODE_OK) {
            mqttPublish(("OTA: Download failed (attempt " + String(attempt) + "), HTTP code: " + String(httpCode)).c_str());
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

        File fwFile = SPIFFS.open(OTA_FILE_PATH, FILE_WRITE);
        if (!fwFile) {
            mqttPublish("OTA: Failed to open SPIFFS file for writing.");
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
            if (len == 0) { delay(1); continue; }

            fwFile.write(buf, len);
            totalWritten += len;

            int percent = (totalWritten * 100) / contentLength;
            if (percent > lastPercent) {
                lastPercent = percent;
                mqttPublish(("OTA: Download progress: " + String(percent) + "%").c_str());
            }
            delay(1);
        }
        fwFile.close();
        http.end();

        if (totalWritten != contentLength) {
            mqttPublish(("OTA: Incomplete download. Retrying...").c_str());
            delay(2000);
            continue;
        }

        mqttPublish("OTA: Firmware downloaded successfully.");
        break; // Download successful
    }

    // -----------------------
    // 3️⃣ Download SHA256
    // -----------------------
    WiFiClientSecure clientSha;
    clientSha.setInsecure();
    HTTPClient httpSha;
    httpSha.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    httpSha.begin(clientSha, shaURL);
    int codeSha = httpSha.GET();
    if (codeSha != HTTP_CODE_OK) {
        mqttPublish(("OTA: Failed to download SHA256, HTTP code: " + String(codeSha)).c_str());
        httpSha.end();
        return;
    }

    String shaRemote = httpSha.getString();
    shaRemote.trim();
    httpSha.end();
    mqttPublish(("OTA: SHA256 downloaded: " + shaRemote).c_str());

    // -----------------------
    // 4️⃣ Verify SHA256
    // -----------------------
    String shaLocal = calculateSHA256(OTA_FILE_PATH); // helper
    if (shaLocal != shaRemote) {
        mqttPublish("OTA: SHA256 mismatch! Aborting.");
        return;
    }
    mqttPublish("OTA: SHA256 verified.");

    // -----------------------
    // 5️⃣ Apply OTA
    // -----------------------
    const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        mqttPublish("OTA: OTA partition not found.");
        return;
    }

    File fwFile = SPIFFS.open(OTA_FILE_PATH, FILE_READ);
    if (!fwFile) {
        mqttPublish("OTA: Failed to open firmware file for OTA.");
        return;
    }

    esp_ota_handle_t ota_handle;
    if (esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle) != ESP_OK) {
        mqttPublish("OTA: esp_ota_begin failed.");
        fwFile.close();
        return;
    }

    uint8_t buf[1024];
    size_t totalWritten = 0;
    int lastPercent = -1;

    while (fwFile.available()) {
        int len = fwFile.read(buf, sizeof(buf));
        if (esp_ota_write(ota_handle, buf, len) != ESP_OK) {
            mqttPublish("OTA: esp_ota_write failed.");
            esp_ota_abort(ota_handle);
            fwFile.close();
            return;
        }

        totalWritten += len;
        int percent = (totalWritten * 100) / fwFile.size();
        if (percent > lastPercent) {
            lastPercent = percent;
            mqttPublish(("OTA: Write progress: " + String(percent) + "%").c_str());
        }
        delay(1);
    }

    fwFile.close();

    if (esp_ota_end(ota_handle) != ESP_OK) {
        mqttPublish("OTA: esp_ota_end failed.");
        return;
    }

    if (esp_ota_set_boot_partition(update_partition) != ESP_OK) {
        mqttPublish("OTA: Failed to set boot partition.");
        return;
    }

    mqttPublish("OTA: Update finished successfully! Restarting...");
    delay(2000);
    ESP.restart();
}

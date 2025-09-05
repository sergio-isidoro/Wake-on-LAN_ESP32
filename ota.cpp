#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <esp_ota_ops.h>
#include "mqtt.h"
#include "config.h"
#include "helpers.h"

// URLs GitHub
const char* versionURL   = "https://raw.githubusercontent.com/sergio-isidoro/Wake-on-LAN_ESP32C3/main/firmware/version.txt";
const char* firmwareURL  = "https://raw.githubusercontent.com/sergio-isidoro/Wake-on-LAN_ESP32C3/main/firmware/WOL_ESP32C3.bin";

#define OTA_BUF_SIZE 1024

// Direct OTA (without SPIFFS): download + flashing with percentage
bool downloadAndFlashFirmware(const char* url) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    if (!http.begin(client, url)) {
        mqttPublish(("OTA: HTTP begin failed for " + String(url)).c_str());
        return false;
    }

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        mqttPublish(("OTA: HTTP GET failed, code " + String(code)).c_str());
        http.end();
        return false;
    }

    int contentLength = http.getSize();
    if (contentLength <= 0) {
        mqttPublish("OTA: Invalid content length");
        http.end();
        return false;
    }

    mqttPublish(("OTA: Firmware size = " + String(contentLength) + " bytes").c_str());

    // Prepare OTA partition
    const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        mqttPublish("OTA: No OTA partition found");
        http.end();
        return false;
    }

    esp_ota_handle_t ota_handle;
    if (esp_ota_begin(update_partition, contentLength, &ota_handle) != ESP_OK) {
        mqttPublish("OTA: esp_ota_begin failed");
        http.end();
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    uint8_t buf[OTA_BUF_SIZE];
    size_t total = 0;
    int lastPercent = -1;

    // Ler stream HTTP e gravar direto na partição OTA
    while (http.connected() && (total < (size_t)contentLength)) {
        size_t avail = stream->available();
        if (avail) {
            int c = stream->readBytes(buf, min((size_t)OTA_BUF_SIZE, avail));
            if (c > 0) {
                if (esp_ota_write(ota_handle, buf, c) != ESP_OK) {
                    mqttPublish("OTA: esp_ota_write failed");
                    esp_ota_abort(ota_handle);
                    http.end();
                    return false;
                }
                total += c;

                int percent = (total * 100) / contentLength;
                if (percent != lastPercent && percent % 10 == 0) {
                    lastPercent = percent;
                    mqttPublish(("OTA: Download/Flashing progress " + String(percent) + "%").c_str());
                }
            }
        } else {
            delay(1);
        }
    }

    http.end();

    if (esp_ota_end(ota_handle) != ESP_OK) {
        mqttPublish("OTA: esp_ota_end failed.");
        return false;
    }

    if (esp_ota_set_boot_partition(update_partition) != ESP_OK) {
        mqttPublish("OTA: Failed to set boot partition");
        return false;
    }

    mqttPublish("OTA: Update successful, restarting...");
    delay(1000);
    ESP.restart();
    return true;
}

// Main OTA function
void performOTA() {
    mqttPublish("OTA: Checking for updates...");

    // 1. Fetch remote version
    String remoteVer;
    {
        WiFiClientSecure clientVer;
        clientVer.setInsecure();
        HTTPClient httpVer;
        httpVer.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

        if (!httpVer.begin(clientVer, versionURL)) {
            mqttPublish("OTA: Failed to connect to version URL");
            return;
        }

        int code = httpVer.GET();
        if (code == HTTP_CODE_OK) {
            remoteVer = httpVer.getString();
            remoteVer.trim();
        } else {
            mqttPublish(("OTA: Failed to get version.txt, code " + String(code)).c_str());
            httpVer.end();
            return;
        }
        httpVer.end();
    }

    if (remoteVer == FIRMWARE_VERSION) {
        mqttPublish("OTA: Already up to date.");
        return;
    }

    mqttPublish(("OTA: New version available: " + remoteVer).c_str());

    // 2. Download + Direct OTA
    if (!downloadAndFlashFirmware(firmwareURL)) {
        mqttPublish("OTA: Firmware update failed");
        return;
    }
}

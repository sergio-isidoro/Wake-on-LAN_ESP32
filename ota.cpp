#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <esp_ota_ops.h>
#include "mqtt.h"
#include "config.h"
#include "helpers.h"

const char* versionURL  = "https://raw.githubusercontent.com/sergio-isidoro/Wake-on-LAN_ESP32C3/main/firmware/version.txt";
const char* firmwareURL = "https://raw.githubusercontent.com/sergio-isidoro/Wake-on-LAN_ESP32C3/main/firmware/WOL_ESP32C3.bin";

void performOTA() {
    mqttPublish("Checking OTA update...");

    // Buscar versão remota
    String remoteVer = "";
    WiFiClientSecure clientVer;
    clientVer.setInsecure();
    HTTPClient httpVer;
    httpVer.begin(clientVer, versionURL);
    int code = httpVer.GET();
    if (code == 200) {
        remoteVer = httpVer.getString();
        remoteVer.trim();
    } else {
        mqttPublish(("Failed to fetch version.txt, HTTP code: " + String(code)).c_str());
        httpVer.end();
        return;
    }
    httpVer.end();

    if (remoteVer == FIRMWARE_VERSION) {
        mqttPublish("No OTA update needed.");
        return;
    }

    mqttPublish(("OTA: new version " + remoteVer).c_str());

    // Iniciar download do firmware
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, firmwareURL);
    int httpCode = http.GET();
    if (httpCode != 200) {
        mqttPublish(("Failed to download firmware, HTTP code: " + String(httpCode)).c_str());
        http.end();
        return;
    }

    int contentLen = http.getSize();
    if (contentLen <= 0) {
        mqttPublish("Invalid content length for OTA");
        http.end();
        return;
    }
    mqttPublish(("OTA size: " + String(contentLen) + " bytes").c_str());

    const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        mqttPublish("Failed to get OTA partition");
        http.end();
        return;
    }

    if (contentLen > update_partition->size) {
        mqttPublish("Firmware too large for OTA partition!");
        http.end();
        return;
    }

    auto* stream = http.getStreamPtr();
    esp_ota_handle_t ota_handle;
    if (esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle) != ESP_OK) {
        mqttPublish("esp_ota_begin failed");
        http.end();
        return;
    }

    uint8_t buf[1024];
    int len;
    size_t totalWritten = 0;
    int lastPercent = 0;

    while ((len = stream->read(buf, sizeof(buf))) > 0) {
        if (esp_ota_write(ota_handle, buf, len) != ESP_OK) {
            mqttPublish("esp_ota_write failed");
            esp_ota_end(ota_handle);
            http.end();
            return;
        }
        totalWritten += len;

        int percent = (int)((totalWritten * 100) / contentLen);
        if (percent / 10 > lastPercent / 10) {
            lastPercent = percent;
            mqttPublish(("OTA progress: " + String((percent / 10) * 10) + "%").c_str());
        }
    }

    esp_err_t err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        mqttPublish(("esp_ota_end failed: " + String(err)).c_str());
        http.end();
        return;
    }

    if (esp_ota_set_boot_partition(update_partition) != ESP_OK) {
        mqttPublish("Failed to set boot partition");
        http.end();
        return;
    }

    mqttPublish("OTA update finished, restarting...");
    http.end();
    delay(1000);
    ESP.restart();
}

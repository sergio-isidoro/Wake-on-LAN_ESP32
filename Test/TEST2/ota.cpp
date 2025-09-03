#include "ota.h"
#include "helpers.h"
#include "mqtt.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <esp_ota_ops.h>

const char* versionURL  = "https://raw.githubusercontent.com/sergio-isidoro/Wake-on-LAN_ESP32C3/refs/heads/main/firmware/version.txt";
const char* firmwareURL = "https://github.com/sergio-isidoro/Wake-on-LAN_ESP32C3/raw/refs/heads/main/firmware/WOL_ESP32C3.bin";

bool isUpdateAvailable(String &remoteVersionOut) {
  HTTPClient http; http.begin(versionURL);
  int code = http.GET();
  if (code != 200) { Serial.printf("Failed to fetch version.txt: %d\n", code); http.end(); return false; }

  String ver = http.getString(); ver.trim();
  remoteVersionOut = ver; http.end();
  return (ver != FIRMWARE_VERSION);
}

void performOTA() {
  String newVer; if (!isUpdateAvailable(newVer)) return;
  mqttPublish(("OTA: new version " + newVer).c_str());
  Serial.println("Starting OTA update...");

  WiFiClientSecure client; client.setInsecure();
  HTTPClient http; http.begin(client, firmwareURL);
  int httpCode = http.GET();
  if (httpCode != 200) { mqttPublish(("Failed to download firmware, HTTP code: " + String(httpCode)).c_str()); http.end(); return; }

  int contentLen = http.getSize(); WiFiClient* stream = http.getStreamPtr();
  const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);
  if (!update_partition) { mqttPublish("Failed to get OTA partition"); http.end(); return; }

  esp_ota_handle_t ota_handle;
  if (esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle) != ESP_OK) { mqttPublish("esp_ota_begin failed"); http.end(); return; }

  uint8_t buf[1024]; int len; int lastPercent=0;
  while ((len = stream->read(buf, sizeof(buf)))>0) {
    if (esp_ota_write(ota_handle, buf, len) != ESP_OK) { mqttPublish("esp_ota_write failed"); http.end(); return; }
    if (contentLen>0) {
      int percent = (int)((esp_ota_get_progress() / (float)contentLen) * 100);
      if (percent/10 > lastPercent/10) { lastPercent=percent; mqttPublish(("OTA progress: " + String((percent/10)*10) + "%").c_str()); }
    }
  }

  if (esp_ota_end(ota_handle) != ESP_OK) { mqttPublish("esp_ota_end failed"); http.end(); return; }
  if (esp_ota_set_boot_partition(update_partition) != ESP_OK) { mqttPublish("Failed to set boot partition"); http.end(); return; }

  mqttPublish("OTA update finished, restarting...");
  http.end(); delay(1000); ESP.restart();
}

/*
 * ESP32-C3 WOL + MQTT + OTA + Ping + OTA Progress
 *
 * Features:
 * - Physical button on D0 (>1s) sends Wake-on-LAN.
 * - LED on D1.
 * - Factory reset on D2 (LOW at boot deletes config.json).
 * - MQTT:
 *    "TurnOn" → sends WOL
 *    "PingPC" → performs ping immediately
 * - After WOL, performs ping automatically 1 minute later (non-blocking).
 * - OTA check every 5 minutes. OTA URLs fixed in code.
 * - OTA progress reporting via MQTT every 10%.
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ESP32Ping.h>
#include <WiFiUdp.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>

// =======================
// DEFINITIONS
// =======================
#define FIRMWARE_VERSION "5.0"
#define RESET_BUTTON_PIN D2   // Factory reset
#define BUTTON_GPIO      D0   // WOL button
#define LED_GPIO         D1   // LED
#define OTA_CHECK_INTERVAL_MS 60000UL // 1 minutes
#define PING_DELAY_AFTER_WOL  60000UL  // 1 minute

// Fixed OTA URLs
const char* versionURL  = "https://raw.githubusercontent.com/youruser/yourrepo/main/version.txt";
const char* firmwareURL = "https://raw.githubusercontent.com/youruser/yourrepo/main/firmware.bin";

// =======================
// CONFIG STRUCT
// =======================
struct Config {
  char ssid[32];
  char password[64];
  char mqtt_server[64];
  int  mqtt_port;
  char mqtt_user[32];
  char mqtt_password[64];
  char target_ip[16];
  char broadcastIPStr[16];
  uint8_t mac_address[6];
  int  udp_port;
};

// =======================
// GLOBALS
// =======================
Config config;
WiFiClientSecure espClient;
PubSubClient mqtt(espClient);
WiFiUDP udp;

unsigned long lastOTACheck = 0;
unsigned long wolSentAt = 0;
bool wolPendingPing = false;
bool buttonTriggered = false;

// =======================
// HELPERS
// =======================
void mqttPublish(const char* msg) {
  Serial.println(msg);
  if (mqtt.connected()) mqtt.publish("wol/log", msg);
}

void blinkDigit(int n) {
  for (int i = 0; i < n; i++) {
    digitalWrite(LED_GPIO, HIGH);
    delay(100);
    digitalWrite(LED_GPIO, LOW);
    delay(100);
  }
}

void blinkVersion(const char* version) {
  int len = strlen(version);
  for (int i = 0; i < len; i++) {
    if (version[i] >= '0' && version[i] <= '9') {
      blinkDigit(version[i] - '0');
      delay(500);
    }
  }
}

// =======================
// CONFIG MANAGEMENT
// =======================
bool saveConfig(const Config &cfg) {
  if (!SPIFFS.begin(true)) return false;
  File f = SPIFFS.open("/config.json", "w");
  if (!f) return false;

  DynamicJsonDocument doc(1024);
  doc["ssid"] = cfg.ssid;
  doc["password"] = cfg.password;
  doc["mqtt_server"] = cfg.mqtt_server;
  doc["mqtt_port"] = cfg.mqtt_port;
  doc["mqtt_user"] = cfg.mqtt_user;
  doc["mqtt_password"] = cfg.mqtt_password;
  doc["target_ip"] = cfg.target_ip;
  doc["broadcastIP"] = cfg.broadcastIPStr;
  JsonArray mac = doc.createNestedArray("mac_address");
  for (int i=0; i<6; i++) mac.add(cfg.mac_address[i]);
  doc["udp_port"] = cfg.udp_port;

  serializeJsonPretty(doc, f);
  f.close();
  return true;
}

bool loadConfig() {
  if (!SPIFFS.begin(true)) return false;
  File f = SPIFFS.open("/config.json", "r");
  if (!f) return false;

  DynamicJsonDocument doc(1024);
  if (deserializeJson(doc, f)) return false;

  strlcpy(config.ssid, doc["ssid"], sizeof(config.ssid));
  strlcpy(config.password, doc["password"], sizeof(config.password));
  strlcpy(config.mqtt_server, doc["mqtt_server"], sizeof(config.mqtt_server));
  config.mqtt_port = doc["mqtt_port"] | 1883;
  strlcpy(config.mqtt_user, doc["mqtt_user"], sizeof(config.mqtt_user));
  strlcpy(config.mqtt_password, doc["mqtt_password"], sizeof(config.mqtt_password));
  strlcpy(config.target_ip, doc["target_ip"], sizeof(config.target_ip));
  strlcpy(config.broadcastIPStr, doc["broadcastIP"], sizeof(config.broadcastIPStr));
  for (int i=0; i<6; i++) config.mac_address[i] = doc["mac_address"][i];
  config.udp_port = doc["udp_port"] | 9;

  f.close();
  Serial.println("Configuration loaded successfully!");
  return true;
}

void factoryReset() {
  Serial.println("\n--- FACTORY RESET ---");
  if (!SPIFFS.begin(true)) return;
  if (SPIFFS.exists("/config.json")) {
    SPIFFS.remove("/config.json");
    Serial.println("config.json deleted.");
  }
  delay(2000);
  ESP.restart();
}

void runSerialSetup() {
  Serial.println("\n--- INTERACTIVE SETUP ---");

  Config newCfg;
  auto readLine = []() -> String {
    while (Serial.available() == 0) delay(50);
    String s = Serial.readStringUntil('\n');
    s.trim();
    return s;
  };

  Serial.print("SSID: "); strlcpy(newCfg.ssid, readLine().c_str(), sizeof(newCfg.ssid));
  Serial.print("WiFi Password: "); strlcpy(newCfg.password, readLine().c_str(), sizeof(newCfg.password));
  Serial.print("MQTT Server: "); strlcpy(newCfg.mqtt_server, readLine().c_str(), sizeof(newCfg.mqtt_server));
  Serial.print("MQTT Port: "); newCfg.mqtt_port = readLine().toInt();
  Serial.print("MQTT User: "); strlcpy(newCfg.mqtt_user, readLine().c_str(), sizeof(newCfg.mqtt_user));
  Serial.print("MQTT Password: "); strlcpy(newCfg.mqtt_password, readLine().c_str(), sizeof(newCfg.mqtt_password));
  Serial.print("Target IP: "); strlcpy(newCfg.target_ip, readLine().c_str(), sizeof(newCfg.target_ip));
  Serial.print("Broadcast IP: "); strlcpy(newCfg.broadcastIPStr, readLine().c_str(), sizeof(newCfg.broadcastIPStr));
  Serial.print("MAC (xx:xx:xx:xx:xx:xx): "); 
  String macStr = readLine();
  sscanf(macStr.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
         &newCfg.mac_address[0], &newCfg.mac_address[1], &newCfg.mac_address[2],
         &newCfg.mac_address[3], &newCfg.mac_address[4], &newCfg.mac_address[5]);
  newCfg.udp_port = 9;

  if (saveConfig(newCfg)) {
    Serial.println("Config saved! Restarting...");
    delay(2000);
    ESP.restart();
  } else {
    Serial.println("Error saving config!");
  }
}

// =======================
// WIFI + MQTT
// =======================
void setupWiFi() {
  WiFi.begin(config.ssid, config.password);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
}

void ensureMqtt() {
  while (!mqtt.connected()) {
    Serial.print("Connecting MQTT...");
    if (mqtt.connect("ESP32C3-WOL", config.mqtt_user, config.mqtt_password)) {
      Serial.println("Connected!");
      mqtt.publish("wol/status", "Ready", true);
      mqtt.subscribe("wol/event");
    } else delay(2000);
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  String msg; for (unsigned int i=0;i<len;i++) msg += (char)payload[i];
  msg.trim();
  if (msg == "TurnOn") sendWOL("MQTT");
  else if (msg == "PingPC") doPing();
}

// =======================
// WOL + PING
// =======================
void sendWOL(const char* reason) {
  uint8_t packet[102]; memset(packet, 0xFF, 6);
  for (int i=0;i<16;i++) memcpy(&packet[6+i*6], config.mac_address, 6);

  IPAddress bcast; bcast.fromString(config.broadcastIPStr);
  udp.beginPacket(bcast, config.udp_port);
  udp.write(packet, sizeof(packet));
  udp.endPacket();

  mqttPublish(("WOL sent (" + String(reason) + ")").c_str());
  wolSentAt = millis(); wolPendingPing = true;
}

void doPing() {
  IPAddress target; target.fromString(config.target_ip);
bool ok = Ping.ping(target, 3);
  if (ok) mqttPublish("Ping OK: PC online");
  else    mqttPublish("Ping FAIL: PC offline");
}

// =======================
// OTA
// =======================
bool isUpdateAvailable(String &remoteVersionOut) {
  HTTPClient http;
  http.begin(versionURL);
  int code = http.GET();
  if (code != 200) {
    Serial.printf("Failed to fetch version.txt: %d\n", code);
    http.end();
    return false;
  }

  String ver = http.getString();
  ver.trim();
  remoteVersionOut = ver;
  http.end();
  return (ver != FIRMWARE_VERSION);
}

void performOTA() {
  String newVer;
  if (!isUpdateAvailable(newVer)) return;

  mqttPublish(("OTA: new version " + newVer).c_str());
  Serial.println("Starting OTA update...");

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.begin(client, firmwareURL);
  int httpCode = http.GET();
  if (httpCode != 200) {
    mqttPublish(("Failed to download firmware, HTTP code: " + String(httpCode)).c_str());
    http.end(); return;
  }

  int contentLen = http.getSize();
  WiFiClient* stream = http.getStreamPtr();
  const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);
  if (!update_partition) { mqttPublish("Failed to get OTA partition"); http.end(); return; }

  esp_ota_handle_t ota_handle;
  if (esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle) != ESP_OK) {
    mqttPublish("esp_ota_begin failed"); http.end(); return;
  }

  uint8_t buf[1024]; int len; int lastPercent = 0;
  while ((len = stream->read(buf, sizeof(buf))) > 0) {
    if (esp_ota_write(ota_handle, buf, len) != ESP_OK) {
      mqttPublish("esp_ota_write failed"); http.end(); return;
    }
    if (contentLen > 0) { // report progress
      int percent = (int)((esp_ota_get_progress() / (float)contentLen) * 100);
      if (percent / 10 > lastPercent / 10) {
        lastPercent = percent;
        mqttPublish(("OTA progress: " + String((percent / 10) * 10) + "%").c_str());
      }
    }
  }

  if (esp_ota_end(ota_handle) != ESP_OK) {
    mqttPublish("esp_ota_end failed"); http.end(); return;
  }

  if (esp_ota_set_boot_partition(update_partition) != ESP_OK) {
    mqttPublish("Failed to set boot partition"); http.end(); return;
  }

  mqttPublish("OTA update finished, restarting...");
  http.end();
  delay(1000);
  ESP.restart();
}

// =======================
// SETUP
// =======================
void setup() {
  Serial.begin(115200);

  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUTTON_GPIO, INPUT_PULLUP);
  pinMode(LED_GPIO, OUTPUT);
  digitalWrite(LED_GPIO, LOW);

  if (digitalRead(RESET_BUTTON_PIN) == LOW) factoryReset();

  if (!loadConfig()) runSerialSetup();

  setupWiFi();
  espClient.setInsecure();
  mqtt.setServer(config.mqtt_server, config.mqtt_port);
  mqtt.setCallback(mqttCallback);
  udp.begin(config.udp_port);

  blinkVersion(FIRMWARE_VERSION);
  lastOTACheck = millis();
}

// =======================
// LOOP
// =======================
void loop() {
  if (!mqtt.connected()) ensureMqtt();
  mqtt.loop();

  // WOL button (>1s)
  static unsigned long t0 = 0;
  if (digitalRead(BUTTON_GPIO) == LOW) {
    if (!buttonTriggered) { buttonTriggered = true; t0 = millis(); }
    else if (millis()-t0 > 1000) { sendWOL("Button"); buttonTriggered = false; }
  } else buttonTriggered = false;

  // OTA check
  if (millis()-lastOTACheck > OTA_CHECK_INTERVAL_MS) {
    lastOTACheck = millis();
    performOTA();
  }

  // Scheduled ping after WOL
  if (wolPendingPing && millis()-wolSentAt >= PING_DELAY_AFTER_WOL) {
    doPing();
    wolPendingPing = false;
  }

  delay(10);
}

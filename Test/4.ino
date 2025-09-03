/*
 * ESP32-C3 WOL + MQTT + OTA + PING
 * 
 * Features:
 * - Physical button on D0 (>1s) sends Wake-on-LAN.
 * - LED on D1.
 * - Factory reset on D2 (LOW at boot deletes config.json).
 * - MQTT:
 *    "TurnOn" → sends WOL
 *    "PingPC" → performs ping immediately
 * - After WOL, performs ping automatically 1 minute later (non-blocking).
 * - OTA check every 5 minutes.
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

// =======================
// DEFINITIONS
// =======================
#define FIRMWARE_VERSION "7.0-final"
#define RESET_BUTTON_PIN 2   // GPIO2 → Factory reset
#define BUTTON_GPIO      0   // GPIO0 → WOL button
#define LED_GPIO         1   // GPIO1 → LED
#define OTA_CHECK_INTERVAL_MS 300000UL // 5 minutes
#define PING_DELAY_AFTER_WOL  60000UL  // 1 minute

// Config structure
struct Config {
  char ssid[32];
  char password[64];
  char mqtt_server[64];
  int  mqtt_port;
  char mqtt_user[32];
  char mqtt_password[64];
  char versionURL[128];
  char firmwareURL[128];
  char target_ip[16];
  char broadcastIPStr[16];
  uint8_t mac_address[6];
  int  udp_port;
};

// Globals
Config config;
WiFiClientSecure espClient;
PubSubClient mqtt(espClient);
WiFiUDP udp;

unsigned long lastOTACheck = 0;
unsigned long wolSentAt = 0;  
bool wolPendingPing = false;
bool buttonTriggered = false;

// =======================
// Helpers
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
// Config management (SPIFFS)
// =======================
bool saveConfig(const Config &cfg) {
  if (!SPIFFS.begin(true)) return false;

  File f = SPIFFS.open("/config.json", "w");
  if (!f) return false;

  DynamicJsonDocument doc(1536);
  doc["ssid"] = cfg.ssid;
  doc["password"] = cfg.password;
  doc["mqtt_server"] = cfg.mqtt_server;
  doc["mqtt_port"] = cfg.mqtt_port;
  doc["mqtt_user"] = cfg.mqtt_user;
  doc["mqtt_password"] = cfg.mqtt_password;
  doc["versionURL"] = cfg.versionURL;
  doc["firmwareURL"] = cfg.firmwareURL;
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

  DynamicJsonDocument doc(1536);
  if (deserializeJson(doc, f)) return false;

  strlcpy(config.ssid, doc["ssid"], sizeof(config.ssid));
  strlcpy(config.password, doc["password"], sizeof(config.password));
  strlcpy(config.mqtt_server, doc["mqtt_server"], sizeof(config.mqtt_server));
  config.mqtt_port = doc["mqtt_port"] | 1883;
  strlcpy(config.mqtt_user, doc["mqtt_user"], sizeof(config.mqtt_user));
  strlcpy(config.mqtt_password, doc["mqtt_password"], sizeof(config.mqtt_password));
  strlcpy(config.versionURL, doc["versionURL"], sizeof(config.versionURL));
  strlcpy(config.firmwareURL, doc["firmwareURL"], sizeof(config.firmwareURL));
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

  Serial.print("SSID: ");
  strlcpy(newCfg.ssid, readLine().c_str(), sizeof(newCfg.ssid));

  Serial.print("WiFi Password: ");
  strlcpy(newCfg.password, readLine().c_str(), sizeof(newCfg.password));

  Serial.print("MQTT Server: ");
  strlcpy(newCfg.mqtt_server, readLine().c_str(), sizeof(newCfg.mqtt_server));

  Serial.print("MQTT Port: ");
  newCfg.mqtt_port = readLine().toInt();

  Serial.print("MQTT User: ");
  strlcpy(newCfg.mqtt_user, readLine().c_str(), sizeof(newCfg.mqtt_user));

  Serial.print("MQTT Password: ");
  strlcpy(newCfg.mqtt_password, readLine().c_str(), sizeof(newCfg.mqtt_password));

  Serial.print("URL version.txt: ");
  strlcpy(newCfg.versionURL, readLine().c_str(), sizeof(newCfg.versionURL));

  Serial.print("URL firmware.bin: ");
  strlcpy(newCfg.firmwareURL, readLine().c_str(), sizeof(newCfg.firmwareURL));

  Serial.print("Target IP (e.g. 192.168.1.100): ");
  strlcpy(newCfg.target_ip, readLine().c_str(), sizeof(newCfg.target_ip));

  Serial.print("Broadcast IP (e.g. 192.168.1.255): ");
  strlcpy(newCfg.broadcastIPStr, readLine().c_str(), sizeof(newCfg.broadcastIPStr));

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
// WiFi + MQTT
// =======================
void setupWiFi() {
  WiFi.begin(config.ssid, config.password);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
}

void ensureMqtt() {
  while (!mqtt.connected()) {
    Serial.print("Connecting MQTT...");
    if (mqtt.connect("ESP32C3-WOL", config.mqtt_user, config.mqtt_password)) {
      Serial.println("Connected!");
      mqtt.publish("wol/status", "Ready", true);
      mqtt.subscribe("wol/event");
    } else {
      delay(2000);
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  String msg;
  for (unsigned int i=0;i<len;i++) msg += (char)payload[i];
  msg.trim();

  if (msg == "TurnOn") {
    sendWOL("MQTT");
  } else if (msg == "PingPC") {
    doPing();
  }
}

// =======================
// WOL + PING
// =======================
void sendWOL(const char* reason) {
  uint8_t packet[102];
  memset(packet, 0xFF, 6);
  for (int i=0;i<16;i++) memcpy(&packet[6+i*6], config.mac_address, 6);

  IPAddress bcast; bcast.fromString(config.broadcastIPStr);
  udp.beginPacket(bcast, config.udp_port);
  udp.write(packet, sizeof(packet));
  udp.endPacket();

  mqttPublish(("WOL sent (" + String(reason) + ")").c_str());

  // schedule ping after 1 min
  wolSentAt = millis();
  wolPendingPing = true;
}

void doPing() {
  IPAddress target;
  target.fromString(config.target_ip);
  bool ok = Ping.ping(target, 3);
  if (ok) mqttPublish("Ping OK: PC online");
  else    mqttPublish("Ping FAIL: PC offline");
}

// =======================
// OTA
// =======================
bool isUpdateAvailable(String &remoteVersionOut) {
  HTTPClient http;
  http.begin(config.versionURL);
  int code = http.GET();
  if (code != 200) return false;

  String ver = http.getString();
  ver.trim();
  remoteVersionOut = ver;
  return (ver != FIRMWARE_VERSION);
}

void performOTA() {
  String newVer;
  if (!isUpdateAvailable(newVer)) return;

  mqttPublish(("OTA: new version " + newVer).c_str());

  WiFiClientSecure client;
  client.setInsecure();
  HTTPUpdate updater;
  t_httpUpdate_return ret = updater.update(client, config.firmwareURL);

  if (ret == HTTP_UPDATE_OK) {
    ESP.restart();
  }
}

// =======================
// SETUP
// =======================
void setup() {
  Serial.begin(115200);
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUTTON_GPIO, INPUT_PULLUP);
  pinMode(LED_GPIO, OUTPUT);

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
    else if (millis()-t0 > 1000) {
      sendWOL("Button");
      buttonTriggered = false;
    }
  } else buttonTriggered = false;

  // OTA
  if (millis()-lastOTACheck > OTA_CHECK_INTERVAL_MS) {
    lastOTACheck = millis();
    performOTA();
  }

  // Scheduled ping after WOL
  if (wolPendingPing && millis()-wolSentAt >= PING_DELAY_AFTER_WOL) {
    doPing();
    wolPendingPing = false;
  }
}

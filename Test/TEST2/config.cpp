#include "config.h"
#include <Arduino.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <esp32-hal.h>

Config config;
unsigned long lastOTACheck = 0;
unsigned long wolSentAt = 0;
bool wolPendingPing = false;
bool buttonTriggered = false;

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
  for (int i=0;i<6;i++) mac.add(cfg.mac_address[i]);
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
  for (int i=0;i<6;i++) config.mac_address[i] = doc["mac_address"][i];
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
    String s = Serial.readStringUntil('\n'); s.trim(); return s;
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
  } else Serial.println("Error saving config!");
}

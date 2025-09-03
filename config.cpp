#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "config.h"
#include "mqtt.h"
#include "helpers.h"

// Definição das variáveis globais declaradas em config.h
Config config;
unsigned long lastOTACheck = 0;
unsigned long wolSentAt = 0;
bool wolPendingPing = false;
bool buttonTriggered = false;

bool saveConfig(const Config &cfg) {
    if (!SPIFFS.begin(true)) return false;
    File f = SPIFFS.open("/config.json", "w");
    if (!f) return false;

    JsonDocument doc;
    doc["ssid"] = cfg.ssid;
    doc["password"] = cfg.password;
    doc["mqtt_server"] = cfg.mqtt_server;
    doc["mqtt_port"] = cfg.mqtt_port;
    doc["mqtt_user"] = cfg.mqtt_user;
    doc["mqtt_password"] = cfg.mqtt_password;
    doc["target_ip"] = cfg.target_ip;
    doc["broadcastIP"] = cfg.broadcastIPStr;

    JsonArray mac = doc["mac_address"].to<JsonArray>();
    for (int i = 0; i < 6; i++) mac.add(cfg.mac_address[i]);

    doc["udp_port"] = cfg.udp_port;

    serializeJsonPretty(doc, f);
    f.close();
    return true;
}

bool loadConfig() {
    if (!SPIFFS.begin(true)) return false;
    File f = SPIFFS.open("/config.json", "r");
    if (!f) return false;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return false;

    strlcpy(config.ssid, doc["ssid"] | "", sizeof(config.ssid));
    strlcpy(config.password, doc["password"] | "", sizeof(config.password));
    strlcpy(config.mqtt_server, doc["mqtt_server"] | "", sizeof(config.mqtt_server));
    config.mqtt_port = doc["mqtt_port"] | 1883;
    strlcpy(config.mqtt_user, doc["mqtt_user"] | "", sizeof(config.mqtt_user));
    strlcpy(config.mqtt_password, doc["mqtt_password"] | "", sizeof(config.mqtt_password));
    strlcpy(config.target_ip, doc["target_ip"] | "", sizeof(config.target_ip));
    strlcpy(config.broadcastIPStr, doc["broadcastIP"] | "", sizeof(config.broadcastIPStr));

    JsonArray mac = doc["mac_address"].as<JsonArray>();
    for (int i = 0; i < 6; i++) config.mac_address[i] = mac[i] | 0;

    config.udp_port = doc["udp_port"] | 9;

    mqttPublish("Configuration loaded successfully!");
    return true;
}

void factoryReset() {
    mqttPublish("--- FACTORY RESET ---");
    if (!SPIFFS.begin(true)) return;
    if (SPIFFS.exists("/config.json")) SPIFFS.remove("/config.json");
    delay(2000);
    ESP.restart();
}

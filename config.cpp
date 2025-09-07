/*
 * config.cpp
 * -------------------------------
 * Implements configuration management:
 *  - Saves and loads configuration to/from SPIFFS as JSON
 *  - Provides factoryReset() to delete config and restart ESP32
 *  - Publishes success messages via MQTT
 */


#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "config.h"
#include "mqtt.h"
#include "helpers.h"

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
    doc["ssid"]          = cfg.ssid;
    doc["password"]      = cfg.password;
    doc["mqtt_server"]   = cfg.mqtt_server;
    doc["mqtt_port"]     = cfg.mqtt_port;
    doc["mqtt_user"]     = cfg.mqtt_user;
    doc["mqtt_password"] = cfg.mqtt_password;
    doc["target_ip"]     = cfg.target_ip;
    doc["broadcastIP"]   = cfg.broadcastIPStr;
    doc["udp_port"]      = cfg.udp_port;

    char macStr[18];
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
            cfg.mac_address[0], cfg.mac_address[1], cfg.mac_address[2],
            cfg.mac_address[3], cfg.mac_address[4], cfg.mac_address[5]);
    
    doc["mac_address"] = macStr;

    // Save the json
    serializeJsonPretty(doc, f);
    f.close();

    Serial.println();
    File f2 = SPIFFS.open("/config.json", "r");
    if (f2) {
        Serial.println("Config JSON (saved in /config.json):");
        while (f2.available()) {
            Serial.write(f2.read());
        }
        f2.close();
        Serial.println();
    } else {
        Serial.println("Error reopening /config.json");
    }
    Serial.println();

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
    config.mqtt_port = doc["mqtt_port"] | 8883;
    strlcpy(config.mqtt_user, doc["mqtt_user"] | "", sizeof(config.mqtt_user));
    strlcpy(config.mqtt_password, doc["mqtt_password"] | "", sizeof(config.mqtt_password));
    strlcpy(config.target_ip, doc["target_ip"] | "", sizeof(config.target_ip));
    strlcpy(config.broadcastIPStr, doc["broadcastIP"] | "", sizeof(config.broadcastIPStr));
    config.udp_port = doc["udp_port"] | 9;
    
    const char* macStr = doc["mac_address"];
    for (int i = 0; i < 6; i++) {
      char byteStr[3] = { macStr[i*3], macStr[i*3 + 1], '\0' };
      config.mac_address[i] = (uint8_t) strtoul(byteStr, nullptr, 16);
    }

    Serial.println();
    File f2 = SPIFFS.open("/config.json", "r");
    if (f2) {
        Serial.println("Config JSON (load from /config.json):");
        while (f2.available()) {
            Serial.write(f2.read());
        }
        f2.close();
        Serial.println();
    } else {
        Serial.println("Error reopening /config.json");
    }

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

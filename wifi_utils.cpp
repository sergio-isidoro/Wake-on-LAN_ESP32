/*
 * wifi_utils.cpp
 * -------------------------------
 * Implements WiFi setup:
 *  - Connects to WiFi using SSID/password from config
 *  - Publishes status via MQTT
 *  - If connection fails, starts ESP32 in AP mode for config portal
 */
 
#include "wifi_utils.h"
#include <WiFi.h>
#include "helpers.h"

void setupWiFi(){
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.ssid, config.password);

  mqttPublish("Connecting WiFi...");

  int retries=0;
  while(WiFi.status() != WL_CONNECTED && retries<30){
    delay(500);
    Serial.print(".");
    retries++;
  }

  if(WiFi.status() == WL_CONNECTED){
    mqttPublish(("WiFi OK: " + WiFi.localIP().toString()).c_str());

  } else {
    mqttPublish("WiFi Failed, starting AP...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("WOL_ESP32_Config");
  }
}
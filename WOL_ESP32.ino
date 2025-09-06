/*
 * main.ino
 * -------------------------------
 * Main program for ESP32 Wake-on-LAN (WOL) + MQTT + OTA + Portal.
 * 
 * Features:
 *  - Sends WOL magic packets
 *  - Performs scheduled ping checks
 *  - Connects to WiFi and MQTT broker
 *  - Supports OTA firmware updates from GitHub
 *  - Configuration via web portal (SPIFFS)
 *
 * Compatible devices:
 *  - ESP32-C3 (e.g., Seeed Studio XIAO ESP32-C3, DevKitM-1)
 *  - ESP32-S3 (DevKit, XIAO S3, AiThinker modules)
 *  - ESP32 original (ESP32-WROOM-32, ESP32-WROVER)
 *  - ESP32-S2 (DevKit, XIAO ESP32-S2)
 *  - ESP32-PICO-D4 (with 4 MB flash)
 *
 * Notes:
 *  - Make sure the partition scheme has at least 2 OTA slots
 *    (Default 4Mb or >= 1.3Mb OTA_Partition)
 *  - GPIO pins for LED, button, and reset may need adjustment
 *    depending on your board
 *  - OTA updates preserve /config.json (portal configuration)
 */

#include <Arduino.h>
#include "wifi_utils.h"
#include "config.h"
#include "helpers.h"
#include "mqtt.h"
#include "wol_ping.h"
#include "ota.h"
#include "configPortal.h"

bool FirstBoot = true;

void setup(){
  Serial.begin(115200);
  
  pinMode(RESET_OTA_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUTTON_GPIO, INPUT_PULLUP);
  pinMode(PIN1_GPIO, OUTPUT); digitalWrite(PIN1_GPIO, LOW);
  pinMode(PIN2_GPIO, OUTPUT); digitalWrite(PIN2_GPIO, LOW);
  pinMode(LED_GPIO, OUTPUT); digitalWrite(LED_GPIO, HIGH);

  if(digitalRead(RESET_OTA_BUTTON_PIN) == LOW) factoryReset();

  if(!loadConfig()){
    startConfigPortal();
    while(true){ 
      server.handleClient(); 
      delay(10);
    }
  }
  digitalWrite(LED_GPIO, LOW);

  setupWiFi();
  setupMQTT();
  udp.begin(config.udp_port);

  blinkVersion(FIRMWARE_VERSION);
  lastOTACheck = millis();
}

void loop(){
  if(!mqttConnected()) ensureMqtt();
  mqttLoop();

  if(FirstBoot){
    mqttPublish(("----> WOL ESP32 v" + String(FIRMWARE_VERSION)).c_str());
    mqtt.publish("wol/event", "", true);
    FirstBoot = false;
  }

  handleButton();
  handleScheduledPing();

  if(millis() - lastOTACheck > OTA_CHECK_INTERVAL_MS || digitalRead(RESET_OTA_BUTTON_PIN) == LOW || chkUpdate){
    lastOTACheck=millis();
    performOTA();
    chkUpdate = false;
  }

  server.handleClient();
  delay(1);
}


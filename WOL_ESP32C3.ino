/*
 * ESP32-C3 WOL + MQTT + JSON + SETUP + HOTSTOP + OTA (with GitHub)
 * ESP32C3 Dev Module (Seeed Studio)
 * ---------------------------------------
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
  pinMode(RESET_OTA_BUTTON_PIN,INPUT_PULLUP);
  pinMode(BUTTON_GPIO,INPUT_PULLUP);
  pinMode(LED_GPIO,OUTPUT); digitalWrite(LED_GPIO,HIGH);

  if(digitalRead(RESET_OTA_BUTTON_PIN)==LOW) factoryReset();

  if(!loadConfig()){
    startConfigPortal();
    while(true){ 
      server.handleClient(); delay(10);
    }
  }
  digitalWrite(LED_GPIO,LOW);

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
    mqttPublish(("----> WOL ESP32C3 v" + String(FIRMWARE_VERSION)).c_str());
    mqtt.publish("wol/event", "", true);
    FirstBoot = false;
  }

  handleButton();
  handleScheduledPing();

  if(millis()-lastOTACheck>OTA_CHECK_INTERVAL_MS || digitalRead(RESET_OTA_BUTTON_PIN) == LOW){
    lastOTACheck=millis();
    performOTA();
  }

  server.handleClient();
  delay(1);
}

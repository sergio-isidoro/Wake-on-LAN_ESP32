#include <Arduino.h>
#include "config.h"
#include "helpers.h"
#include "mqtt.h"
#include "wol_ping.h"
#include "ota.h"

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
  setupMQTT();
  udp.begin(config.udp_port);

  blinkVersion(FIRMWARE_VERSION);
  lastOTACheck = millis();
}

// =======================
// LOOP
// =======================
void loop() {
  if (!mqttConnected()) ensureMqtt();
  mqttLoop();

  handleButton();

  // OTA check
  if (millis()-lastOTACheck > OTA_CHECK_INTERVAL_MS) {
    lastOTACheck = millis();
    performOTA();
  }

  // Scheduled ping after WOL
  handleScheduledPing();

  delay(10);
}

#pragma once
#include <Arduino.h>

#define FIRMWARE_VERSION      "5.3"
#define RESET_OTA_BUTTON_PIN  D2
#define BUTTON_GPIO           D0
#define LED_GPIO              D1
#define PIN1_GPIO             D4
#define PIN2_GPIO             D5
#define OTA_CHECK_INTERVAL_MS 3600000UL   // 1h
#define PING_DELAY_AFTER_WOL  120000UL    // 2min

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

extern Config config;
extern unsigned long lastOTACheck;
extern unsigned long wolSentAt;
extern bool wolPendingPing;
extern bool buttonTriggered;

bool saveConfig(const Config &cfg);
bool loadConfig();
void factoryReset();

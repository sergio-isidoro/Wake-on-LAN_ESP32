/*
 * helpers.h
 * -------------------------------
 * Declares helper functions used across the project:
 *  - mqttPublish(): send log messages via MQTT
 *  - blinkDigit(): blink LED a number of times
 *  - blinkVersion(): blink LED to display firmware version
 */

#pragma once
#include "config.h"
#include <WiFiUdp.h>

extern WiFiUDP udp;

void mqttPublish(const char* msg);
void blinkDigit(int n);
void blinkVersion(const char* version);

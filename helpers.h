#pragma once
#include "config.h"
#include <WiFiUdp.h>

extern WiFiUDP udp;

void mqttPublish(const char* msg);
void blinkDigit(int n);
void blinkVersion(const char* version);

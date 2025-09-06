/*
 * configPortal.h
 * -------------------------------
 * Declares functions for the WiFi/MQTT configuration portal:
 *  - startConfigPortal() starts a web server for user configuration
 *  - handleRoot() serves the setup HTML page
 *  - handleSave() saves posted configuration and restarts ESP32
 */

#pragma once
#include <WebServer.h>
#include "config.h"

extern WebServer server;

void startConfigPortal();
void handleRoot();
void handleSave();
#pragma once
#include <WebServer.h>
#include "config.h"

extern WebServer server;

void startConfigPortal();
void handleRoot();
void handleSave();
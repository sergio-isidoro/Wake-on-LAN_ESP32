#pragma once
#include "config.h"

void sendWOL(const char* reason);
void doPing();
void handleButton();
void handleScheduledPing();
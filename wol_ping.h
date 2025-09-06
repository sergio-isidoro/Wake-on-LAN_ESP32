#pragma once
#include "config.h"

void sendWOL(const char* reason, int n);
void doPing();
void handleButton();
void handleScheduledPing();
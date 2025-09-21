/*
 * wol_ping.h
 * -------------------------------
 * Declares functions for Wake-on-LAN and ping monitoring:
 *  - sendWOL() sends a magic packet to wake the PC
 *  - doPing() checks if the PC is online
 *  - handleButton() triggers WOL by button press
 *  - handleScheduledPing() does ping after WOL delay
 */

#pragma once
#include "config.h"
#include <EthernetUdp.h>

void sendWOL(const char* reason, int n);
void sendShutdownPacket(const char* reason, int n);
void doPing();
void handleButton();
void handleScheduledPing();
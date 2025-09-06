/*
 * ota.h
 * -------------------------------
 * Declares the OTA update function.
 * This module handles checking GitHub for a new firmware version
 * and flashing it directly to the OTA partition.
 */
 
#pragma once
#include <Arduino.h>

void performOTA();

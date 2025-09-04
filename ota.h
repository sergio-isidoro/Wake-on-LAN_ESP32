#pragma once

#include <Arduino.h>

#define OTA_FILE_PATH   "/firmware.bin"
#define OTA_SHA_PATH    "/firmware.sha256"
#define MAX_OTA_RETRIES 3

void performOTA();
String calculateSHA256(const char* path);

#pragma once
#include "config.h"
#include <HTTPClient.h>
#include <esp_ota_ops.h>

bool isUpdateAvailable(String &remoteVersionOut);
void performOTA();

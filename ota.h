#pragma once
#include "config.h"

bool isUpdateAvailable(String &remoteVersionOut);
void performOTA();
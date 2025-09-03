#include "wifi_utils.h"
#include <WiFi.h>
#include "helpers.h"

void setupWiFi(){
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.ssid, config.password);

  mqttPublish("Connecting WiFi...");
  int retries=0;
  while(WiFi.status()!=WL_CONNECTED && retries<30){
    delay(500);
    Serial.print(".");
    retries++;
  }
  if(WiFi.status()==WL_CONNECTED){
    mqttPublish(("WiFi OK: " + WiFi.localIP().toString()).c_str());
  } else {
    mqttPublish("WiFi Failed, starting AP...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32C3_Config");
  }
}
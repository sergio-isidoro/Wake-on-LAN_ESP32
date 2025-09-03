#include "helpers.h"
#include "mqtt.h"
#include "config.h"

WiFiUDP udp;

void mqttPublish(const char* msg){
  Serial.println(msg);
  if(mqtt.connected()) mqtt.publish("wol/log",msg);
}

void blinkDigit(int n){
  for(int i=0;i<n;i++){
    digitalWrite(LED_GPIO,HIGH); delay(100);
    digitalWrite(LED_GPIO,LOW); delay(100);
  }
}

void blinkVersion(const char* version){
  int len = strlen(version);
  for(int i=0;i<len;i++){
    if(version[i]>='0' && version[i]<='9'){
      blinkDigit(version[i]-'0');
      delay(500);
    }
  }
}
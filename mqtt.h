#pragma once
#include <PubSubClient.h>
#include <WiFiClientSecure.h>

extern WiFiClientSecure espClient;
extern PubSubClient mqtt;

extern bool chkUpdate;

void setupMQTT();
void ensureMqtt();
bool mqttConnected();
void mqttLoop();
void mqttCallback(char* topic, byte* payload, unsigned int len);

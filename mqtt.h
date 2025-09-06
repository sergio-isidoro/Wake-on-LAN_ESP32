/*
 * mqtt.h
 * -------------------------------
 * Declares functions and objects for MQTT communication.
 * Handles connection, reconnection, subscribing, publishing,
 * and processing incoming MQTT messages for WOL events.
 */

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

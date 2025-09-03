#include "mqtt.h"
#include "config.h"
#include "wol_ping.h"


WiFiClientSecure espClient;
PubSubClient mqtt(espClient);

void setupMQTT() {
  espClient.setInsecure();
  mqtt.setServer(config.mqtt_server, config.mqtt_port);
  mqtt.setCallback(mqttCallback);
}

bool mqttConnected() { return mqtt.connected(); }

void ensureMqtt() {
  while(!mqtt.connected()){
    Serial.print("Connecting MQTT...");
    if(mqtt.connect("ESP32C3-WOL", config.mqtt_user, config.mqtt_password)){
      Serial.println("Connected!");
      mqtt.publish("wol/status","Ready",true);
      mqtt.subscribe("wol/event");
    } else delay(2000);
  }
}

void mqttLoop() { mqtt.loop(); }

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  String msg; for(unsigned int i=0;i<len;i++) msg += (char)payload[i]; msg.trim();
  if(msg=="TurnOn") sendWOL("MQTT");
  else if(msg=="PingPC") doPing();
}

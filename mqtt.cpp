#include "mqtt.h"
#include "config.h"
#include "wol_ping.h"
#include "helpers.h"

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
    if(mqtt.connect("ESP32C3-WOL", config.mqtt_user, config.mqtt_password)){
      mqtt.publish("wol/status","MQTT Ready",true);
      mqtt.subscribe("wol/event");
    } else delay(2000);
  }
}

void mqttLoop() { mqtt.loop(); }

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  String msg; for(unsigned int i=0;i<len;i++) msg += (char)payload[i]; msg.trim();
  if(msg == "TurnOn"){
    sendWOL("MQTT");
  } else if(msg == "TurnOff"){
    //... coming soon
  } else if(msg == "PingPC"){
    doPing();
  } else if(msg == "PinOut1On"){
    digitalWrite(PIN1_GPIO,HIGH);
    mqttPublish("PinOut 1 -> ON");
  } else if(msg == "PinOut1Off"){
    digitalWrite(PIN1_GPIO,LOW);
    mqttPublish("PinOut 1 -> OFF");
  } else if(msg == "PinOut2On"){
    digitalWrite(PIN2_GPIO,HIGH);
    mqttPublish("PinOut 2 -> ON");
  } else if(msg == "PinOut2Off"){
    digitalWrite(PIN2_GPIO,LOW);
    mqttPublish("PinOut 2 -> OFF");
  }
  mqtt.publish("wol/event", "", true);
}
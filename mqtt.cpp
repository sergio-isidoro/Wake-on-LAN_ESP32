/*
 * mqtt.cpp
 * -------------------------------
 * Implements MQTT communication:
 *  - Connects to the MQTT server using credentials from config
 *  - Publishes status and log messages
 *  - Subscribes to WOL commands
 *  - Processes incoming messages to trigger WOL or ping
 */

#include "mqtt.h"
#include "config.h"
#include "wol_ping.h"
#include "helpers.h"

WiFiClientSecure espClient;
PubSubClient mqtt(espClient);

bool chkUpdate = false;

void setupMQTT() {
  espClient.setInsecure();
  mqtt.setServer(config.mqtt_server, config.mqtt_port);
  mqtt.setCallback(mqttCallback);
}

bool mqttConnected() { 
  return mqtt.connected();
}

void ensureMqtt() {
  while(!mqtt.connected()){
    if(mqtt.connect("ESP32C3-WOL", config.mqtt_user, config.mqtt_password)){
      mqtt.publish("wol/status","MQTT Ready",true);
      mqtt.subscribe("wol/event");

    } else {
      delay(2000);
    } 
  }
}

void mqttLoop() { 
  mqtt.loop();
}

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  String msg; 
  for(unsigned int i = 0;i < len; i++) {
    msg += (char)payload[i];
    msg.trim();
  }

  if(msg == "TurnOn"){
    blinkDigit(2);
    sendWOL("MQTT", 10);

  } else if(msg == "TurnOff"){
    blinkDigit(2);
    //... coming soon

  } else if(msg == "CheckUpdate"){
    blinkDigit(2);
    chkUpdate = true;

  } else if(msg == "PingPC"){
    blinkDigit(2);
    doPing();

  } else if(msg == "PinOut1On"){
    blinkDigit(2);
    digitalWrite(PIN1_GPIO, HIGH);
    mqttPublish("PinOut 1 -> ON");

  } else if(msg == "PinOut1Off"){
    blinkDigit(2);
    digitalWrite(PIN1_GPIO, LOW);
    mqttPublish("PinOut 1 -> OFF");

  } else if(msg == "PinOut2On"){
    blinkDigit(2);
    digitalWrite(PIN2_GPIO, HIGH);
    mqttPublish("PinOut 2 -> ON");

  } else if(msg == "PinOut2Off"){
    blinkDigit(2);
    digitalWrite(PIN2_GPIO, LOW);
    mqttPublish("PinOut 2 -> OFF");
  }

  mqtt.publish("wol/event", "", true);
}
/*
 * configPortal.cpp
 * -------------------------------
 * Implements the WiFi/MQTT configuration portal:
 *  - Serves HTML form to input SSID, MQTT server, and WOL target
 *  - Parses POST requests to save config
 *  - Stores configuration to SPIFFS using saveConfig()
 *  - Restarts ESP32 after saving
 */

#include "configPortal.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <WiFi.h>

WebServer server(80);

void handleRoot(){
  if(!SPIFFS.begin(true)){ 
    server.send(500,"text/plain","SPIFFS error");
    return;
  }
  
  File f = SPIFFS.open("/setup.html","r");
  
  if(!f){ 
    server.send(404,"text/plain","File not found");
    return;
  }
  
  server.streamFile(f,"text/html");
  f.close();
}

void handleSave(){
  if(!SPIFFS.begin(true)){ 
    server.send(500,"text/plain","SPIFFS error");
    return;
  }

  Config newCfg;

  strlcpy(newCfg.ssid, server.arg("ssid").c_str(), sizeof(newCfg.ssid));
  strlcpy(newCfg.password, server.arg("password").c_str(), sizeof(newCfg.password));
  strlcpy(newCfg.mqtt_server, server.arg("mqtt_server").c_str(), sizeof(newCfg.mqtt_server));
  newCfg.mqtt_port = server.arg("mqtt_port").toInt();
  strlcpy(newCfg.mqtt_user, server.arg("mqtt_user").c_str(), sizeof(newCfg.mqtt_user));
  strlcpy(newCfg.mqtt_password, server.arg("mqtt_password").c_str(), sizeof(newCfg.mqtt_password));
  strlcpy(newCfg.target_ip, server.arg("target_ip").c_str(), sizeof(newCfg.target_ip));
  strlcpy(newCfg.broadcastIPStr, server.arg("broadcastIP").c_str(), sizeof(newCfg.broadcastIPStr));

  String macStr = server.arg("mac_address");
  sscanf(macStr.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
         &newCfg.mac_address[0], &newCfg.mac_address[1], &newCfg.mac_address[2],
         &newCfg.mac_address[3], &newCfg.mac_address[4], &newCfg.mac_address[5]);

  newCfg.udp_port = server.arg("udp_port").toInt();

  if(saveConfig(newCfg)){
    server.send(200,"text/html","<h3>Config saved! Rebooting...</h3>");
    delay(2000); 
    ESP.restart();
  } else {
    server.send(500,"text/html","<h3>Error saving config!</h3>");
  }
}

void startConfigPortal(){
  WiFi.mode(WIFI_AP);
  WiFi.softAP("WOL_ESP32C3_Setup");

  Serial.println("Connect to WiFi AP: ESP32C3_Config");
  Serial.println("Open http://192.168.4.1 in browser");

  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
}

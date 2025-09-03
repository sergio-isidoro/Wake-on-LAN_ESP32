/*
 * ESP32-C3 WOL + MQTT + OTA - Versão Completa Interativa com Broadcast IP
 * Funcionalidades:
 * - Setup interativo via Serial se /config.json não existir
 * - Reset de fábrica segurando D2 (GPIO2) durante boot
 * - Configuração WiFi/MQTT/OTA/WOL via JSON
 * - Broadcast IP configurável para WOL
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ESP32Ping.h>
#include <WiFiUdp.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include <esp_ota_ops.h>

#define FIRMWARE_VERSION "5.0-interactive"
#define RESET_BUTTON_PIN D0 // D0 = GPIO0 para reset de fábrica

struct Config {
  char ssid[32];
  char password[64];
  char mqtt_server[64];
  int mqtt_port;
  char mqtt_user[32];
  char mqtt_password[64];
  char versionURL[128];
  char firmwareURL[128];
  int ota_check_interval_ms;
  int button_gpio;
  int led_gpio;
  char target_ip[16];
  uint8_t mac_address[6];
  int udp_port;
  char broadcastIPStr[16]; // para JSON
  IPAddress broadcastIP;    // convertido do broadcastIPStr
};

// ===== Variáveis Globais =====
Config config;
WiFiClientSecure espClient;
PubSubClient mqtt(espClient);
WiFiUDP udp;
RTC_DATA_ATTR bool justUpdated = false;
unsigned long lastOTACheck = 0;
bool buttonTriggered = false;
unsigned long lastAction = 0;
String lastReason;

// ======== Funções de Configuração ========

bool saveConfig(const Config &cfg) {
  if (!SPIFFS.begin(true)) return false;
  File f = SPIFFS.open("/config.json","w");
  if(!f) return false;
  DynamicJsonDocument doc(1536);
  doc["ssid"]=cfg.ssid; doc["password"]=cfg.password;
  doc["mqtt_server"]=cfg.mqtt_server; doc["mqtt_port"]=cfg.mqtt_port;
  doc["mqtt_user"]=cfg.mqtt_user; doc["mqtt_password"]=cfg.mqtt_password;
  doc["versionURL"]=cfg.versionURL; doc["firmwareURL"]=cfg.firmwareURL;
  doc["ota_check_interval_ms"]=cfg.ota_check_interval_ms;
  doc["button_gpio"]=cfg.button_gpio; doc["led_gpio"]=cfg.led_gpio;
  doc["target_ip"]=cfg.target_ip; doc["udp_port"]=cfg.udp_port;
  doc["broadcastIP"]=cfg.broadcastIPStr;
  JsonArray mac=doc.createNestedArray("mac_address");
  for(int i=0;i<6;i++) mac.add(cfg.mac_address[i]);
  if(serializeJsonPretty(doc,f)==0){f.close();return false;}
  f.close();
  return true;
}

bool loadConfig() {
  if(!SPIFFS.begin(true)) return false;
  if(!SPIFFS.exists("/config.json")) return false;
  File f = SPIFFS.open("/config.json","r");
  if(!f) return false;
  DynamicJsonDocument doc(1536);
  DeserializationError err = deserializeJson(doc,f);
  f.close();
  if(err) return false;

  strlcpy(config.ssid, doc["ssid"], sizeof(config.ssid));
  strlcpy(config.password, doc["password"], sizeof(config.password));
  strlcpy(config.mqtt_server, doc["mqtt_server"], sizeof(config.mqtt_server));
  config.mqtt_port = doc["mqtt_port"];
  strlcpy(config.mqtt_user, doc["mqtt_user"], sizeof(config.mqtt_user));
  strlcpy(config.mqtt_password, doc["mqtt_password"], sizeof(config.mqtt_password));
  strlcpy(config.versionURL, doc["versionURL"], sizeof(config.versionURL));
  strlcpy(config.firmwareURL, doc["firmwareURL"], sizeof(config.firmwareURL));
  config.ota_check_interval_ms = doc["ota_check_interval_ms"];
  config.button_gpio = doc["button_gpio"];
  config.led_gpio = doc["led_gpio"];
  strlcpy(config.target_ip, doc["target_ip"], sizeof(config.target_ip));
  strlcpy(config.broadcastIPStr, doc["broadcastIP"], sizeof(config.broadcastIPStr));
  config.broadcastIP.fromString(config.broadcastIPStr);
  JsonArray mac = doc["mac_address"];
  for(int i=0;i<6;i++) config.mac_address[i] = mac[i];
  config.udp_port = doc["udp_port"];
  return true;
}

void factoryReset() {
  Serial.println("Reset de fábrica ativado...");
  if(SPIFFS.exists("/config.json")) SPIFFS.remove("/config.json");
  Serial.println("Configuração apagada. Reiniciando...");
  delay(3000); ESP.restart();
}

void runSerialSetup() {
  Serial.println("\n--- CONFIGURAÇÃO INICIAL ---");
  Config newCfg;
  auto readLine=[]()->String{while(Serial.available()==0)delay(50); String s=Serial.readStringUntil('\n'); s.trim(); return s;};

  Serial.print("SSID: "); strlcpy(newCfg.ssid, readLine().c_str(),sizeof(newCfg.ssid));
  Serial.print("Password WiFi: "); strlcpy(newCfg.password, readLine().c_str(),sizeof(newCfg.password));
  Serial.print("MQTT server: "); strlcpy(newCfg.mqtt_server, readLine().c_str(),sizeof(newCfg.mqtt_server));
  Serial.print("MQTT port: "); newCfg.mqtt_port=readLine().toInt();
  Serial.print("MQTT user: "); strlcpy(newCfg.mqtt_user, readLine().c_str(),sizeof(newCfg.mqtt_user));
  Serial.print("MQTT password: "); strlcpy(newCfg.mqtt_password, readLine().c_str(),sizeof(newCfg.mqtt_password));
  Serial.print("URL version.txt: "); strlcpy(newCfg.versionURL, readLine().c_str(),sizeof(newCfg.versionURL));
  Serial.print("URL firmware.bin: "); strlcpy(newCfg.firmwareURL, readLine().c_str(),sizeof(newCfg.firmwareURL));
  Serial.print("IP PC (WOL): "); strlcpy(newCfg.target_ip, readLine().c_str(),sizeof(newCfg.target_ip));
  Serial.print("MAC PC (XX:XX:XX:XX:XX:XX): "); 
  String macStr=readLine(); sscanf(macStr.c_str(),"%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
    &newCfg.mac_address[0],&newCfg.mac_address[1],&newCfg.mac_address[2],
    &newCfg.mac_address[3],&newCfg.mac_address[4],&newCfg.mac_address[5]);
  Serial.print("Broadcast IP (ex: 192.168.1.255): "); strlcpy(newCfg.broadcastIPStr, readLine().c_str(), sizeof(newCfg.broadcastIPStr));
  newCfg.broadcastIP.fromString(newCfg.broadcastIPStr);

  // Defaults
  newCfg.ota_check_interval_ms=300000;
  newCfg.button_gpio=0; newCfg.led_gpio=8; newCfg.udp_port=9;

  if(saveConfig(newCfg)) Serial.println("Configuração gravada com sucesso!");
  else Serial.println("Erro ao gravar config!");
  delay(3000); ESP.restart();
}

// ===== WiFi + MQTT + WOL Helpers =====
void mqttPublish(const char* msg){Serial.println(msg);if(mqtt.connected()) mqtt.publish("wol/log",msg);}
void setupWiFi(){Serial.print("Conectando WiFi ");Serial.println(config.ssid);WiFi.begin(config.ssid,config.password);unsigned long t=millis();while(WiFi.status()!=WL_CONNECTED){delay(500);Serial.print(".");if(millis()-t>30000){Serial.println("Timeout WiFi");mqttPublish("Timeout WiFi");break;}}if(WiFi.status()==WL_CONNECTED){Serial.println("\nWiFi conectado!");Serial.print("IP: ");Serial.println(WiFi.localIP());}}
void ensureMqtt(){while(!mqtt.connected()){Serial.print("Conectando MQTT...");if(mqtt.connect("ESP32C3-WOL",config.mqtt_user,config.mqtt_password)){Serial.println("connected");mqttPublish(("Boot v"+String(FIRMWARE_VERSION)).c_str());mqtt.publish("wol/status","Ready",true);mqtt.subscribe("wol/event");}else{Serial.print("falhou rc=");Serial.println(mqtt.state());delay(2000);}}}

void sendMagicPacketBurst(const char* reason){
  Serial.println("WOL reason:"+String(reason));
  if(mqtt.connected()) mqtt.publish("wol/event",reason);
  uint8_t packet[102]; memset(packet,0xFF,6); for(int i=0;i<16;i++) memcpy(&packet[6+i*6],config.mac_address,6);
  if(!udp.beginPacket(config.broadcastIP,config.udp_port)){mqttPublish("UDP beginPacket failed");return;}
  udp.write(packet,sizeof(packet)); udp.endPacket();
  mqttPublish("WOL sent");
}

// ======================= OTA =======================
bool isUpdateAvailable(String &remoteVersion){
  HTTPClient http; http.begin(config.versionURL); int code=http.GET();
  if(code==200){remoteVersion=http.getString();remoteVersion.trim();return remoteVersion!=String(FIRMWARE_VERSION);}
  return false;
}

void performOTA(){
  String remoteVer; if(!isUpdateAvailable(remoteVer)) return;
  mqttPublish(("OTA available: "+remoteVer).c_str());
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http; http.begin(client,config.firmwareURL); int httpCode=http.GET(); if(httpCode!=200){mqttPublish(("Falha OTA http:"+String(httpCode)).c_str());http.end();return;}
  int contentLen=http.getSize(); WiFiClient* stream=http.getStreamPtr();
  const esp_partition_t* update_partition=esp_ota_get_next_update_partition(NULL);
  if(!update_partition){mqttPublish("Falha OTA partição");http.end();return;}
  esp_ota_handle_t ota_handle; if(esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle)!=ESP_OK){mqttPublish("Falha esp_ota_begin");http.end();return;}
  uint8_t buf[1024]; int len; bool ok=true;
  while((len=stream->read(buf,sizeof(buf)))>0){if(esp_ota_write(ota_handle,buf,len)!=ESP_OK){mqttPublish("esp_ota_write failed");ok=false;break;}}
  if(!ok){esp_ota_end(ota_handle);http.end();return;}
  if(esp_ota_end(ota_handle)!=ESP_OK){mqttPublish("esp_ota_end failed");http.end();return;}
  if(esp_ota_set_boot_partition(update_partition)!=ESP_OK){mqttPublish("Falha set_boot_partition");http.end();return;}
  justUpdated=true; mqttPublish("OTA concluída, reiniciando"); http.end(); delay(1000); ESP.restart();
}

void otaSuccessCheck(){if(justUpdated){mqttPublish("OTA success, rebooted");justUpdated=false;}}

// ======================= MQTT Callback =======================
void mqttCallback(char* topic, byte* payload, unsigned int length){
  String msg=""; for(unsigned int i=0;i<length;i++) msg+=(char)payload[i]; msg.trim(); 
  Serial.println("MQTT: "+msg);
  if(msg=="TurnOn"){digitalWrite(config.led_gpio,HIGH);lastReason="MQTT TurnOn";sendMagicPacketBurst(lastReason.c_str());lastAction=millis();delay(100);digitalWrite(config.led_gpio,LOW);}
  else if(msg=="PingPC"){digitalWrite(config.led_gpio,HIGH);lastReason="MQTT PingPC";bool r=Ping.ping(config.target_ip,3);mqtt.publish("wol/status",r?"On":"Off",true);lastAction=millis();delay(100);digitalWrite(config.led_gpio,LOW);}
}

// ======================= SETUP =======================
void setup() {
  Serial.begin(115200);
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  Serial.println("\n=== ESP32-C3 WOL/MQTT/OTA ===");
  delay(2000);
  if(digitalRead(RESET_BUTTON_PIN)==LOW) factoryReset();

  if(!loadConfig()) runSerialSetup();

  pinMode(config.led_gpio,OUTPUT); digitalWrite(config.led_gpio,LOW);
  pinMode(config.button_gpio,INPUT_PULLUP);

  setupWiFi();
  espClient.setInsecure();
  mqtt.setServer(config.mqtt_server,config.mqtt_port);
  mqtt.setCallback(mqttCallback);

  udp.begin(config.udp_port);
  esp_task_wdt_init(10,true); esp_task_wdt_add(NULL);

  otaSuccessCheck();
  Serial.println("Firmware v"+String(FIRMWARE_VERSION));
  lastAction=millis();
}

// ======================= LOOP =======================
void loop() {
  esp_task_wdt_reset();
  if(!mqtt.connected()) ensureMqtt();
  mqtt.loop();

  // Button >1s → WOL
  static unsigned long t0=0;
  if(digitalRead(config.button_gpio)==LOW){
    if(!buttonTriggered){buttonTriggered=true;t0=millis();}
    else if(millis()-t0>1000){digitalWrite(config.led_gpio,HIGH);lastReason="Button GPIO";sendMagicPacketBurst(lastReason.c_str());buttonTriggered=false;t0=0;lastAction=millis();delay(100);digitalWrite(config.led_gpio,LOW);}
  }else{buttonTriggered=false;t0=0;}

  // Ping periodic
  if(millis()-lastAction>300000UL){digitalWrite(config.led_gpio,HIGH);mqttPublish("Periodic ping");bool r=Ping.ping(config.target_ip,3);mqtt.publish("wol/status",r?"On":"Off",true);lastAction=millis();delay(100);digitalWrite(config.led_gpio,LOW);}

  // OTA check
  if(millis()-lastOTACheck>config.ota_check_interval_ms){lastOTACheck=millis();performOTA();}

  delay(10);
}

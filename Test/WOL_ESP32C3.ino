/*
 * ESP32-C3 WOL + MQTT + OTA with GitHub
 * ESP32C3 Dev Module (Seeed Studio)
 * ---------------------------------------
 */

 #include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HTTPUpdate.h>
#include <esp_task_wdt.h>
#include <ESP32Ping.h>
#include <WiFiUdp.h>
#include <esp_ota_ops.h>
#include <esp_http_client.h>
#include <esp_https_ota.h>

// =======================
// WIFI CONFIG
// =======================
const char* ssid       = "SI_WIFI";    
const char* password   = "SInternet"; 

// =======================
// MQTT CONFIG
// =======================
const char* mqtt_server     = "b6694cd5ec7d4b9b82398eb149201b12.s1.eu.hivemq.cloud";
const int   mqtt_port       = 8883;
const char* mqtt_user       = "manoper";
const char* mqtt_password   = "Sergio93";

const char* mqtt_topic_status = "wol/status";
const char* mqtt_topic_event  = "wol/event";
const char* mqtt_topic_log    = "wol/log";

WiFiClientSecure espClient;
PubSubClient mqtt(espClient);

// =======================
// OTA CONFIG
// =======================
#define FIRMWARE_VERSION "3.0"
const char* versionURL  = "https://raw.githubusercontent.com/sergio-isidoro/Wake-on-LAN_ESP32C3/main/firmware/version.txt";
const char* firmwareURL = "https://raw.githubusercontent.com/sergio-isidoro/Wake-on-LAN_ESP32C3/main/firmware/WOL_ESP32C3.bin";

RTC_DATA_ATTR bool justUpdated = false; // Confirm OTA after reboot
unsigned long lastOTACheck = 0;
const unsigned long OTA_CHECK_INTERVAL_MS = 60000UL; // 1 min

// =======================
// BUTTON + LED CONFIG
// =======================
#define BUTTON_GPIO D0
#define LED_GPIO    D1

bool buttonTriggered = false;
unsigned long lastAction = 0;
String lastReason;

// =======================
// WOL CONFIG
// =======================
const IPAddress targetIP(192, 168, 2, 229);
const char* broadcastIP = "192.168.2.255";
const int udpPort = 9;
const uint8_t macAddress[6] = {0xC8, 0x5A, 0xCF, 0xDE, 0x5C, 0xA1};

WiFiUDP udp;

// =======================
// HELPERS
// =======================
void mqttPublish(const char* msg) {
  Serial.println(msg);
  mqtt.publish(mqtt_topic_log, msg);
}

void blinkDigit(int n) {
  for (int i = 0; i < n; i++) {
    digitalWrite(LED_GPIO, HIGH);
    delay(100);
    digitalWrite(LED_GPIO, LOW);
    delay(100);
  }
}

void blinkVersion(const char* version) {
  int len = strlen(version);
  for (int i = 0; i < len; i++) {
    if (version[i] >= '0' && version[i] <= '9') {
      blinkDigit(version[i] - '0');
      delay(500);
    }
  }
}

void setupWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
}

void ensureMqtt() {
  while (!mqtt.connected()) {
    Serial.print("Connecting to MQTT...");
    if (mqtt.connect("ESP32C3-WOL", mqtt_user, mqtt_password)) {
      Serial.println("connected!");
      mqttPublish(("Boot ESP32C3 version: " + String(FIRMWARE_VERSION)).c_str());
      mqtt.publish(mqtt_topic_status, "Ready", true);
      mqtt.subscribe(mqtt_topic_event);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" retrying in 2s...");
      delay(2000);
    }
  }
}

void sendMagicPacketBurst(const char* reason) {
  Serial.println("Sending WOL - reason: " + String(reason));
  mqtt.publish(mqtt_topic_event, reason);

  uint8_t packet[102];
  memset(packet, 0xFF, 6);
  for (int i = 0; i < 16; i++) {
    memcpy(&packet[6 + i * 6], macAddress, 6);
  }

  udp.beginPacket(broadcastIP, udpPort);
  udp.write(packet, sizeof(packet));
  udp.endPacket();

  mqttPublish("WOL sent");
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) message += (char)payload[i];
  message.trim();
  Serial.println("MQTT received: " + message);

  if (message == "TurnOn") {
    digitalWrite(LED_GPIO, HIGH);
    lastReason = "MQTT TurnOn";
    sendMagicPacketBurst(lastReason.c_str());
    lastAction = millis();
    delay(100);
    digitalWrite(LED_GPIO, LOW);
  } else if (message == "PingPC") {
    digitalWrite(LED_GPIO, HIGH);
    lastReason = "MQTT PingPC";
    bool reachable = Ping.ping(targetIP, 3);
    mqtt.publish(mqtt_topic_status, reachable ? "On" : "Off", true);
    mqttPublish("PingPC executed");
    lastAction = millis();
    delay(100);
    digitalWrite(LED_GPIO, LOW);
  }
}

void blinkBeforeOTA() {
  for (int i = 0; i < 8; i++) {
    digitalWrite(LED_GPIO, HIGH);
    delay(250);
    digitalWrite(LED_GPIO, LOW);
    delay(250);
  }
}

// =======================
// Check if OTA update is available
// =======================
bool isUpdateAvailable() {
  HTTPClient http;
  http.begin(versionURL);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String newVersion = http.getString();
    newVersion.trim();               // remove espaços e quebras de linha
    newVersion.replace("\n", "");   // remove qualquer newline remanescente
    newVersion.replace("\r", "");

    Serial.println("Remote version: " + newVersion);
    mqttPublish(("Remote version: " + newVersion).c_str());

    // Compare versions safely
    if (newVersion != String(FIRMWARE_VERSION)) {
      Serial.println("New version available!");
      mqttPublish(("New version available: " + newVersion).c_str());
      http.end();
      return true;
    } else {
      Serial.println("No new version.");
      mqttPublish("No new version available.");
    }

  } else {
    Serial.printf("Error checking version: %d\n", httpCode);
    mqttPublish(("Error checking version: " + String(httpCode)).c_str());
  }

  http.end();
  return false;
}

// =======================
// Perform OTA update
// =======================
void performOTA() {
  if (!isUpdateAvailable()) return;

  // Blink LED 2s before updating
  blinkBeforeOTA();
  mqttPublish("Starting OTA update...");
  justUpdated = true;

  const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);
  if (!update_partition) {
    mqttPublish("Failed to get OTA update partition");
    justUpdated = false;
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.begin(client, firmwareURL);
  int httpCode = http.GET();
  if (httpCode != 200) {
    mqttPublish(("Failed to download firmware, HTTP code: " + String(httpCode)).c_str());
    justUpdated = false;
    http.end();
    return;
  }

  WiFiClient* stream = http.getStreamPtr();
  esp_ota_handle_t ota_handle;
  if (esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle) != ESP_OK) {
    mqttPublish("esp_ota_begin failed");
    justUpdated = false;
    http.end();
    return;
  }

  uint8_t buf[1024];
  int len;
  while ((len = stream->read(buf, sizeof(buf))) > 0) {
    if (esp_ota_write(ota_handle, buf, len) != ESP_OK) {
      mqttPublish("esp_ota_write failed");
      esp_ota_end(ota_handle);
      justUpdated = false;
      http.end();
      return;
    }
  }

  if (esp_ota_end(ota_handle) != ESP_OK) {
    mqttPublish("esp_ota_end failed");
    justUpdated = false;
    http.end();
    return;
  }

  if (esp_ota_set_boot_partition(update_partition) != ESP_OK) {
    mqttPublish("Failed to set boot partition");
    justUpdated = false;
    http.end();
    return;
  }

  mqttPublish("OTA update finished, restarting...");
  http.end();
  delay(1000);
  ESP.restart();
}


void otaSuccessCheck() {
  if (justUpdated) {
    mqttPublish("OTA success, rebooted!");
    justUpdated = false;
  }
}

// =======================
// SETUP
// =======================
void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_GPIO, INPUT_PULLUP);
  pinMode(LED_GPIO, OUTPUT);
  digitalWrite(LED_GPIO, LOW);

  blinkVersion(FIRMWARE_VERSION);

  setupWiFi();
  espClient.setInsecure();
  mqtt.setServer(mqtt_server, mqtt_port);
  mqtt.setCallback(mqttCallback);

  udp.begin(udpPort);

  esp_task_wdt_init(10, true);
  esp_task_wdt_add(NULL);

  otaSuccessCheck();
  Serial.println("Firmware version: " FIRMWARE_VERSION);
  lastAction = millis();
}

// =======================
// LOOP
// =======================
void loop() {
  esp_task_wdt_reset();

  if (!mqtt.connected()) ensureMqtt();
  mqtt.loop();

  // Button >1s → WOL
  static unsigned long t0 = 0;
  if (digitalRead(BUTTON_GPIO) == LOW) {
    if (!buttonTriggered) { buttonTriggered = true; t0 = millis(); }
    else if (millis() - t0 > 1000) {
      digitalWrite(LED_GPIO, HIGH);
      lastReason = "Button GPIO";
      sendMagicPacketBurst(lastReason.c_str());
      buttonTriggered = false; t0 = 0; lastAction = millis();
      delay(100); digitalWrite(LED_GPIO, LOW);
    }
  } else { buttonTriggered = false; t0 = 0; }

  // Ping every 5 min
  if (millis() - lastAction > 300000UL) {
    digitalWrite(LED_GPIO, HIGH);
    mqttPublish("Periodic ping");
    bool reachable = Ping.ping(targetIP, 3);
    mqtt.publish(mqtt_topic_status, reachable ? "On" : "Off", true);
    lastAction = millis();
    delay(100); digitalWrite(LED_GPIO, LOW);
  }

  // OTA check
  if (millis() - lastOTACheck > OTA_CHECK_INTERVAL_MS) {
    lastOTACheck = millis();
    performOTA();
  }

  delay(10);
}

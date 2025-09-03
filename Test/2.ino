/* 
 * ESP32-C3 WOL + MQTT + OTA (corrigido)
 * Correções: includes, broadcastIP, partições ota_0/ota_1 checks, logs e fluxo OTA mais robusto.
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HTTPClient.h>           // <<-- ADDED (faltava)
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
const char* ssid       = "<YOUR_WIFI_SSID>";    
const char* password   = "<YOUR_WIFI_PASSWORD>"; 

// =======================
// MQTT CONFIG
// =======================
const char* mqtt_server     = "<YOUR_MQTT_SERVER>";
const int   mqtt_port       = 8883;
const char* mqtt_user       = "<YOUR_MQTT_USER>";
const char* mqtt_password   = "<YOUR_MQTT_PASSWORD>";

const char* mqtt_topic_status = "wol/status";
const char* mqtt_topic_event  = "wol/event";
const char* mqtt_topic_log    = "wol/log";

WiFiClientSecure espClient;
PubSubClient mqtt(espClient);

// =======================
// OTA CONFIG
// =======================
#define FIRMWARE_VERSION "3.0"
const char* versionURL  = "<URL_TO_VERSION_TXT>";      // texto c/ versão (ex: "3.1")
const char* firmwareURL = "<URL_TO_FIRMWARE_BIN>";     // .bin (HTTPS preferível)

RTC_DATA_ATTR bool justUpdated = false;                 // persiste após reboot
unsigned long lastOTACheck = 0;
const unsigned long OTA_CHECK_INTERVAL_MS = 60000UL; // 1 min (ajusta conforme necessidade)

// =======================
// BUTTON + LED CONFIG
// =======================
// Nota: ESP32-C3 não define D0/D1 como no Arduino UNO.
// Usa números GPIO reais - ajusta conforme a tua placa.
#define BUTTON_GPIO 0   // exemplo: GPIO0 (ajusta para um pino livre na tua placa C3)
#define LED_GPIO    2   // exemplo: LED na GPIO2 (ajusta conforme necessário)

bool buttonTriggered = false;
unsigned long lastAction = 0;
String lastReason;

// =======================
// WOL CONFIG
// =======================
const IPAddress targetIP(192, 168, XXX, XXX); // substitui XXX
const int udpPort = 9;
const uint8_t macAddress[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}; // valores fictícios

WiFiUDP udp;
IPAddress broadcastIP(255,255,255,255); // uso simples: broadcast global (podes calcular pela subrede se preferires)

// =======================
// HELPERS
// =======================
void mqttPublish(const char* msg) {
  Serial.println(msg);
  if (mqtt.connected()) mqtt.publish(mqtt_topic_log, msg);
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
  unsigned long tstart = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    // timeout opcional
    if (millis() - tstart > 30000) {
      Serial.println("\nWiFi connect timeout");
      mqttPublish("WiFi connect timeout");
      break;
    }
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP: "); Serial.println(WiFi.localIP());
  }
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
  if (mqtt.connected()) mqtt.publish(mqtt_topic_event, reason);

  uint8_t packet[102];
  memset(packet, 0xFF, 6);
  for (int i = 0; i < 16; i++) {
    memcpy(&packet[6 + i * 6], macAddress, 6);
  }

  // usa broadcastIP definido acima
  if (!udp.beginPacket(broadcastIP, udpPort)) {
    mqttPublish("UDP beginPacket failed");
    return;
  }
  udp.write(packet, sizeof(packet));
  udp.endPacket();

  mqttPublish("WOL sent");
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) message += (char)payload[i];
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
bool isUpdateAvailable(String &remoteVersionOut) {
  HTTPClient http;
  http.begin(versionURL);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String newVersion = http.getString();
    newVersion.trim();
    newVersion.replace("\n", "");
    newVersion.replace("\r", "");

    Serial.println("Remote version: " + newVersion);
    mqttPublish(("Remote version: " + newVersion).c_str());

    remoteVersionOut = newVersion;

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
  String remoteVer;
  if (!isUpdateAvailable(remoteVer)) return;

  // Log partições (activo vs próxima) — ajuda a diagnosticar
  const esp_partition_t* boot = esp_ota_get_boot_partition();
  const esp_partition_t* next = esp_ota_get_next_update_partition(NULL);
  if (boot) {
    Serial.printf("Boot partition: type %d subtype %d label %s addr 0x%08x\n",
                  boot->type, boot->subtype, boot->label ? boot->label : "NULL", boot->address);
    mqttPublish(("Boot partition: " + String(boot->label ? boot->label : "NULL")).c_str());
  }
  if (next) {
    Serial.printf("Next update partition: label %s addr 0x%08x\n",
                  next->label ? next->label : "NULL", next->address);
    mqttPublish(("Next partition: " + String(next->label ? next->label : "NULL")).c_str());
  }

  // Blink LED antes da OTA
  blinkBeforeOTA();
  mqttPublish("Starting OTA update...");

  // Usa client seguro para HTTPS (ou insecure se self-signed)
  WiFiClientSecure client;
  client.setInsecure(); // se quiser verificar cert, remove isto e configura CA

  HTTPClient http;
  http.begin(client, firmwareURL);
  int httpCode = http.GET();
  if (httpCode != 200) {
    mqttPublish(("Failed to download firmware, HTTP code: " + String(httpCode)).c_str());
    http.end();
    return;
  }

  // obter stream e tamanho (quando disponível)
  int contentLen = http.getSize(); // pode ser -1 se chunked
  WiFiClient *stream = http.getStreamPtr();

  // obter partição de escrita (ota_0 ou ota_1 alternadas automaticamente)
  const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);
  if (!update_partition) {
    mqttPublish("Failed to get OTA update partition");
    http.end();
    return;
  }

  esp_ota_handle_t ota_handle;
  if (esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle) != ESP_OK) {
    mqttPublish("esp_ota_begin failed");
    http.end();
    return;
  }

  // marcaremos justUpdated apenas depois de set_boot_partition com sucesso
  uint8_t buf[1024];
  int len;
  bool write_ok = true;
  Serial.println("Writing firmware to flash...");
  while ((len = stream->read(buf, sizeof(buf))) > 0) {
    if (esp_ota_write(ota_handle, buf, len) != ESP_OK) {
      mqttPublish("esp_ota_write failed");
      write_ok = false;
      break;
    }
  }

  if (!write_ok) {
    esp_ota_end(ota_handle);
    http.end();
    return;
  }

  if (esp_ota_end(ota_handle) != ESP_OK) {
    mqttPublish("esp_ota_end failed");
    http.end();
    return;
  }

  // validação simples: podes adicionar verificação de binário/MD5 se o servidor fornecer
  if (esp_ota_set_boot_partition(update_partition) != ESP_OK) {
    mqttPublish("Failed to set boot partition");
    http.end();
    return;
  }

  // grava flag para sinalizar reboot com nova fw (RTC_DATA_ATTR)
  justUpdated = true;
  mqttPublish("OTA update finished, restarting...");
  http.end();
  delay(1000);
  ESP.restart();
}

void otaSuccessCheck() {
  if (justUpdated) {
    mqttPublish("OTA success, rebooted!");
    // limpa flag para não repetir a mensagem
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

  // watchdog (opcional)
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

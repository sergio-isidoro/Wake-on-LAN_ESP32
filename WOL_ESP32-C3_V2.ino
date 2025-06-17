#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESP32Ping.h>
#include <AsyncMqttClient.h>

#define LED_PIN D1
#define BUTTON_GPIO D0
#define WATCHDOG_TIMEOUT 15000

const char* ssid = "...";
const char* password = "...";

#define MQTT_HOST "..."
#define MQTT_PORT 8883
const char* mqtt_user = "...";
const char* mqtt_password = "...";
const char* mqtt_topic_status = "wol/status";
const char* mqtt_topic_event = "wol/event";
const char* mqtt_topic_log = "wol/log";

const IPAddress targetIP(192, 168, 1, 100);
const char* broadcastIP = "192.168.1.255";
const int udpPort = 9;
const uint8_t macAddress[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

bool wakeRequested = false;
bool gpioTriggered = false;

WiFiUDP udp;
AsyncMqttClient mqttClient;
hw_timer_t* watchdog = NULL;
unsigned long lastActionTime = 0;

void IRAM_ATTR resetModule() {
  esp_restart();
}

void resetWatchdog() {
  timerWrite(watchdog, 0);
}

void setupWatchdog() {
  watchdog = timerBegin(0, 80, true);  // 80MHz / 80 = 1 tick = 1us
  timerAttachInterrupt(watchdog, &resetModule, true);
  timerAlarmWrite(watchdog, WATCHDOG_TIMEOUT * 1000, false);
  timerAlarmEnable(watchdog);
}

void sendMagicPacketBurst() {
  Serial.println("Sending Magic Packets...");
  mqttClient.publish(mqtt_topic_log, 1, false, "Sending Magic Packets...");

  for (int i = 0; i < 10; i++) {
    uint8_t packet[102];
    memset(packet, 0xFF, 6);
    for (int j = 1; j <= 16; j++) {
      memcpy(&packet[j * 6], macAddress, 6);
    }

    udp.beginPacket(broadcastIP, udpPort);
    udp.write(packet, sizeof(packet));
    udp.endPacket();
    delay(100);
  }

  mqttClient.publish(mqtt_topic_status, 2, false, "Magic Packet sent");
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties,
                   size_t len, size_t index, size_t total) {
  String message = String(payload).substring(0, len);
  Serial.print("MQTT received: ");
  Serial.println(message);

  if (message == "turnon") {
    wakeRequested = true;
    mqttClient.publish(mqtt_topic_log, 1, false, "MQTT 'turnon' received");
    sendMagicPacketBurst();
    mqttClient.publish(mqtt_topic_event, 2, true, "");
    lastActionTime = millis();
  }
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("MQTT connected");
  mqttClient.subscribe(mqtt_topic_event, 2);
  mqttClient.publish(mqtt_topic_log, 1, false, "MQTT connected");

  // Ping
  if (Ping.ping(targetIP, 3)) {
    Serial.println("PC is ONLINE");
    mqttClient.publish(mqtt_topic_status, 2, false, "online");
  } else {
    Serial.println("PC is OFFLINE");
    mqttClient.publish(mqtt_topic_status, 2, false, "offline");
  }

  lastActionTime = millis();
}

void connectToMqtt() {
  mqttClient.setClientId("ESP32Client");
  mqttClient.setCredentials(mqtt_user, mqtt_password);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setSecure(true);
  mqttClient.setKeepAlive(15);
  mqttClient.setWill(mqtt_topic_status, 2, false, "offline");

  mqttClient.onMessage(onMqttMessage);
  mqttClient.onConnect(onMqttConnect);
  mqttClient.connect();
}

void IRAM_ATTR gpioISR() {
  gpioTriggered = true;
}

void enterLightSleep(uint64_t duration_us) {
  esp_sleep_enable_timer_wakeup(duration_us);
  esp_light_sleep_start();
}

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  setupWatchdog();

  // Set up GPIO wake
  pinMode(BUTTON_GPIO, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_GPIO), gpioISR, FALLING);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    resetWatchdog();
  }
  Serial.println("\nWiFi connected");
  udp.begin(udpPort);

  connectToMqtt();
  lastActionTime = millis();
}

void loop() {
  resetWatchdog();

  // If GPIO triggered manually (button press)
  if (gpioTriggered) {
    gpioTriggered = false;
    mqttClient.publish(mqtt_topic_log, 1, false, "Wake: Button GPIO");
    sendMagicPacketBurst();
    lastActionTime = millis();
  }

  // If no action in last 10s → light sleep for 5s
  if (millis() - lastActionTime > 10000) {
    mqttClient.publish(mqtt_topic_log, 1, false, "Entering light sleep for 5s");
    delay(200); // ensure publish sent
    enterLightSleep(5 * 1000000);
    lastActionTime = millis(); // reset after wake
  }
}

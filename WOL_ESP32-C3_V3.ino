#include <WiFi.h>
#include <WiFiUdp.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ESP32Ping.h>

#define SLEEP_GPIO D2
#define LED_PIN D1
#define BUTTON_GPIO D0
#define WATCHDOG_TIMEOUT 15000

const char* ssid = "...";
const char* password = "...";

const char* mqtt_server = "...";
const int mqtt_port = ....;
const char* mqtt_user = "...";
const char* mqtt_password = "...";
const char* mqtt_topic_status = "wol/status";
const char* mqtt_topic_event = "wol/event";
const char* mqtt_topic_log = "wol/log";

const IPAddress targetIP(192, 168, 1, 100);
const char* broadcastIP = "192.168.1.255";
const int udpPort = 9;
const uint8_t macAddress[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

bool gpioSleep = false;
bool gpioTriggered = false;
RTC_DATA_ATTR int boot = 0;

WiFiUDP udp;
WiFiClientSecure net;
PubSubClient mqttClient(net);
hw_timer_t* watchdog = NULL;
unsigned long lastActionTime = 0;
TaskHandle_t mqttTaskHandle = NULL;

// ---- Variáveis novas ----
volatile unsigned long lastInterruptTime = 0; // debounce
String lastTriggerReason = "Boot";            // logging

void IRAM_ATTR resetModule() {
  esp_restart();
}

void resetWatchdog() {
  timerWrite(watchdog, 0);
}

void setupWatchdog() {
  watchdog = timerBegin(0, 80, true);
  timerAttachInterrupt(watchdog, &resetModule, true);
  timerAlarmWrite(watchdog, WATCHDOG_TIMEOUT * 1000, false);
  timerAlarmEnable(watchdog);
}

void sendMagicPacketBurst(const char* reason) {
  Serial.println("Sending Magic Packets...");
  mqttClient.publish(mqtt_topic_log, "Sending Magic Packets...");

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

  mqttClient.publish(mqtt_topic_status, "Magic Packet sent");

  // Inicia ciclo de ping
  delay(1000);
  bool reachable = Ping.ping(targetIP, 3);
  mqttClient.publish(mqtt_topic_status, reachable ? "On" : "Off");

  // Logging da razão
  String logMsg = "Magic Packet enviado - Razão: " + String(reason) + " @ " + String(millis()) + "ms";
  mqttClient.publish(mqtt_topic_log, logMsg.c_str());
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("MQTT received: ");
  Serial.println(message);

  if (message == "TurnOn") {
    digitalWrite(LED_PIN, HIGH);
    Serial.print("MQTT 'TurnOn' received");
    mqttClient.publish(mqtt_topic_log, "MQTT 'TurnOn' received");
    lastTriggerReason = "MQTT TurnOn";
    sendMagicPacketBurst(lastTriggerReason.c_str());
    digitalWrite(LED_PIN, LOW);
    lastActionTime = millis();
  }
}

void connectToMqtt() {
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);
  net.setInsecure();  // Ignora certificado SSL (apenas para testes)

  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT... ");
    if (mqttClient.connect("ESP32Client", mqtt_user, mqtt_password, mqtt_topic_status, 1, false, "offline")) {
      Serial.println("connected");
      mqttClient.subscribe(mqtt_topic_event);
      mqttClient.publish(mqtt_topic_log, "MQTT connected");

      // Limpar retained messages antigas
      mqttClient.publish(mqtt_topic_event, "", true);

      bool reachable = Ping.ping(targetIP, 3);
      mqttClient.publish(mqtt_topic_status, reachable ? "On" : "Off");
      lastActionTime = millis();
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" -> retrying...");
      delay(3000);
    }
  }
}

void mqttTask(void* parameter) {
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.reconnect();
    }

    if (!mqttClient.connected()) {
      connectToMqtt();
    }

    mqttClient.loop();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void IRAM_ATTR gpioISR() {
  unsigned long now = millis();
  if (now - lastInterruptTime > 200) { // debounce 200ms
    gpioTriggered = true;
    lastInterruptTime = now;
  }
}

void enterLightSleep(uint64_t duration_us) {
  esp_sleep_enable_timer_wakeup(duration_us);
  esp_light_sleep_start();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  boot++;

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  pinMode(SLEEP_GPIO, INPUT_PULLUP);
  if (digitalRead(SLEEP_GPIO) == 0){
    gpioSleep = true;
    setupWatchdog();
  }

  // Botão com pull-up interno
  pinMode(BUTTON_GPIO, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_GPIO), gpioISR, FALLING);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (digitalRead(SLEEP_GPIO) == 0) resetWatchdog();
  }

  Serial.println("\nWiFi connected");
  udp.begin(udpPort);

  connectToMqtt();

  xTaskCreatePinnedToCore(
    mqttTask,
    "MQTT Task",
    4096,
    NULL,
    1,
    &mqttTaskHandle,
    1
  );

  lastActionTime = millis();

  String bootMsg = "Boot #" + String(boot) + " @ " + String(millis()) + "ms";
  mqttClient.publish(mqtt_topic_log, bootMsg.c_str());
}

void loop() {

  if (gpioTriggered) {
    gpioTriggered = false;
    digitalWrite(LED_PIN, HIGH);
    lastTriggerReason = "Button GPIO";
    sendMagicPacketBurst(lastTriggerReason.c_str());
    delay(300);
    digitalWrite(LED_PIN, LOW);
    lastActionTime = millis();
  }

  if (gpioSleep) {
    resetWatchdog();
    if (millis() - lastActionTime > 10000) {
      mqttClient.publish(mqtt_topic_log, "Entering light sleep for 5s");
      Serial.print("Entering light sleep for 5s");
      digitalWrite(LED_PIN, LOW);
      delay(200);
      enterLightSleep(5 * 1000000);
      digitalWrite(LED_PIN, HIGH);
      lastActionTime = millis();
    }
  } else {
    if (millis() - lastActionTime > 60000) {
      mqttClient.publish(mqtt_topic_log, "Checking ping every 1 min");
      bool reachable = Ping.ping(targetIP, 3);
      mqttClient.publish(mqtt_topic_status, reachable ? "On" : "Off");
      lastActionTime = millis();
    }
  }
}

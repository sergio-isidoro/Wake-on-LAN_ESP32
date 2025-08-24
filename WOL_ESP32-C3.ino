#include <WiFi.h>              // Include WiFi library
#include <WiFiUdp.h>           // Include UDP support for WiFi
#include <WiFiClientSecure.h>  // Include secure WiFi client (TLS/SSL)
#include <PubSubClient.h>      // Include MQTT client library
#include <ESP32Ping.h>         // Include ping library for ESP32

#define SLEEP_GPIO D2          // GPIO used to trigger sleep mode
#define LED_PIN D1             // GPIO for LED indicator
#define BUTTON_GPIO D0         // GPIO for button input
#define WATCHDOG_TIMEOUT 15000 // Watchdog timeout in milliseconds

const char* ssid = "???";                   // WiFi SSID
const char* password = "???";               // WiFi password

const char* mqtt_server = "???";            // MQTT broker address
const int mqtt_port = ????;                 // MQTT broker port
const char* mqtt_user = "???";              // MQTT username
const char* mqtt_password = "???";          // MQTT password
const char* mqtt_topic_status = "wol/status"; // MQTT topic for status
const char* mqtt_topic_event  = "wol/event";  // MQTT topic for events
const char* mqtt_topic_log    = "wol/log";    // MQTT topic for logs

const IPAddress targetIP(192, 168, ???, ???); // Target device IP for ping / magic packet
const char* broadcastIP = "192.168.???.255";  // Broadcast IP for Magic Packet
const int udpPort = ????;                      // UDP port for Magic Packet
const uint8_t macAddress[6] = {0x??,0x??,0x??,0x??,0x??,0x??}; // Target MAC address

bool gpioSleep = false;                      // Flag for sleep mode
RTC_DATA_ATTR int boot = 0;                  // Persistent boot count across deep sleep

WiFiUDP udp;                                 // UDP instance
WiFiClientSecure net;                        // Secure network client
PubSubClient mqttClient(net);                // MQTT client
hw_timer_t* watchdog = NULL;                 // Watchdog timer
unsigned long lastActionTime = 0;            // Timestamp of last action

TaskHandle_t mqttTaskHandle = NULL;          // Handle for MQTT FreeRTOS task

// ---- New variables ----
volatile unsigned long lastInterruptTime = 0; // Debounce timestamp
volatile bool gpioTriggered = false;          // Flag for GPIO trigger
String lastTriggerReason = "Boot";            // Reason for last action (logging)

void IRAM_ATTR resetModule() {
  esp_restart();                              // Restart ESP32 immediately
}

void resetWatchdog() {
  timerWrite(watchdog, 0);                    // Reset watchdog timer counter
}

void setupWatchdog() {
  watchdog = timerBegin(0, 80, true);         // Initialize hardware timer
  timerAttachInterrupt(watchdog, &resetModule, true); // Attach ISR to reset module
  timerAlarmWrite(watchdog, WATCHDOG_TIMEOUT * 1000, false); // Set timeout
  timerAlarmEnable(watchdog);                // Enable timer alarm
}

void sendMagicPacketBurst(const char* reason) {
  Serial.println("Sending Magic Packets..."); // Log sending
  mqttClient.publish(mqtt_topic_log, "Sending Magic Packets..."); // MQTT log

  for (int i = 0; i < 10; i++) {            // Send 10 packets
    uint8_t packet[102];
    memset(packet, 0xFF, 6);                 // Start with 6 FF bytes
    for (int j = 1; j <= 16; j++) {         // Repeat MAC address 16 times
      memcpy(&packet[j*6], macAddress, 6);
    }

    udp.beginPacket(broadcastIP, udpPort);   // Send packet via UDP
    udp.write(packet, sizeof(packet));
    udp.endPacket();
    delay(100);
  }

  mqttClient.publish(mqtt_topic_status, "Magic Packet sent"); // Publish status

  delay(1000);
  bool reachable = Ping.ping(targetIP, 3);   // Ping target 3 times
  mqttClient.publish(mqtt_topic_status, reachable ? "On" : "Off"); // Publish ping result

  String logMsg = "Magic Packet sent - Reason: " + String(reason) + " @ " + String(millis()) + "ms"; // Log reason
  mqttClient.publish(mqtt_topic_log, logMsg.c_str());
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];             // Convert payload to string
  }

  Serial.print("MQTT received: ");           // Print received message
  Serial.println(message);

  if (message == "TurnOn") {                 // If MQTT command is "TurnOn"
    digitalWrite(LED_PIN, HIGH);             // Turn on LED
    Serial.print("MQTT 'TurnOn' received");  // Log to serial
    mqttClient.publish(mqtt_topic_log, "MQTT 'TurnOn' received"); // MQTT log
    lastTriggerReason = "MQTT TurnOn";       // Set trigger reason
    sendMagicPacketBurst(lastTriggerReason.c_str()); // Send Magic Packet
    digitalWrite(LED_PIN, LOW);              // Turn off LED
    lastActionTime = millis();               // Update last action time
  }
}

void connectToMqtt() {
  mqttClient.setServer(mqtt_server, mqtt_port);   // Set MQTT server
  mqttClient.setCallback(mqttCallback);           // Set callback
  net.setInsecure();                               // Ignore SSL certificates

  while (!mqttClient.connected()) {               // Keep trying to connect
    Serial.print("Connecting to MQTT... ");
    if (mqttClient.connect("ESP32Client", mqtt_user, mqtt_password, mqtt_topic_status, 1, false, "offline")) {
      Serial.println("connected");                // Connected
      mqttClient.subscribe(mqtt_topic_event);     // Subscribe to event topic
      mqttClient.publish(mqtt_topic_log, "MQTT connected"); // MQTT log

      mqttClient.publish(mqtt_topic_event, "", true); // Clear old retained messages

      bool reachable = Ping.ping(targetIP, 3);   // Ping target
      mqttClient.publish(mqtt_topic_status, reachable ? "On" : "Off"); // Publish ping result
      lastActionTime = millis();                  // Update last action time
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());          // Print error code
      Serial.println(" -> retrying...");
      delay(3000);
    }
  }
}

void mqttTask(void* parameter) {
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) WiFi.reconnect(); // Reconnect WiFi

    if (!mqttClient.connected()) connectToMqtt();       // Reconnect MQTT

    mqttClient.loop();                                   // MQTT loop
    vTaskDelay(10 / portTICK_PERIOD_MS);                // Delay for FreeRTOS
  }
}

void IRAM_ATTR gpioISR() {
  unsigned long now = millis();
  if (now - lastInterruptTime > 300) gpioTriggered = true; // Debounce 300ms
}

void enterLightSleep(uint64_t duration_us) {
  esp_sleep_enable_timer_wakeup(duration_us); // Enable wakeup timer
  esp_light_sleep_start();                    // Enter light sleep
}

void setup() {
  Serial.begin(115200);                        // Start serial
  delay(1000);
  boot++;                                      // Increment boot count

  pinMode(LED_PIN, OUTPUT);                    // LED pin
  digitalWrite(LED_PIN, LOW);                  // Turn off LED

  pinMode(SLEEP_GPIO, INPUT_PULLUP);           // Sleep GPIO
  if (digitalRead(SLEEP_GPIO) == 0) {          // If LOW, enable sleep mode
    gpioSleep = true;
    setupWatchdog();                           // Start watchdog
  }

  pinMode(BUTTON_GPIO, INPUT_PULLUP);          // Button GPIO
  attachInterrupt(digitalPinToInterrupt(BUTTON_GPIO), gpioISR, FALLING); // Attach ISR

  WiFi.begin(ssid, password);                  // Connect to WiFi
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (digitalRead(SLEEP_GPIO) == 0) resetWatchdog(); // Feed watchdog while waiting
  }

  Serial.println("\nWiFi connected");
  udp.begin(udpPort);                          // Start UDP

  connectToMqtt();                              // Connect to MQTT

  xTaskCreatePinnedToCore(
    mqttTask,
    "MQTT Task",
    4096,
    NULL,
    1,
    &mqttTaskHandle,
    1
  );

  lastActionTime = millis();                    // Set last action time
  String bootMsg = "Boot #" + String(boot) + " @ " + String(millis()) + "ms"; // Boot log
  mqttClient.publish(mqtt_topic_log, bootMsg.c_str());
}

void loop() {
  resetWatchdog();                              // Reset watchdog

  static unsigned long pressStart = 0;

  if (gpioTriggered) {                          // GPIO triggered
    unsigned long pressTime = millis();
    while(digitalRead(BUTTON_GPIO) == LOW) {
      if (millis() - pressTime > 2000) {       // Pressed > 2s
        digitalWrite(LED_PIN, HIGH);
        lastTriggerReason = "Button GPIO";     // Set reason
        sendMagicPacketBurst(lastTriggerReason.c_str()); // Send packet
        break;
      }
    }
    gpioTriggered = false;
    delay(300);
    digitalWrite(LED_PIN, LOW);
    lastActionTime = millis();
  }

  if (gpioSleep) {                              // Sleep mode active
    if (millis() - lastActionTime > 10000) {   // Wait 10s
      resetWatchdog();
      mqttClient.publish(mqtt_topic_log, "Entering light sleep for 5s");
      Serial.print("Entering light sleep for 5s");
      digitalWrite(LED_PIN, LOW);
      delay(200);
      enterLightSleep(5 * 1000000);            // Sleep 5 seconds
      digitalWrite(LED_PIN, HIGH);
      lastActionTime = millis();
    }
  } else {                                      // Normal mode
    if (millis() - lastActionTime > 60000) {   // Every 1 min
      mqttClient.publish(mqtt_topic_log, "Checking ping every 1 min");
      bool reachable = Ping.ping(targetIP, 3); // Ping target
      mqttClient.publish(mqtt_topic_status, reachable ? "On" : "Off"); // Publish status
      lastActionTime = millis();
    }
  }
}

#include <WiFi.h>                  // Include WiFi library for ESP32
#include <WiFiUdp.h>               // Include UDP support for WiFi
#include <WiFiClientSecure.h>      // Include secure WiFi client for TLS
#include <PubSubClient.h>          // Include MQTT client library
#include <ESP32Ping.h>             // Include library to ping devices
#include <esp_sleep.h>             // Include ESP32 sleep functions

#define SLEEP_GPIO D2              // GPIO for sleep button
#define LED_PIN D1                 // GPIO for status LED
#define BUTTON_GPIO D0             // GPIO for user button
#define WATCHDOG_TIMEOUT 120000   // Watchdog timeout: 2 minutes in ms
#define BUTTON_PIN_BITMASK (1ULL << 2) // Bitmask for GPIO2 (used for wakeup)

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

bool gpioSleep = false;                      // Flag for light sleep mode
RTC_DATA_ATTR int boot = 0;                  // Boot counter stored in RTC memory

WiFiUDP udp;                                 // UDP object
WiFiClientSecure net;                        // Secure WiFi client object
PubSubClient mqttClient(net);                // MQTT client using secure network
hw_timer_t* watchdog = NULL;                 // Hardware watchdog timer
unsigned long lastActionTime = 0;            // Last action timestamp

TaskHandle_t mqttTaskHandle = NULL;          // Task handle for MQTT task

// ---- New variables ----
volatile unsigned long lastInterruptTime = 0; // Last GPIO interrupt timestamp (for debounce)
volatile bool gpioTriggered = false;          // Flag indicating GPIO trigger
String lastTriggerReason = "Boot";            // Reason for last trigger, used for logging

// Reset module immediately
void IRAM_ATTR resetModule() {
  esp_restart();
}

// Reset watchdog timer
void resetWatchdog() {
  timerWrite(watchdog, 0);
}

// Setup hardware watchdog for 5min sleep
void setupWatchdog() {
  watchdog = timerBegin(0, 80, true);                     // Create timer 0, prescaler 80
  timerAttachInterrupt(watchdog, &resetModule, true);    // Attach ISR to reset module
  timerAlarmWrite(watchdog, WATCHDOG_TIMEOUT * 1000, false); // 5min in microseconds
  timerAlarmEnable(watchdog);                             // Enable watchdog
}

// Send WOL Magic Packet burst
void sendMagicPacketBurst(const char* reason) {
  Serial.println("Sending Magic Packets...");
  mqttClient.publish(mqtt_topic_log, "Sending Magic Packets...");

  for (int i = 0; i < 10; i++) {
    uint8_t packet[102];
    memset(packet, 0xFF, 6);           // Start with 6 bytes of 0xFF
    for (int j = 1; j <= 16; j++) {
      memcpy(&packet[j * 6], macAddress, 6); // Append target MAC 16 times
    }

    udp.beginPacket(broadcastIP, udpPort); // Send UDP packet
    udp.write(packet, sizeof(packet));
    udp.endPacket();
    delay(100);                            // Small delay between packets
  }

  mqttClient.publish(mqtt_topic_status, "Magic Packet sent");

  // Log the reason for sending
  String logMsg = "Magic Packet sent - Reason: " + String(reason) + " @ " + String(millis()) + "ms";
  mqttClient.publish(mqtt_topic_log, logMsg.c_str());
}

// MQTT callback for incoming messages
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];       // Convert payload to string
  }

  Serial.print("MQTT received: ");
  Serial.println(message);

  if (message == "TurnOn") {           // If command is TurnOn
    digitalWrite(LED_PIN, HIGH);       // Turn on LED
    Serial.print("MQTT 'TurnOn' received");
    lastTriggerReason = "MQTT TurnOn";
    sendMagicPacketBurst(lastTriggerReason.c_str()); // Send WOL
    digitalWrite(LED_PIN, LOW);        // Turn off LED
    lastActionTime = millis();
  } else if (message == "PingPC") {    // If command is PingPC
    digitalWrite(LED_PIN, HIGH);       // Turn on LED
    Serial.print("MQTT 'PingPC' received");
    lastTriggerReason = "MQTT PingPC";
    bool reachable = Ping.ping(targetIP, 3); // Ping target
    mqttClient.publish(mqtt_topic_status, reachable ? "On" : "Off");
    String logMsg = "Check ping manually - Reason: " + String(lastTriggerReason) + " @ " + String(millis()) + "ms";
    mqttClient.publish(mqtt_topic_log, logMsg.c_str());
    digitalWrite(LED_PIN, LOW);        // Turn off LED
  }
}

// Connect to MQTT broker
void connectToMqtt() {
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);
  net.setInsecure();                   // Ignore SSL certificate (testing only)

  while (!mqttClient.connected()) {    // Retry until connected
    Serial.print("Connecting to MQTT... ");
    if (mqttClient.connect("ESP32Client", mqtt_user, mqtt_password, mqtt_topic_status, 1, false, "Client offline")) {
      Serial.println("connected");
      mqttClient.subscribe(mqtt_topic_event);        // Subscribe to events
      mqttClient.publish(mqtt_topic_log, "MQTT connected");

      mqttClient.publish(mqtt_topic_event, "", true); // Clear old retained messages

      bool reachable = Ping.ping(targetIP, 3);       // Initial ping
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

// MQTT task loop
void mqttTask(void* parameter) {
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.reconnect();            // Reconnect WiFi if disconnected
    }

    if (!mqttClient.connected()) {
      connectToMqtt();             // Reconnect MQTT if disconnected
    }

    mqttClient.loop();             // Process MQTT messages
    vTaskDelay(10 / portTICK_PERIOD_MS); // Short delay
  }
}

// GPIO interrupt for button press
void IRAM_ATTR gpioISR() {
  unsigned long now = millis();
  if (now - lastInterruptTime > 300) {  // Debounce 300ms
    gpioTriggered = true;
  }
}

// Enter light sleep for specified duration
void enterLightSleep(uint64_t duration_us) {
  esp_deep_sleep_enable_gpio_wakeup(1 << 2, ESP_GPIO_WAKEUP_GPIO_LOW); // Wake on GPIO2 low - NEW METHOD
  esp_sleep_enable_timer_wakeup(duration_us);                          // Wake on timer
  esp_light_sleep_start();                                              // Start light sleep
}

void connection(){
  WiFi.begin(ssid, password);         // Connect to WiFi
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (digitalRead(SLEEP_GPIO) == 0) resetWatchdog(); // Reset watchdog during wait
  }
  Serial.println("\nWiFi connected");
  udp.begin(udpPort);                  // Start UDP
  connectToMqtt();                     // Connect to MQTT broker
}

// Setup function
void setup() {
  Serial.begin(115200);       // Start serial
  delay(1000);
  boot++;                     // Increment boot counter

  pinMode(LED_PIN, OUTPUT);   // Setup LED pin
  digitalWrite(LED_PIN, LOW);

  pinMode(SLEEP_GPIO, INPUT_PULLUP); // Setup sleep button with pull-up
  if (digitalRead(SLEEP_GPIO) == 0){
    gpioSleep = true;          // Enable sleep mode if pressed
    setupWatchdog();           // Setup 30min watchdog
  }

  pinMode(BUTTON_GPIO, INPUT_PULLUP); // Setup user button with pull-up
  attachInterrupt(digitalPinToInterrupt(BUTTON_GPIO), gpioISR, FALLING); // Attach ISR

  connection();

  xTaskCreatePinnedToCore(             // Start MQTT task
    mqttTask,
    "MQTT Task",
    4096,
    NULL,
    1,
    &mqttTaskHandle,
    1
  );

  lastActionTime = millis();           // Initialize last action timestamp

  String bootMsg = "Boot #" + String(boot) + " @ " + String(millis()) + "ms";
  mqttClient.publish(mqtt_topic_log, bootMsg.c_str()); // Log boot message
}

// Main loop
void loop() {

  if (gpioTriggered) {                  // If button triggered
      static unsigned long buttonPressStart = 0;
      if (digitalRead(BUTTON_GPIO) == LOW) {  // Button still pressed
          if (buttonPressStart == 0) buttonPressStart = millis();
          else if (millis() - buttonPressStart > 1000) { // Pressed >1s
              lastTriggerReason = "Button GPIO";
              sendMagicPacketBurst(lastTriggerReason.c_str()); // Send WOL
              digitalWrite(LED_PIN, LOW);
              lastActionTime = millis();
              gpioTriggered = false;
              buttonPressStart = 0;
          }
      } else {                          // Button released
          gpioTriggered = false;
          buttonPressStart = 0;
      }
  }

  if (gpioSleep) {                      // If sleep mode enabled
    resetWatchdog();                    // Reset watchdog
    if (millis() - lastActionTime > 10000) { // Stay on 10s before sleep 
      mqttClient.publish(mqtt_topic_log, "Entering light sleep for 1min");
      Serial.print("Entering light sleep for 5min");
      digitalWrite(LED_PIN, LOW);
      delay(200);
      enterLightSleep(60000 * 1000);    // Sleep 1min
      digitalWrite(LED_PIN, HIGH);
      mqttClient.publish(mqtt_topic_log, "Checking ping on WakeUp");
      bool reachable = Ping.ping(targetIP, 3);
      mqttClient.publish(mqtt_topic_status, reachable ? "On" : "Off");
      lastActionTime = millis();
      digitalWrite(LED_PIN, LOW);
    }
  } else {                              // Normal operation
    if (millis() - lastActionTime > 300000) { // Every 5 min
      mqttClient.publish(mqtt_topic_log, "Checking ping every 5 min");
      bool reachable = Ping.ping(targetIP, 3);
      mqttClient.publish(mqtt_topic_status, reachable ? "On" : "Off");
      lastActionTime = millis();
    }
  }
}

#include <WiFi.h>
#include <WiFiUdp.h>

// Wi-Fi network configurations
const char* ssid = "wifi_name";
const char* password = "wifi_pass";

// MAC address of the device to be woken up
const uint8_t macAddress[6] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };

// UDP configurations
WiFiUDP udp;
const char* broadcastIP = "XXX.XXX.XXX.XXX"; // Replace with the broadcast address of the network
const int udpPort = XXXX; // Standard port for Wake-on-LAN

#define BUTTON_PIN_BITMASK 0x200000000 // 2^33 in hex

RTC_DATA_ATTR int bootCount = 0; //save in memory in deep sleep, in case of reset ou poweroff default = 0

// Function to send the Magic Packet
void sendWakeOnLanPacket() {
  uint8_t packet[102];
  memset(packet, 0xFF, 6); // Fill the first 6 bytes with 0xFF
  for (int i = 1; i <= 16; i++) {
    memcpy(&packet[i * 6], macAddress, 6); // Fill with the MAC address
  }

  udp.beginPacket(broadcastIP, udpPort);
  udp.write(packet, sizeof(packet));
  udp.endPacket();

  Serial.println("Magic Packet sent.");
}

// Connect to Wi-Fi
void connectWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting to Wi-Fi...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(100);
      Serial.print(".");
    }
    Serial.println("\nWi-Fi connected.");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    udp.begin(udpPort); // Reconfigure UDP after reconnection
  }
}

void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

void setup(){
  Serial.begin(115200);
  delay(1000); //Take some time to open up the Serial Monitor

  Serial.println("Boot number: " + String(bootCount));

  //Print the wakeup reason for ESP32
  print_wakeup_reason();

  esp_deep_sleep_enable_gpio_wakeup(BIT(D0), ESP_GPIO_WAKEUP_GPIO_LOW); //gpio2 D0

  connectWiFi();

  for (int i = 1; i <= 10; i++) {
    Serial.print(", ");
    Serial.println(i);
    delay(1000);
    //send 10x if is not reset board, only wakeup status
    if(bootCount != 0) sendWakeOnLanPacket();
  }
  
  //Increment boot number and print it every reboot
  ++bootCount;
  
  //Go to sleep now
  Serial.println(" ");
  Serial.println("Going to sleep now");
  esp_deep_sleep_start();
}

void loop(){
  //This is not going to be called
}

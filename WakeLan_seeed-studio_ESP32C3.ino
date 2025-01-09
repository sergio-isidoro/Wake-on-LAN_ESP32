#include <WiFi.h>
#include <WiFiUdp.h>

// Wi-Fi network configurations
const char* ssid = "XXXX";
const char* password = "XXXX";

// MAC address of the device to be woken up
const uint8_t macAddress[6] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };

// UDP configurations
WiFiUDP udp;
const char* broadcastIP = "XXX.XXX.XXX.XXX"; // Broadcast address
const int udpPort = XXXX;

// Button GPIO
const int interruptPin = GPIO_NUM_2; // GPIO2 (D0)

// Flag to signal the wake event
volatile bool wake = false;

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
}

// Interrupt function
void IRAM_ATTR handleInterrupt() {
  wake = true; // Signal the wake event
}

// Connect to Wi-Fi
void connectWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(100);
    }
    udp.begin(udpPort); // Reconfigure UDP after reconnection
  }
}

void setup() {
  // Configure the interrupt pin
  pinMode(interruptPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptPin), handleInterrupt, FALLING);

  // Initialize Wi-Fi to be ready at startup
  connectWiFi();
}

void loop() {
  if (wake) {
    wake = false; // Reset the wake flag

    // Send the Magic Packet
    sendWakeOnLanPacket();
  }

  // Add other logic here if necessary
  delay(10); // Small delay to avoid an overly fast loop
}

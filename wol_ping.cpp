/*
 * wol_ping.cpp
 * -------------------------------
 * Implements Wake-on-LAN and ping monitoring:
 *  - Builds and sends the magic packet to broadcast address
 *  - Uses ESP32Ping to check if the PC is online
 *  - Button press can trigger WOL
 *  - After sending WOL, performs a delayed ping
 */

#include "wol_ping.h"
#include "helpers.h"
#include "config.h"
#include <ESP32Ping.h>

void sendWOL(const char* reason, int n) {
    uint8_t magic_packet[102];

    // 1. Construir o Magic Packet uma vez
    memset(magic_packet, 0xFF, 6);
    for(int j = 0; j < 16; j++) {
        memcpy(&magic_packet[6 + j * 6], config.mac_address, 6);
    }

    // --- Send by Wi-Fi ---
    IPAddress wifi_bcast;
    if(wifi_bcast.fromString(config.broadcastIPStr)) {
        for(int i = 0; i < n; i++) {
            udp.beginPacket(wifi_bcast, config.udp_port);
            udp.write(magic_packet, sizeof(magic_packet));
            udp.endPacket();
        }
        mqttPublish(("WOL sent (Wi-Fi) - " + String(reason)).c_str());
        Serial.println("WOL sent (Wi-Fi)");
    } else {
        Serial.println("Broadcast IP (Wi-Fi) error");
    }

    // --- Send by Ethernet (Offline) ---
    EthernetUDP ethUdp;
    ethUdp.begin(config.udp_port);
    
    IPAddress eth_bcast(192, 168, 5, 255); 

    for(int i = 0; i < n; i++) {
        ethUdp.beginPacket(eth_bcast, config.udp_port);
        ethUdp.write(magic_packet, sizeof(magic_packet));
        ethUdp.endPacket();
    }
    mqttPublish(("WOL sent (LAN) - " + String(reason)).c_str());
    Serial.println("WOL sent (LAN)");
    ethUdp.stop();

    wolSentAt = millis();
    wolPendingPing = true;
}


void sendShutdownPacket(const char* reason, int n) {
    uint8_t packet[102];

    for(int i = 0; i < n; i++) {
        memset(packet, 0xEE, 6); 
        for(int j = 0; j < 16; j++) {
            memcpy(&packet[6+j*6], config.mac_address, 6);
        }

        IPAddress bcast;
        if(!bcast.fromString(config.broadcastIPStr)) {
            Serial.println("Broadcast IP error");
            return;
        }

        udp.beginPacket(bcast, config.udp_port);
        udp.write(packet, sizeof(packet));
        udp.endPacket();
    }

    mqttPublish(("Shutdown Packet sent (" + String(reason) + ")").c_str());
    Serial.println("Shutdown Packet sent " + String(reason));

    wolSentAt = millis();
    wolPendingPing = true;
}

void doPing(){
  IPAddress target; 
  target.fromString(config.target_ip);
  bool ok = Ping.ping(target,3);
  if(ok){
    mqttPublish("Ping: PC online");
  } else {
    mqttPublish("Ping: PC offline");
  }  
}

void handleButton(){
  static unsigned long t0 = 0;
  if(digitalRead(BUTTON_GPIO) == LOW){
    if(!buttonTriggered){ 
      buttonTriggered = true; 
      t0=millis(); 
    }
    else if(millis() - t0 > 1000){ 
      blinkDigit(1);
      sendWOL("Button", 10); 
      buttonTriggered = false;
    }
  } else buttonTriggered = false;
}

void handleScheduledPing(){
  if(wolPendingPing && millis() - wolSentAt >= PING_DELAY_AFTER_WOL){
    doPing();
    wolPendingPing = false;
  }
}

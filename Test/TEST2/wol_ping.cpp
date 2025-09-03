#include "wol_ping.h"
#include "helpers.h"
#include "config.h"
#include <ESP32Ping.h>

void sendWOL(const char* reason) {
  uint8_t packet[102]; memset(packet, 0xFF, 6);
  for (int i=0;i<16;i++) memcpy(&packet[6+i*6], config.mac_address, 6);

  IPAddress bcast; bcast.fromString(config.broadcastIPStr);
  udp.beginPacket(bcast, config.udp_port);
  udp.write(packet, sizeof(packet));
  udp.endPacket();

  mqttPublish(("WOL sent (" + String(reason) + ")").c_str());
  wolSentAt = millis(); wolPendingPing = true;
}

void doPing() {
  IPAddress target; target.fromString(config.target_ip);
  bool ok = Ping.ping(target, 3);
  if (ok) mqttPublish("Ping OK: PC online");
  else    mqttPublish("Ping FAIL: PC offline");
}

void handleButton() {
  static unsigned long t0 = 0;
  if (digitalRead(BUTTON_GPIO) == LOW) {
    if (!buttonTriggered) { buttonTriggered = true; t0 = millis(); }
    else if (millis()-t0 > 1000) { sendWOL("Button"); buttonTriggered=false; }
  } else buttonTriggered=false;
}

void handleScheduledPing() {
  if (wolPendingPing && millis()-wolSentAt >= PING_DELAY_AFTER_WOL) {
    doPing();
    wolPendingPing = false;
  }
}

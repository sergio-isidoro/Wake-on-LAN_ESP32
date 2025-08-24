# ⚡ ESP32 Wake-on-LAN + MQTT (Light Sleep + Watchdog + Logging) -> v3.1

A complete **ESP32-based project** for sending **Wake-on-LAN (WOL) Magic Packets** over Wi-Fi, with MQTT integration, hardware watchdog, and light sleep support.

Includes:
- **Light sleep mode for energy efficiency**
- **Ping-based device status**
- **Hardware watchdog timer**
- **Button-triggered wake**
- **Online/offline MQTT reporting**

> 🔋 Optimized for low power using `esp_light_sleep_start()` while keeping Wi-Fi/MQTT active, with hardware watchdog ensuring recovery in case of hangs.

---

## 📋 Features

- 🌐 **Wi-Fi Integration**: Connects to your local Wi-Fi network.
- 🖥️ **Wake-on-LAN**: Sends standard WOL Magic Packets to wake compatible devices.
- 🔘 **Hardware Button Wake**: GPIO0 button triggers Magic Packet burst.
- ☁️ **MQTT Support**: Receives `"TurnOn"` command and reports device status via topics.
- 🔄 **Magic Packet Burst**: Sends 10 packets at 100ms intervals for reliability.
- 💤 **Light Sleep Mode**: Reduces power consumption using `esp_light_sleep_start()`.
- 🧠 **Ping-based Status Check**: Uses `ESP32Ping` to verify if the target is online.
- 🐶 **Watchdog Timer**: Hardware watchdog resets ESP32 if system hangs.
- 🔆 **LED Indicator**: GPIO1 shows WOL activity.
- 🔌 **Sleep Control Pin**: GPIO2 enables/disables light sleep mode.

---

## 🛠️ Requirements

- ESP32 board (WROOM, C3, etc.)
- Target device with **Wake-on-LAN enabled**
- MQTT broker (e.g., Mosquitto, HiveMQ)
- Button connected to **GPIO0** (D0)
- LED connected to **GPIO1** (D1)
- Sleep control input on **GPIO2** (D2)
- Libraries used:
  - `WiFi.h`
  - `WiFiClientSecure.h`
  - `WiFiUDP.h`
  - `PubSubClient.h`
  - `ESP32Ping.h`

---

## 📡 MQTT Topics

- `wol/event` – Subscribe to receive `"TurnOn"` commands.
- `wol/status` – Publishes `"On"` or `"Off"` based on ping.
- `wol/log` – Publishes debug logs (e.g., Magic Packet sent, entering sleep).
- 
---

## 🧪 Behavior

1. Connects to Wi-Fi and MQTT broker.
2. Publishes initial status (based on ping).
3. Available triggers:
   - **Button (GPIO0)** → Sends Magic Packets.
   - **MQTT (`wol/event: TurnOn`)** → Sends Magic Packets.
4. After WOL:
   - LED lights up during transmission.
   - Ping result is published to `wol/status`.
   - Trigger reason is logged to `wol/log`.
5. **Light Sleep Mode**:
   - Enabled if GPIO2 is LOW.
   - Enters light sleep after 10s of inactivity.
   - Wakes up automatically after 5s or by event.
6. **Watchdog Timer**:
   - Hardware watchdog resets ESP32 if unresponsive.
   - Periodically refreshed in main loop.
7. If sleep mode is **disabled**, pings target every 60s.

---

## ⚙️ Pinout

| Function            | GPIO  |
|--------------------|-------|
| LED Indicator       | D1    |
| Button Input        | D0    |
| Sleep Mode Control  | D2    |

---

## ⚙️ Configuration

Update these in the code before uploading:
```cpp
const char* ssid = "???";
const char* password = "???";

const char* mqtt_server = "???";
const int   mqtt_port = ????;
const char* mqtt_user = "???";
const char* mqtt_password = "???";

const IPAddress targetIP(192, 168, ???, ???);   // Target PC IP
const char* broadcastIP = "192.168.???.255";    // Network broadcast
const uint8_t macAddress[6] = {0x??,0x??,0x??,0x??,0x??,0x??}; // Target PC MAC
````

---

## 📈 Notes

- Button must be held for >2 seconds to trigger Magic Packet.
- Watchdog timeout is set to 15 seconds.
- Light sleep interval is 5 seconds; can be adjusted in enterLightSleep().

# ✨ Thanks for checking out this project!

## ⚡Image

![SHEMATIC](https://github.com/user-attachments/assets/69b907f5-264b-4f98-b777-c53e9436570a)
![PINOUT](https://github.com/user-attachments/assets/5ac26256-06c6-40ae-ab29-bd35d11dfe80)

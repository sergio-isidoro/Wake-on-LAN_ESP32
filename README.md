# ⚡ ESP32 Wake-on-LAN + MQTT (Light Sleep + Watchdog + Logging) -> V3

A complete **ESP32-based project** for sending **Wake-on-LAN (WOL) Magic Packets** over Wi-Fi, with MQTT integration, hardware watchdog, and light sleep support.

Includes:
- **Remote control via MQTT**
- **Physical button trigger**
- **Light sleep mode with watchdog refresh**
- **Target status monitoring via ping**
- **Detailed logging via MQTT**
- **Visual feedback with LED**

> 🔋 Optimized for low power using `esp_light_sleep_start()` while keeping Wi-Fi/MQTT active, with hardware watchdog ensuring recovery in case of hangs.

---

## 📋 Features

- 🌐 **Wi-Fi**: Auto-connects to your local network.
- 🖥️ **Wake-on-LAN**: Sends standard Magic Packets.
- 🔘 **Physical Button**: GPIO0 (D0) triggers Magic Packets.
- ☁️ **MQTT**:
  - Listens for `"TurnOn"` on `wol/event`.
  - Publishes device status (`On`/`Off`) to `wol/status`.
  - Publishes logs and events to `wol/log`.
- 🔄 **Burst Packets**: 10 packets sent every 100 ms for reliability.
- 💤 **Light Sleep + Watchdog**:
  - Enabled when GPIO2 (D2) is pulled LOW.
  - Hardware watchdog resets ESP32 if it hangs.
- 🧠 **Ping Monitoring**:
  - Runs after WOL events to confirm target is online.
  - Periodic check every 60s when active.
- 🔆 **LED Indicator**: GPIO1 (D1) lights up during WOL events.
- 📝 **Advanced Logging**:
  - Includes trigger reason (Boot, Button, MQTT).
  - Timestamp (ms) for each action.

---

## 🛠️ Requirements

- **ESP32 board** (WROOM, DevKit, C3, etc.)
- Target device with **Wake-on-LAN enabled**
- **MQTT broker** (Mosquitto, HiveMQ, etc.)
- Hardware connections:
  - **GPIO0 (D0)** → Button (with internal pull-up)
  - **GPIO2 (D2)** → Sleep mode control (LOW = enabled)
  - **GPIO1 (D1)** → LED indicator
- Required libraries:
  - `WiFi.h`
  - `WiFiUDP.h`
  - `WiFiClientSecure.h`
  - `PubSubClient.h`
  - `ESP32Ping.h`

---

## 📡 MQTT Topics

- `wol/event` → Receives `"TurnOn"`
- `wol/status` → Publishes `"On"` or `"Off"`
- `wol/log` → Publishes debug messages and events (boot, WOL, sleep, ping, etc.)

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

## ⚙️ Configuration

Update these in the code before uploading:
```cpp
const char* ssid = "...";
const char* password = "...";

const char* mqtt_server = "...";
const int   mqtt_port = ...;
const char* mqtt_user = "...";
const char* mqtt_password = "...";

const IPAddress targetIP(192, 168, 1, 100);   // Target PC IP
const char* broadcastIP = "192.168.1.255";    // Network broadcast
const uint8_t macAddress[6] = {0xAA,0xBB,0xCC,0xDD,0xEE,0xFF}; // Target PC MAC
````

---

## 🚀 Notes

- Watchdog timeout: 15s (WATCHDOG_TIMEOUT).
- Button debounce: 200 ms.
- WOL burst: 10 UDP packets on port 9.
- SSL certificate check is disabled (net.setInsecure() → testing only).

# ✨ Thanks for checking out this project!

## ⚡Image

![SHEMATIC](https://github.com/user-attachments/assets/69b907f5-264b-4f98-b777-c53e9436570a)
![PINOUT](https://github.com/user-attachments/assets/5ac26256-06c6-40ae-ab29-bd35d11dfe80)

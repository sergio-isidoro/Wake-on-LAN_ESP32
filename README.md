# 🔥 Wake-on-LAN ESP32 + MQTT (Light Sleep & Watchdog) v3.2

An advanced ESP32-based project for sending **Wake-on-LAN (WOL) Magic Packets** over Wi-Fi, with full MQTT support, optional light sleep mode, and a hardware watchdog timer.

This version distinguishes operation **with sleep** and **without sleep**, including GPIO button-triggered wake-up.

---

## 📋 Features

- 🌐 **Wi-Fi Integration**: Connects securely to your local Wi-Fi network.
- 🖥️ **Wake-on-LAN**: Sends standard WOL Magic Packets to wake compatible PCs.
- 🔘 **User Button Wake**: GPIO0 button triggers WOL; requires ~1 second press to activate.
- ☁️ **MQTT Support**: Subscribes to `wol/event` for commands (`TurnOn`, `PingPC`) and publishes logs/status.
- 🔄 **Magic Packet Burst**: Sends 10 packets at 100ms intervals for reliability.
- 🧠 **Ping-based Status Check**: Uses `ESP32Ping` to check if the target device is online.
- 🐶 **Hardware Watchdog Timer**: Ensures the ESP32 resets if it becomes unresponsive.
- 🔆 **LED Indicator**: GPIO1 LED indicates activity (WOL sent, ping, sleep).
- 💤 **Optional Light Sleep Mode**: Controlled via GPIO2 (`SLEEP_GPIO`) to reduce power consumption.
- ⏱️ **Boot Counter**: Keeps track of boot events in RTC memory.
- 🕒 **Periodic Ping**: Without sleep, pings target every 5 minutes; with sleep, pings on wake-up.

---

## 🛠️ Requirements

- **ESP32 board** (WROOM, C3, etc.).
- Target PC with **Wake-on-LAN enabled**.
- **MQTT broker** (e.g., HiveMQ, Mosquitto) with credentials.
- **Button** connected to **GPIO0 (D0)** for manual WOL.
- **Sleep control input** connected to **GPIO2 (D2)**.
- **LED** connected to **GPIO1 (D1)**.
- Libraries:
  - `WiFi.h`
  - `WiFiClientSecure.h`
  - `WiFiUDP.h`
  - `PubSubClient.h`
  - `ESP32Ping.h`
  - `esp_sleep.h`

---

## 📡 MQTT Topics

| Topic           | Purpose                                       |
|-----------------|-----------------------------------------------|
| `wol/event`     | Subscribe to receive `"TurnOn"` or `"PingPC"` |
| `wol/status`    | Publishes `"On"` or `"Off"` based on ping     |
| `wol/log`       | Publishes detailed logs (boot, sleep, WOL)   |

---

## ⚡ Operation Modes

### 1️⃣ Without Light Sleep
- ESP32 remains fully powered.
- Every 5 minutes, the device pings the target IP and updates MQTT status.
- Button on GPIO0 triggers WOL if pressed for **>1 second**.
- LED indicates active operations.

### 2️⃣ With Light Sleep (via GPIO2)
- Sleep mode enabled by holding GPIO2 low on boot.
- Hardware watchdog ensures the ESP32 resets if stuck.
- After **10 seconds of inactivity**, ESP32 enters **light sleep for 1 minute**.
- On wake-up:
  - LED briefly indicates wake.
  - Target IP is pinged and MQTT status updated.
- Button on GPIO0 works the same: press >1 second to send Magic Packets.

---

## 🧪 Detailed Behavior

1. **Boot**
   - Connects to Wi-Fi.
   - Connects to MQTT broker.
   - Publishes boot log (`wol/log`) and status (`wol/status`).

2. **Button Press (GPIO0)**
   - Requires ~1 second press.
   - Sends 10 WOL Magic Packets to the target MAC.
   - Updates MQTT logs and status.
   - LED flashes during WOL burst.

3. **MQTT Commands**
   - `"TurnOn"`: Sends WOL Magic Packets.
   - `"PingPC"`: Pings target IP and publishes `"On"`/`"Off"`.

4. **Sleep Mode (if enabled)**
   - GPIO2 low at boot enables sleep.
   - Light sleep reduces power consumption while preserving Wi-Fi/MQTT connections.
   - Wake-up triggered by timer or GPIO2.
   - On wake, ping target and update MQTT.

5. **Watchdog**
   - Hardware timer triggers ESP32 reset if inactive for 2 minutes.
   - Ensures reliability during Wi-Fi or MQTT failures.

---

## ⚙️ Configuration

Update these in the code before uploading:
```cpp
const char* ssid = "???";                                      // WiFi SSID
const char* password = "???";                                  // WiFi password

const char* mqtt_server = "???";                               // MQTT broker address
const int mqtt_port = ????;                                    // MQTT broker port
const char* mqtt_user = "???";                                 // MQTT username
const char* mqtt_password = "???";                             // MQTT password
const char* mqtt_topic_status = "wol/status";                  // MQTT topic for status
const char* mqtt_topic_event  = "wol/event";                   // MQTT topic for events
const char* mqtt_topic_log    = "wol/log";                     // MQTT topic for logs

const IPAddress targetIP(192, 168, ???, ???);                  // Target device IP for ping / magic packet
const char* broadcastIP = "192.168.???.255";                   // Broadcast IP for Magic Packet
const int udpPort = ????;                                      // UDP port for Magic Packet
const uint8_t macAddress[6] = {0x??,0x??,0x??,0x??,0x??,0x??}; // Target MAC address
````

---

## 🛠️ Pinout Summary

| Pin      | Function                         |
|----------|---------------------------------|
| D0       | User button (send WOL)          |
| D1       | LED indicator                   |
| D2       | Sleep mode input / wake-up      |

---

## 🚀 Notes

- **Debounce:** GPIO button press is debounced (300ms) and requires >1 second press to trigger WOL.
- **Magic Packet:** Broadcast UDP to `192.168.2.255:9` using target MAC.
- **MQTT TLS:** `WiFiClientSecure` with insecure certificate (for testing). Update for production.
- **Light Sleep Timing:** Adjustable by modifying `enterLightSleep()` duration.

# ✨ Thanks for checking out this project!

## ⚡Image

![SHEMATIC](https://github.com/user-attachments/assets/69b907f5-264b-4f98-b777-c53e9436570a)
![PINOUT](https://github.com/user-attachments/assets/5ac26256-06c6-40ae-ab29-bd35d11dfe80)

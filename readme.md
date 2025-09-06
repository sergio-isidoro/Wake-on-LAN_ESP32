# 🔥 Wake-on-LAN ESP32 + MQTT + OTA + Portal (Wi-Fi) v5.4

Advanced ESP32 project for sending **Wake-on-LAN (WOL) Magic Packets** over Wi-Fi, with full MQTT support, OTA updates, ping-based status checks, and a configuration portal hosted on the device using SPIFFS.

This project supports **hardware button-triggered WOL**, scheduled ping after WOL, OTA updates with MQTT progress reporting, optional factory reset, and the ability to **wake up the PC and check its ping from anywhere in the world via MQTT**.

---

## 📋 Features

- 🌐 **Wi-Fi Integration**: Connects to your local Wi-Fi network.
- 🖥️ **Wake-on-LAN (WOL)**: Sends **n** magic packets to wake compatible PCs **(n = 10)**.
- 🔘 **User Button WOL**: D0 button sends WOL on >1s press.
- 🔘 **User command PinOut 1**: D4 output LOW or HIGH (Default LOW).
- 🔘 **User command PinOut 2**: D5 output LOW or HIGH (Default LOW).
- ☁️ **MQTT Support**: Subscribes to `wol/event` for `"TurnOn"`, `"CheckUpdate"`, `"PingPC"`, `"PinOut1On"`, `"PinOut1Off"`, `"PinOut2On"` or `"PinOut2Off"` commands and publishes logs/status to `wol/log` and `wol/status`.
- 🔄 **Automatic Ping After WOL**: Schedules a ping 2 minutes after sending WOL (non-blocking).
- 🕵️ **Ping-based Status Check**: Uses `ESP32Ping` to verify if the target device is online.
- 🔆 **LED Indicator**: D1 LED flashes to indicate WOL, ping, or OTA progress.
- 💾 **OTA Updates**: Checks for firmware every hour; publishes progress to MQTT every 10%.
- 🛠️ **Factory Reset**: Holding D2 button LOW at boot deletes `config.json`.
- 📄 **Configuration Portal**: Hosts HTML page on SPIFFS to configure Wi-Fi, MQTT, target IP/MAC, and UDP port.


👉 **Important:** OTA updates only replace the firmware.  
- The configuration stored in **`/config.json`** (set via the WiFi/MQTT portal) is **preserved** and not modified by the update.  

---

## ✅ The code is compatible with:

- **ESP32-C3** (e.g., **Seeed Studio XIAO ESP32-C3 [Tested]**, DevKitM-1, Lolin C3 Mini) 
- **ESP32-S3** (DevKit, Seeed Studio XIAO S3, AiThinker modules, etc.)
- **ESP32 original** (ESP32-WROOM-32, ESP32-WROVER, DevKit V1)
- **ESP32-S2** (DevKit, XIAO ESP32-S2, etc.)
- **ESP32-PICO-D4** (with 4 MB flash)

---

## 🛠️ Requirements

### 1. Prerequisites

Before starting, ensure you have all the tools and libraries correctly installed:

1. **Arduino IDE**
   - Version 1.8.18 recommended ( >2.0 Upload ESP32 DATA don't work).

2. **ESP32 Board Manager**
   - Add URL: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - Install **ESP32 Boards** version 3.3.0 or higher.

3. **Libraries**
   - **ArduinoJson v7.4.2**
   - **PubSubClient**
   - **ESP32Ping**

4. **Internet Connection**
   - Required for OTA updates and MQTT.

5. **MQTT Broker**
   - Example: Mosquitto, HiveMQ.
   - Note broker IP, port, username, and password.

### 2. ESP32 Board & Partition Configuration

1. **Board**: `XIAO-ESP32C3` (Seeed Studio)  
2. **Partition Scheme**: Default 4MB (supports OTA)  
3. **Upload Speed**: 921600 (optional)  
4. **Flash Size**: 4MB  
5. **SPIFFS / LittleFS**
   - Store `config.json` and `setup.html`
   - Upload via **ESP32 Sketch Data Upload** plugin.
    - The **`data`** folder is used to store files that will be uploaded to the **SPIFFS** filesystem on the ESP32.
    - Install the **ESP32 Sketch Data Upload** plugin for Arduino IDE if not already installed:
        - Follow instructions: https://randomnerdtutorials.com/install-esp32-filesystem-uploader-arduino-ide/

✅ After these steps, your ESP32 is ready for OTA, SPIFFS, JSON configuration, WOL, and MQTT.

---

## 📡 MQTT Topics

| Topic        | Purpose                                         |
|-------------|-------------------------------------------------|
| `wol/event` | Subscribe to `"TurnOn"`, `"CheckUpdate"`, `"PingPC"`, `"PinOut1On"`, `"PinOut1Off"`, `"PinOut2On"` or `"PinOut2Off"` commands |
| `wol/status`| Publishes `"MQTT Ready"`, firmware version, and status messages |
| `wol/log`   | Publishes detailed logs (boot, WOL, ping, OTA)|

---

## ⚡ Operation Modes

### 1️⃣ Button-triggered WOL
- Press Button D0 (>1s) to send WOL magic packet.
- LED flashes during WOL.
- Schedules a ping 2 minutes later to check if PC is online.
- PinOut 1 and PinOut 2 MQTT commands for custom config.

### 2️⃣ MQTT Commands
- LED flashes during Commands.
- `"TurnOn"`: Sends WOL immediately.
- `"CheckUpdate"`: Manually trigger an OTA update check.
- `"PingPC"`: Pings the target and publishes online/offline status.
- `"PinOut1On"`: Command for D4 output HIGH.
- `"PinOut1Off"`: Command for D4 output LOW.
- `"PinOut2On"`: Command for D5 output HIGH.
- `"PinOut2Off"`: Command for D5 output LOW.

### 3️⃣ OTA Updates
- Checks every **1 hour** or Press Button D2 (only after boot) for new firmware (`version.txt`) on GitHub.
- Downloads and flashes firmware directly to OTA partition.
- Publishes progress via MQTT every 10%.
- Reports:
  - Download progress
  - Flash writing
  - Update success or errors
- Device restarts automatically after OTA.

### 4️⃣ Factory Reset
- Hold Button D2 LOW at boot to delete `config.json`.
- Device restarts and launches the configuration portal if no config exists.

### 5️⃣ Configuration Portal
- LED stay **fixed ON**
- Hotspot: `WOL_ESP32_Config` if no config file.
- HTML page allows:
  - Wi-Fi SSID & password
  - MQTT server, port, user, password
  - Target IP & Broadcast IP
  - MAC address for WOL
  - UDP port
- Submitting saves config and restarts device.

---

## ⚙️ Configuration

Portal via browser **(http://192.168.4.1)**.

Or update by ESP32DATA `config.json`:

```json
{
  "ssid": "YOUR_WIFI",
  "password": "YOUR_PASSWORD",
  "mqtt_server": "broker.local",
  "mqtt_port": 1883,
  "mqtt_user": "user",
  "mqtt_password": "pass",
  "target_ip": "192.168.1.100",
  "broadcastIP": "192.168.1.255",
  "mac_address": [0xDE,0xAD,0xBE,0xEF,0xFE,0xED],
  "udp_port": 9
}
```

---

## 🛠️ Pinout Summary

| Pin  | Function                          |
|------|----------------------------------|
| D0   | User button (send WOL)            |
| D1   | LED indicator                     |
| D2   | Factory reset / config portal     |
| D4   | Output LOW or HIGH (MQTT)         |
| D5   | Output LOW or HIGH (MQTT)         |

---

## 💡 Notes

- Button debounce: >1s press triggers WOL.
- Magic Packet: Broadcast UDP to broadcastIP:udp_port using target MAC.
- MQTT Logs: Full OTA, WOL, and ping progress published to wol/log.
- SPIFFS HTML: setup.html must be uploaded via Arduino IDE or ESP32FS tool.
- Firmware Version: Stored in FIRMWARE_VERSION constant (5.2); OTA compares with version.txt.

---

# ✨ Thanks for using this project!
- Developed for ESP32-C3 with Seeed Studio XIAO board.
- Integrates WOL, MQTT, OTA, and a Wi-Fi configuration portal.
- Ideal for automating PC wake-up and monitoring over Wi-Fi.

## ⚡Image

<div align="center">

### Pinout
<div align="center"><img src="https://github.com/user-attachments/assets/5ac26256-06c6-40ae-ab29-bd35d11dfe80" alt="PINOUT" /></div>
<br>

### Schematic
<div align="center"><img width="1005" height="732" alt="Captura de tela 2025-09-06 022846" src="https://github.com/user-attachments/assets/70a35082-1443-414f-a9de-7901313f4b07" alt="SCHEMATIC" /></div>
<br>

### Wi-Fi AP
<div align="center"><img width="230" height="42" src="https://github.com/user-attachments/assets/6a31179c-f777-4ec5-8c40-fe7e1d493d08" alt="Wi-Fi AP" /></div>
<br>

### Configuration Portal
<div align="center"><img width="1014" height="954" src="https://github.com/user-attachments/assets/f758a0f1-685b-4d10-8cdd-0a98d9a8f501" alt="Configuration Portal" /></div>
<br>

### Configuration Portal - SAVED!
<div align="center"><img width="218" height="24" src="https://github.com/user-attachments/assets/c4edd568-4199-45e3-b0d0-dcecbf7f3bd9" alt="Portal Saved" /></div>
<br>

### Console HiveMQ Cloud (can be other)
<div align="center"><img width="600" height="361" src="https://github.com/user-attachments/assets/a818f4de-7b05-4c5f-ae86-9de40ca9604e" alt="HiveMQ Console" /></div>
<br>

### IoT MQTT Panel (can be other)
<div align="center"><img width="824" height="280" src="https://github.com/user-attachments/assets/1b963f7b-a4e0-46c9-835a-5fe4d8f9a40c" alt="IoT MQTT Panel" /></div>
<br>

### IoT MQTT Panel - Dash (Customizable - send: wol/event - receive: wol/status and wol/log)
<div align="center"><img width="600" height="700" src="https://github.com/user-attachments/assets/6c4d2a4a-0736-497f-8eae-c5382d370452" alt="IoT MQTT Dashboard" /></div>
<br>


# 🔥 Wake-on-LAN ESP32-C3 + MQTT + OTA + Portal Wi-Fi v5.2

Advanced ESP32-C3 project for sending **Wake-on-LAN (WOL) Magic Packets** over Wi-Fi, with full MQTT support, OTA updates, ping-based status checks, and a configuration portal hosted on the device using SPIFFS.

This project supports **hardware button-triggered WOL**, scheduled ping after WOL, OTA updates with MQTT progress reporting, and optional factory reset.

---

## 📋 Features

- 🌐 **Wi-Fi Integration**: Connects to your local Wi-Fi network.
- 🖥️ **Wake-on-LAN (WOL)**: Sends magic packets to wake compatible PCs.
- 🔘 **User Button**: GPIO0 (D0) button sends WOL on >1s press.
- ☁️ **MQTT Support**: Subscribes to `wol/event` for `"TurnOn"` or `"PingPC"` commands and publishes logs/status to `wol/log` and `wol/status`.
- 🔄 **Automatic Ping After WOL**: Schedules a ping 2 minutes after sending WOL (non-blocking).
- 🕵️ **Ping-based Status Check**: Uses `ESP32Ping` to verify if the target device is online.
- 🔆 **LED Indicator**: GPIO1 (D1) flashes to indicate WOL, ping, or OTA progress.
- 💾 **OTA Updates**: Checks for firmware every hour; publishes progress to MQTT every 10%.
- 🛠️ **Factory Reset**: Holding GPIO2 (D2) LOW at boot deletes `config.json`.
- 📄 **Configuration Portal**: Hosts HTML page on SPIFFS to configure Wi-Fi, MQTT, target IP/MAC, and UDP port.

---

## 🛠️ Requirements

### 1. Prerequisites

Before starting, ensure you have all the tools and libraries correctly installed:

1. **Arduino IDE**
   - Version 1.8.18 recommended.

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
| `wol/event` | Subscribe to `"TurnOn"` or `"PingPC"` commands |
| `wol/status`| Publishes `"MQTT Ready"`, firmware version, and status messages |
| `wol/log`   | Publishes detailed logs (boot, WOL, ping, OTA)|

---

## ⚡ Operation Modes

### 1️⃣ Button-triggered WOL
- Press GPIO0 (>1s) to send WOL magic packet.
- LED flashes during WOL.
- Schedules a ping 2 minutes later to check if PC is online.

### 2️⃣ MQTT Commands
- `"TurnOn"`: Sends WOL immediately.
- `"PingPC"`: Pings the target and publishes online/offline status.

### 3️⃣ OTA Updates
- Checks every **1 hour** for new firmware (`version.txt`) on GitHub.
- Downloads and flashes firmware directly to OTA partition.
- Publishes progress via MQTT every 10%.
- Reports:
  - Download progress
  - Flash writing
  - Update success or errors
- Device restarts automatically after OTA.

### 4️⃣ Factory Reset
- Hold GPIO2 (D2) LOW at boot to delete `config.json`.
- Device restarts and launches the configuration portal if no config exists.

### 5️⃣ Configuration Portal
- Hotspot: `ESP32C3_Config` if no config file.
- HTML page allows:
  - Wi-Fi SSID & password
  - MQTT server, port, user, password
  - Target IP & Broadcast IP
  - MAC address for WOL
  - UDP port
- Submitting saves config and restarts device.

---

## ⚙️ Configuration

Portal via browser (http://192.168.4.1).

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

<p>1. Pinout</p>
<img src="https://github.com/user-attachments/assets/5ac26256-06c6-40ae-ab29-bd35d11dfe80" alt="PINOUT" />
<br>
<br>
<p>2. Schematic</p>
<img src="https://github.com/user-attachments/assets/69b907f5-264b-4f98-b777-c53e9436570a" alt="SCHEMATIC" />
<br>
<br>
<p>3. Wi-Fi AP</p>
<img width="230" height="42" src="https://github.com/user-attachments/assets/6a31179c-f777-4ec5-8c40-fe7e1d493d08" alt="Screenshot 075647" />
<br>
<br>
<p>4. Configuration Portal</p>
<img width="1014" height="954" src="https://github.com/user-attachments/assets/f758a0f1-685b-4d10-8cdd-0a98d9a8f501" alt="Screenshot 201030" />
<br>
<br>
<p>5. Configuration Portal - SAVED!</p>
<img width="218" height="24" src="https://github.com/user-attachments/assets/c4edd568-4199-45e3-b0d0-dcecbf7f3bd9" alt="Screenshot 195423" />
<br>
<br>
<p>6. Console Hivemq Cloud (can be other)</p>
<img width="600" height="361" src="https://github.com/user-attachments/assets/a818f4de-7b05-4c5f-ae86-9de40ca9604e" alt="Screenshot 213951" />
<br>
<br>
<p>7. IoT MQTT Panel (can be other)</p>
<img width="824" height="280" src="https://github.com/user-attachments/assets/1b963f7b-a4e0-46c9-835a-5fe4d8f9a40c" alt="Screenshot 075451" />
<br>
<br>
<p>8. IoT MQTT Panel - Dash (Customizable - send: wol/event - receive: wol/status and wol/log)</p>
<img width="600" height="800" src="https://github.com/user-attachments/assets/1ad0e9a7-2fc2-4a06-8f62-421ac95fb27d" alt="Screenshot 075451" />

</div>
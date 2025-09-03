# 🔥 Wake-on-LAN ESP32-C3 + MQTT + OTA + Portal Wi-Fi v5.0 (Not working OTA)

Advanced ESP32-C3 project for sending **Wake-on-LAN (WOL) Magic Packets** over Wi-Fi, with full MQTT support, OTA updates, ping-based status checks, and a configuration portal hosted on the device using SPIFFS.

This project supports **hardware button-triggered WOL**, scheduled ping after WOL, OTA updates with MQTT progress reporting, and optional factory reset.

---

## 📋 Features

- 🌐 **Wi-Fi Integration**: Connects to your local Wi-Fi network.
- 🖥️ **Wake-on-LAN (WOL)**: Sends magic packets to wake compatible PCs.
- 🔘 **User Button**: GPIO0 (D0) button sends WOL on >1s press.
- ☁️ **MQTT Support**: Subscribes to `wol/event` for `"TurnOn"` or `"PingPC"` commands and publishes logs/status to `wol/log` and `wol/status`.
- 🔄 **Automatic Ping After WOL**: Schedules a ping 1 minute after sending WOL (non-blocking).
- 🕵️ **Ping-based Status Check**: Uses `ESP32Ping` to verify if the target device is online.
- 🔆 **LED Indicator**: GPIO1 (D1) flashes to indicate WOL, ping, or OTA progress.
- 💾 **OTA Updates**: Checks for firmware every 5 minutes; publishes progress to MQTT every 10%.
- 🛠️ **Factory Reset**: Holding GPIO2 (D2) LOW at boot deletes `config.json`.
- 📄 **Configuration Portal**: Hosts HTML page on SPIFFS to configure Wi-Fi, MQTT, target IP/MAC, and UDP port.

---

## 🛠️ Requirements

- **ESP32-C3 board**
- Target PC with **Wake-on-LAN enabled**
- **MQTT broker** (e.g., HiveMQ, Mosquitto) with credentials
- **Button** connected to **GPIO0 (D0)** for manual WOL
- **LED** connected to **GPIO1 (D1)**
- **Reset / Factory reset button** (optional) connected to **GPIO2 (D2)**
- Libraries:
  - `WiFi.h`
  - `WiFiClientSecure.h`
  - `PubSubClient.h`
  - `ESP32Ping.h`
  - `WiFiUdp.h`
  - `ArduinoJson.h`
  - `SPIFFS.h`
  - `HTTPClient.h`
  - `esp_ota_ops.h`

---

## 📡 MQTT Topics

| Topic        | Purpose                                         |
|-------------|-------------------------------------------------|
| `wol/event` | Subscribe to `"TurnOn"` or `"PingPC"` commands |
| `wol/status`| Publishes `"PC Online"` or `"PC Offline"`      |
| `wol/log`   | Publishes detailed logs (boot, WOL, ping, OTA)|

---

## ⚡ Operation Modes

### 1️⃣ Button-triggered WOL
- Press GPIO0 (>1s) to send a WOL magic packet to the target MAC.
- LED flashes during WOL.
- Schedules a ping 1 minute later to check if the PC is online.

### 2️⃣ MQTT Commands
- `"TurnOn"`: Sends WOL immediately.
- `"PingPC"`: Pings the target and publishes online/offline status.

### 3️⃣ OTA Updates
- Checks every 5 minutes for new firmware (`version.txt`) on GitHub.
- Downloads and flashes firmware via OTA if a newer version is available.
- Reports progress every 10% via MQTT.
- Device restarts automatically after successful OTA.

### 4️⃣ Factory Reset
- Hold GPIO2 (D2) LOW at boot to delete `config.json`.
- Triggers a restart for initial configuration.

### 5️⃣ Configuration Portal
- ESP32-C3 creates a Wi-Fi hotspot `ESP32C3_Config` if `config.json` is missing.
- HTML page (`setup.html` on SPIFFS) allows:
  - Wi-Fi SSID & password
  - MQTT server, port, user, password
  - Target IP & Broadcast IP
  - MAC address for WOL
  - UDP port for magic packets
- Submitting the form saves settings to `config.json` and restarts the device.

---

## ⚙️ Configuration

All settings are stored in `config.json` or via portal. Example structure:

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
Or update directly in portal via browser (http://192.168.4.1).

---

## 🛠️ Pinout Summary

| Pin  | Function                          |
|------|----------------------------------|
| D0   | User button (send WOL)            |
| D1   | LED indicator                     |
| D2   | Factory reset / config portal     |

---

## 🧪 Detailed Behavior

### Boot
- Checks `config.json`.  
- If missing, starts Wi-Fi AP and configuration portal.  
- Otherwise, connects to Wi-Fi and MQTT.  
- Publishes boot log and status.

### Button Press (D0)
- Sends WOL packet.  
- LED flashes.  
- Schedules ping 1 min later.

### MQTT Commands
- `"TurnOn"` → WOL  
- `"PingPC"` → Ping target, update status

### OTA Updates
- Fetches `version.txt` from GitHub.  
- Downloads `.bin` if version differs.  
- Writes to OTA partition.  
- Publishes progress to MQTT.  
- Reboots after update.

### Factory Reset
- D2 LOW at boot deletes `config.json`.  
- Reboot triggers Wi-Fi portal.

---

## 💡 Notes

- **Debounce**: Button press requires >1 second to trigger WOL.  
- **Magic Packet**: Broadcast UDP to `broadcastIP:udp_port` using target MAC.  
- **MQTT Logs**: Full OTA and ping progress published to `wol/log`.  
- **SPIFFS HTML**: Ensure `setup.html` is uploaded via Arduino IDE or ESP32FS tool.  
- **Firmware Version**: Stored in `FIRMWARE_VERSION` constant; OTA compares with `version.txt`.

---

# ✨ Thanks for using this project!

## ⚡Image

<div align="center">

![SHEMATIC](https://github.com/user-attachments/assets/69b907f5-264b-4f98-b777-c53e9436570a)

![PINOUT](https://github.com/user-attachments/assets/5ac26256-06c6-40ae-ab29-bd35d11dfe80)

<img width="1014" height="954" alt="Captura de tela 2025-09-03 201030" src="https://github.com/user-attachments/assets/f758a0f1-685b-4d10-8cdd-0a98d9a8f501" />

<img width="218" height="24" alt="Captura de tela 2025-09-03 195423" src="https://github.com/user-attachments/assets/c4edd568-4199-45e3-b0d0-dcecbf7f3bd9" />

<img width="600" height="361" alt="Captura de tela 2025-09-03 213951" src="https://github.com/user-attachments/assets/a818f4de-7b05-4c5f-ae86-9de40ca9604e" />

</div>

# 🔥 Wake-on-LAN ESP32-C3 + MQTT + OTA + Portal Wi-Fi v5.1 (Not working OTA)

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
- Downloads firmware to SPIFFS (supports resuming if interrupted).
- Flashes firmware via OTA to ESP32 partition.
- Reports detailed progress via MQTT:
  - Download progress
  - SHA256 verification
  - Flash writing
  - Update success or errors
- Interrupted downloads resume automatically from where they stopped.
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

## 🚀 OTA Update - Detailed MQTT Output (Example)

The OTA implementation performs:

- 🔍 Checks for a new firmware version
- 💾 Downloads firmware to SPIFFS (to allow resuming)
- ✅ Verifies SHA256 checksum
- ⚡ Safely writes to the OTA partition
- 📡 Provides detailed MQTT logs

---

### **1. Firmware already up-to-date ✅**

- 🔍 OTA: Checking for updates...
- 🌐 OTA: Remote version found: 1.02
- ✅ OTA: Firmware is already up to date.

---

### **2. New firmware available, full download without interruption ⬇️**

- 🔍 OTA: Checking for updates...
- 🌐 OTA: Remote version found: 1.03
- ⚡ OTA: New version available: 1.03
- 💾 OTA: Downloading firmware...
- ⏳ OTA: Download progress 1%
- ⏳ OTA: Download progress 5%
- ⏳ OTA: Download progress 10%
- ...
- ⏳ OTA: Download progress 100%
- ✅ OTA: Firmware downloaded successfully.
- 🔒 OTA: SHA256 verified successfully.
- ⚡ OTA: Writing to flash 1%
- ⚡ OTA: Writing to flash 5%
- ⚡ OTA: Writing to flash 10%
- ...
- ⚡ OTA: Writing to flash 100%
- 🎉 OTA: Update applied successfully! Restarting...

---

### **3. Download interrupted and resumed 🔄**

- 🔍 OTA: Checking for updates...
- 🌐 OTA: Remote version found: 1.03
- ⚡ OTA: New version available: 1.03
- 💾 OTA: Found partial file in SPIFFS (size 512000 bytes). Resuming download...
- ⏳ OTA: Download progress 40%
- ⏳ OTA: Download progress 41%
- ⏳ OTA: Download progress 42%
- ...
- ⚠️ OTA: Network error! Download interrupted.
- 🔄 OTA: Retrying download, resuming from 42%...
- ⏳ OTA: Download progress 43%
- ⏳ OTA: Download progress 44%
- ...
- ⏳ OTA: Download progress 100%
- ✅ OTA: Firmware downloaded successfully.
- 🔒 OTA: SHA256 verified successfully.
- ⚡ OTA: Writing to flash 1%
- ...
- ⚡ OTA: Writing to flash 100%
- 🎉 OTA: Update applied successfully! Restarting...

---

### **4. Possible errors ❌**

#### 4.1 Failed to fetch remote version 🌐

- 🔍 OTA: Checking for updates...
- ❌ OTA: Failed to get version (attempt 1), HTTP code: 404
- ❌ OTA: Failed to get version (attempt 2), HTTP code: 404
- ⚠️ OTA: Could not get remote version. Aborting.

#### 4.2 Download with invalid content length 📏

- 💾 OTA: Downloading firmware...
- ❌ OTA: Invalid content length.
- 🔄 OTA: Retrying download...

#### 4.3 Error writing to OTA partition ⚡

- ⚡ OTA: Writing to flash...
- ❌ OTA: esp_ota_write failed.
- 🔄 OTA: Retrying update...

#### 4.4 SHA256 mismatch 🔒

- ✅ OTA: Firmware downloaded successfully.
- ❌ OTA: SHA256 mismatch! Update aborted.

---

### **Summary 📝**

- ✅ **Success:** Firmware updated, ESP restarts automatically.
- 🔄 **Resume:** Interrupted downloads resume from where they stopped via SPIFFS.
- ❌ **Errors:** Detailed on MQTT, multiple retry attempts before aborting.

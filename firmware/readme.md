# Firmware (OTA Zone)

This folder contains the **firmware binaries and version file** used by the ESP32 OTA (Over-The-Air) update system.

## 📂 Contents
- **`WOL_ESP32.bin`** → The compiled ESP32 firmware binary uploaded here by the developer.  
- **`version.txt`** → A plain text file containing the current firmware version string (e.g., `5.3`).  

## 🔄 OTA Workflow
1. On boot or at scheduled intervals, the ESP32 checks the `version.txt` file hosted in this folder (via raw GitHub URL).  
2. If the version in `version.txt` is **newer than the one running locally**, the ESP32 downloads `WOL_ESP32.bin`.  
3. The binary is written directly to the OTA partition.  
4. After flashing, the ESP32 restarts automatically and runs the new firmware.

👉 **Important:** OTA updates only replace the firmware.  
The configuration stored in **`/config.json`** (set via the WiFi/MQTT portal) is **preserved** and not modified by the update.  
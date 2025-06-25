# 🔥 Wake-on-LAN V2 with ESP32 + MQTT (Light Sleep Mode + Watchdog)

A complete ESP32-based project for sending Wake-on-LAN (WOL) Magic Packets over Wi-Fi, now upgraded with:

- **MQTT remote control**
- **Light sleep mode for energy efficiency**
- **Ping-based device status**
- **Hardware watchdog timer**
- **Button-triggered wake**
- **Online/offline MQTT reporting**

> 💤 Efficient light sleep with watchdog refresh ensures low-power operation without disconnecting Wi-Fi or MQTT. Wake-up can occur via GPIO or network command.

---

## 📋 Features

- 🌐 **Wi-Fi Integration**: Connects securely to your local Wi-Fi network.
- 🖥️ **Wake-on-LAN**: Sends standard WOL Magic Packets to turn on compatible PCs.
- 🔘 **Hardware Button Wake**: Wakes target device by button press on GPIO0 (D0).
- ☁️ **MQTT Support**: Receives `"TurnOn"` command and reports online status via topics.
- 🔄 **Magic Packet Burst**: Sends 10 packets at 100ms intervals for reliability.
- 💤 **Light Sleep Mode**: Uses `esp_light_sleep_start()` to reduce current while maintaining fast wake-up and MQTT/Wi-Fi connection.
- 🧠 **Ping-based Status Check**: Uses `ESP32Ping` to verify if the target is online after event or every 1 min.
- 🐶 **Watchdog Timer**: Hardware watchdog using `hw_timer_t` resets the ESP if it hangs (e.g., during Wi-Fi connection).
- 🔆 **LED Indicator**: GPIO1 (D1) indicates WOL operation and activity.
- 🔋 **Sleep Control Pin**: GPIO2 (D2) enables sleep mode with watchdog refresh.

---

## 🛠️ Requirements

- An **ESP32 board** (e.g., WROOM, C3, etc).
- Target PC or device with **Wake-on-LAN enabled**.
- Access to an **MQTT broker** (e.g., Mosquitto, HiveMQ).
- **Button** connected to **GPIO0** (D0).
- **Sleep control input** connected to **GPIO2** (D2).
- **LED** connected to **GPIO1** (D1) for feedback.
- Libraries used:
  - `WiFi.h`
  - `WiFiClientSecure.h`
  - `WiFiUDP.h`
  - `PubSubClient.h`
  - `ESP32Ping.h`

---

## 📡 MQTT Topics

- `wol/event`: Subscribe to receive `"TurnOn"` command.
- `wol/status`: Publishes `"On"` or `"Off"` based on ping to the target IP.
- `wol/log`: Publishes status and debug logs (e.g., Magic Packet sent, sleeping...).

---

## 🧪 Behavior Overview

- On boot, connects to Wi-Fi and MQTT broker.
- Publishes status (based on ping).
- Listens for MQTT `"TurnOn"` command or GPIO0 button press.
- Sends Magic Packets and updates MQTT status.
- Enters light sleep if sleep mode is enabled via GPIO2.
- Watchdog timer ensures the ESP auto-resets if the system becomes unresponsive.

---

Thank you for exploring this project! 🚀

## Image

![SHEMATIC](https://github.com/user-attachments/assets/69b907f5-264b-4f98-b777-c53e9436570a)
![PINOUT](https://github.com/user-attachments/assets/5ac26256-06c6-40ae-ab29-bd35d11dfe80)

## Github link

[Wake-on-LAN_ESP32C3](https://github.com/manoper93/Wake-on-LAN_ESP32C3)

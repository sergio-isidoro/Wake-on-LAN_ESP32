# 🔥 Wake-on-LAN V2 with ESP32 + MQTT (Light Sleep Mode)

A fully featured ESP32 project that sends Wake-on-LAN (WOL) Magic Packets over Wi-Fi — now enhanced with **MQTT control**, **light sleep**, **ping-based online status**, **watchdog safety**, and **hardware button wake-up**.

> This project supports near-instant wake-up via MQTT **without reconnecting Wi-Fi** using **light sleep**, while keeping power usage low.

---

## 📋 Features

- 🌐 **Wi-Fi Integration**: Connects to a secured network.
- 🖥️ **Wake-on-LAN**: Sends Magic Packets to wake compatible computers.
- 🔘 **Hardware Button Wake**: Triggers WOL by pressing a button on GPIO2.
- ☁️ **MQTT Support**: Receives `"turnon"` command and publishes `"online"` / `"offline"` based on ping result.
- 🔄 **Magic Packet Burst**: Sends 10 packets, spaced at 100ms intervals.
- 💤 **Light Sleep Mode**: Keeps MQTT and Wi-Fi alive while reducing power.
- 🧠 **Ping Status Detection**: Pings target device to report online/offline status.
- 🐶 **Watchdog Timer**: Auto-restarts if Wi-Fi or MQTT hangs.
- 🔆 **LED Indicator**: GPIO3 (D1) turns on at boot.

---

## 🛠️ Requirements

- An **ESP32** (e.g., ESP32C3, ESP32-WROOM, etc).
- A PC configured for **Wake-on-LAN** (WOL).
- Access to an **MQTT broker** (e.g., HiveMQ, Mosquitto, etc).
- A **push button** connected to **GPIO2** (D0).
- The following Arduino libraries:
  - `ESP32Ping`
  - `AsyncMqttClient`
  - `ESPAsyncTCP`
- Wi-Fi credentials and target MAC/IP.

---

Thank you for exploring this project! 💡

## Image

![pin_map-2](https://github.com/user-attachments/assets/5ac26256-06c6-40ae-ab29-bd35d11dfe80)

## Github link

[Wake-on-LAN_ESP32C3](https://github.com/manoper93/Wake-on-LAN_ESP32C3)

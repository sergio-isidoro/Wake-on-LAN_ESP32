# 🔥 Wake-on-LAN with ESP32C3

A simple ESP32-based implementation for sending Wake-on-LAN (WOL) Magic Packets over a Wi-Fi network. This project demonstrates how to configure an ESP32 to wake up devices using Magic Packets triggered by a button press.

---

## 📋 Features

- 🌐 **Wi-Fi Integration**: Connects to a specified Wi-Fi network.
- 🖥️ **Wake-on-LAN (WOL)**: Sends Magic Packets to wake up compatible devices.
- 🔘 **Hardware Button Support**: Sends the WOL signal when a button is pressed.
- ⚡ **Efficient Loop**: Minimal delays to ensure responsiveness.

---

## 🛠️ Requirements

- An **ESP32** microcontroller.
- A device capable of being woken up via **Wake-on-LAN**.
- A **push button** connected to GPIO2/D0 (or a configurable GPIO pin).
- Wi-Fi network credentials.
- WakeOnLanMonitor.exe for testing.

---

## 🚀 Getting Started

### 1️⃣ Setup Hardware

1. Connect a push button to GPIO2/D0 (or modify the code for a different GPIO pin).
2. Ensure the ESP32 is powered and connected to your network.

---

## 🎯 **Next Steps**
- Implement deep sleep. (DONE ✅)

---

Thank you for exploring this project! 💡

## Image

 ![pin_map-2](https://github.com/user-attachments/assets/5ac26256-06c6-40ae-ab29-bd35d11dfe80)

## Github link

[Wake-on-LAN_ESP32C3](https://github.com/manoper93/Wake-on-LAN_ESP32C3)

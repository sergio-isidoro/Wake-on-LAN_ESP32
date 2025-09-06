# WakeOnLanMonitor

A small Windows tool (`WakeOnLanMonitor.exe`) to check whether **Wake-on-LAN (WOL) magic packets** are being received on your network.

## 📌 Features
- Listens for WOL **magic packets** on a configurable UDP port (default: 9).
- Displays sender IP, MAC address, and timestamp for each packet.
- Useful for testing ESP32 WOL implementations or troubleshooting WOL in your LAN.
- Lightweight standalone `.exe` (no installation required).
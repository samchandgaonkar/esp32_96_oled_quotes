# ESP32 Multi-Display Smart Clock & Quotes Ticker

An ESP32-powered desktop smart clock and weather station. It automatically syncs local time via NTP, fetches local weather data using IP-based geolocation, and displays dynamically retrieved random quotes every 30 seconds with custom scrolling animations.

## Features

- 🕰️ **Automatic Time Sync**: Connects to WiFi and synchronizes local time (UTC+5:30) automatically via NTP.
- 🌦️ **Geolocation & Weather**: Pinpoints location via IP geolocation and fetches current temperature and weather descriptions from Open-Meteo.
- 💬 **Dynamic Quotes Ticker**: Fetches random inspirational quotes from `dummyjson.com` over secure HTTPS every 30 seconds.
- 📺 **Dual Display Support**: A single configuration variable toggles between:
  - **0.96" SSD1306 OLED (128x64)**: Features smooth horizontal pixel-by-pixel scrolling using the elegant `FreeSansBold12pt7b` font. Long quotes scroll, while short quotes are centered statically.
  - **I2C Character LCD (16x2 / 20x4)**: Features a classic right-to-left ticker-tape character shift on the bottom line.
- 📶 **OTA Updates (Over-The-Air)**: Flash new code wirelessly over your local WiFi network.
- 🔍 **I2C Address Scanner**: Scans and prints all connected I2C devices to the Serial Monitor at bootup to aid in hardware troubleshooting.

---

## Hardware Configuration

Ensure the `ACTIVE_DISPLAY` macro in `src/main.cpp` matches your connected display.

### Pinout (Common to both displays)
- **SDA**: Connects to ESP32 Pin **`D21`**
- **SCL**: Connects to ESP32 Pin **`D22`**
- **GND**: Connects to ESP32 **`GND`**

### VCC Connection Requirements
* **0.96" OLED Display**: Connect VCC to the **`3.3V`** pin on the ESP32.
* **I2C Character LCD (16x2 / 20x4)**: Connect VCC to the **`VIN` (5V)** pin on the ESP32 for proper liquid crystal contrast.

---

## Software Settings

Configure your WiFi network credentials in `src/main.cpp`:
```cpp
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";
```

Toggle the active display:
```cpp
#define ACTIVE_DISPLAY DISPLAY_OLED // For 0.96" OLED
// OR
#define ACTIVE_DISPLAY DISPLAY_LCD  // For 16x2 / 20x4 I2C LCD
```

---

## Build & Flash

Built using **PlatformIO**. 

1. Install PlatformIO in VS Code.
2. Build and upload:
   ```bash
   platformio run --target upload
   ```
3. Open the Serial Monitor at `115200` baud rate to monitor startup:
   ```bash
   platformio device monitor
   ```

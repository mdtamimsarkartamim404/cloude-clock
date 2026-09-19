# CYD Smart Sky Clock 🌤️⏰

A feature-packed, interactive desk clock and weather station built for the **ESP32-2432S028R** (Cheap Yellow Display / CYD 2.8" ST7789 TFT display). It features real-time time synchronization over NTP, dynamic weather updates via OpenWeatherMap, automatic day/night theme transitions, and built-in touch mini-games.

---

## ✨ Features

* **Real-Time Clock (NTP Synchronized):**
* Automatic time sync via NTP servers (`pool.ntp.org`).
* 12-hour time display with AM/PM indicator, blinking colon, date, and day.
* WiFi signal strength indicator (RSSI).


* **Live Weather Display:**
* Fetches temperature, "feels like", humidity, wind speed, and atmospheric pressure.
* OpenWeatherMap API integration.
* Custom dynamic weather icons (Sun, Moon, Clouds, Rain, Thunderstorm, Snow, Mist).


* **Dynamic Day/Night Theme:**
* Automatically switches UI themes (Background, Cards, Accents) based on the current time.
* Night mode features background starry skies; Day mode features subtle cloud animations.


* **Built-in Touch Games:**
* **Catch Game:** Interactive touch arcade game where you catch falling weather items. Saves high scores using ESP32 Non-Volatile Storage (`Preferences`).
* **Tic-Tac-Toe (X-O):** Play against an intelligent onboard AI algorithm with score history tracking (Wins, Losses, Draws).


* **On-Screen Touch Navigation:** Smooth bottom navigation bar to switch between Clock, Weather, Catch Game, and Tic-Tac-Toe.

---

## 🛠️ Hardware Requirements

* **Board:** ESP32-2432S028R (CYD / Cheap Yellow Display - 2.8" ST7789 TFT with XPT2046 Touch Screen)
* **Connectivity:** Micro-USB or USB-C cable for programming and power supply.

---

## 📚 Required Libraries

Install the following libraries using the **Arduino Library Manager** (`Ctrl + Shift + I` in Arduino IDE):

1. **TFT_eSPI** (by Bodmer)
2. **XPT2046_Touchscreen** (by Paul Stoffregen)
3. **ArduinoJson** (Version `7.x` by Benoit Blanchon)

---

## ⚙️ Setup & Configuration

### 1. Configure `TFT_eSPI` Library

Before uploading the main code, you **must** replace or update the `User_Setup.h` file located inside your local Arduino library directory:
`.../Arduino/libraries/TFT_eSPI/User_Setup.h`

Paste the following setup configuration:

```cpp
#define USER_SETUP_ID 202
#define ST7789_DRIVER

#define TFT_WIDTH  240
#define TFT_HEIGHT 320

#define TFT_INVERSION_ON
#define TFT_RGB_ORDER TFT_BGR

#define USE_HSPI_PORT

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1
#define TFT_BL   21
#define TFT_BACKLIGHT_ON HIGH

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define SMOOTH_FONT

#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY  20000000

```

---

### 2. Configure Credentials in `CYD_Smart_Sky_Clock.ino`

Open the sketch file and update the WiFi credentials, OpenWeatherMap API key, and location settings:

```cpp
// ================== User Configuration ==================
const char* WIFI_SSID  = "YOUR_WIFI_NAME";
const char* WIFI_PASS  = "YOUR_WIFI_PASSWORD";
const char* OWM_KEY    = "YOUR_OPENWEATHERMAP_API_KEY"; // Free key from openweathermap.org
const char* OWM_CITY   = "Rajshahi,BD";                // City, Country Code
const char* CITY_LABEL = "RAJSHAHI";                    // Display label on UI
const long  GMT_OFFSET = 6 * 3600;                     // UTC Offset in seconds (e.g., UTC+6 = 6 * 3600)
// ========================================================

```

---

### 3. Board Settings in Arduino IDE

* **Board:** `ESP32 Dev Module`
* **CPU Frequency:** `240MHz (WiFi / BT)`
* **Flash Frequency:** `80MHz`
* **Flash Mode:** `QIO`
* **Partition Scheme:** `Default 4MB with spiffs` or `Huge APP (3MB No OTA/1MB SPIFFS)`
* **Upload Speed:** `921600` or `115200`

---

## 📌 Pinout Reference (ESP32-2432S028R)

| Function | ESP32 Pin | SPI Bus |
| --- | --- | --- |
| **TFT Display (ST7789)** |  | **HSPI** |
| TFT MOSI | GPIO 13 | HSPI |
| TFT MISO | GPIO 12 | HSPI |
| TFT SCLK | GPIO 14 | HSPI |
| TFT CS | GPIO 15 | HSPI |
| TFT DC | GPIO 2 | — |
| TFT Backlight | GPIO 21 | — |
| **Touch Screen (XPT2046)** |  | **VSPI** |
| Touch MOSI | GPIO 32 | VSPI |
| Touch MISO | GPIO 39 | VSPI |
| Touch CLK | GPIO 25 | VSPI |
| Touch CS | GPIO 33 | VSPI |
| Touch IRQ | GPIO 36 | — |

---

## 🎮 How to Play

### 1. Catch Game

* Tap items as they fall from the top before they reach the bottom navigation bar.
* **Sun / Raindrop:** +1 Point
* **Star:** +5 Points
* **Dark Cloud:** -3 Points
* High scores are saved automatically to board memory.

### 2. Tic-Tac-Toe

* Play as **X** against the CYD AI (**O**).
* Tap any empty space on the grid to place your mark.
* Track your total Wins, Losses, and Draws on the right panel.

---

## 📄 License

This project is open-source under the MIT License. Feel free to modify and adapt it for personal projects!

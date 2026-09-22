# 🌟 CYD HUB Professional Control v2

CYD HUB is a feature-rich, all-in-one smart dashboard and retro gaming hub designed specifically for the **ESP32-2432S028R (Cheap Yellow Display - 2.8" ST7789 version)**. 

It features a completely standalone UI combined with a **Professional Web Control Center**, allowing you to set up Wi-Fi, change themes, track weather, and control the device remotely from your smartphone without ever needing to hardcode network credentials into the sketch.

---

## ✨ Key Features

### 📱 Professional Phone Control (Web UI)
*   **Zero Hardcoding:** Scan the on-screen QR codes to connect to the CYD's access point and open the control center.
*   **Wi-Fi Scanner:** Scan and connect to your home network directly from your phone browser.
*   **Remote Navigation:** Switch between screens (Clock, Games, Weather, Settings) remotely.
*   **Device Management:** Adjust brightness, change themes, toggle 12/24h time, and sync NTP manually.
*   **Location Config:** Enter your City, Latitude, Longitude, and Timezone directly from the phone for accurate weather data.
*   **Custom Palette Editor:** Build and apply your own custom color themes using the web-based color picker.

### 🕰️ Smart Clocks (10 Styles)
Features a highly optimized rendering engine with smooth, flicker-free analog hand movements using off-screen sprites.
1. Digital
2. Analog (Smooth movement)
3. Big Digital
4. Minimal
5. Ring
6. Neon
7. Dashboard
8. Split
9. Clean
10. Seconds

### 🌦️ Live Weather Station
*   Powered by the **Open-Meteo API** (No API key required).
*   Displays current temperature, condition (with custom icons), "feels like" temp, humidity, and wind speed.
*   Includes a 4-day future forecast (Highs/Lows and conditions).

### ⏱️ Productivity Tools
*   **Timer:** Visual progress bar, precise adjustment buttons, and loud alarm.
*   **Stopwatch:** Millisecond accuracy with split/lap tracking.

### 🎮 Retro Gaming Console (8 Games)
Includes local high-score tracking saved directly to the ESP32's non-volatile memory (NVS).
*   **X & O (Tic-Tac-Toe):** Play against an AI opponent.
*   **Snake:** Swipe-to-steer classic snake game.
*   **Memory:** Match-2 card game with 16 tiles.
*   **Whack-a-Mole:** Fast-paced tapping challenge (30-second rush).
*   **Reflex:** Millisecond reaction time tester.
*   **2048:** Full swipe-based 2048 puzzle game.
*   **Bricks (Breakout):** Paddle and ball block-breaking game with increasing levels.
*   **Simon:** Memory sequence game with audio-visual feedback.

---

## 🛠️ Hardware Requirements

*   **Board:** ESP32-2432S028R (Cheap Yellow Display)
*   **Screen:** 2.8" TFT Touch Screen (ST7789 controller)
*   **Features used:** Touchscreen (XPT2046), Backlight control, Built-in Speaker/Buzzer.

---

## 💻 Software & Library Dependencies

Install the following libraries via the Arduino IDE Library Manager:

1.  **`TFT_eSPI`** by Bodmer
2.  **`XPT2046_Touchscreen`** by Paul Stoffregen
3.  **`ArduinoJson`** by Benoit Blanchon (Must be v7.x)

*Note: `WiFi`, `WebServer`, `HTTPClient`, `Preferences`, and `time` are built into the ESP32 Arduino Core.*

### TFT_eSPI Configuration (`User_Setup.h`)
You must configure the `TFT_eSPI` library to work with the CYD. Replace the contents of your `User_Setup.h` (found in the `TFT_eSPI` library folder) with the correct pins for the ESP32-2432S028R. 
*(Usually: Driver = ST7789, TFT_WIDTH = 240, TFT_HEIGHT = 320, MOSI = 23, SCLK = 18, CS = 15, DC = 2, RST = 4, BL = 21).*

---

## 🚀 Installation & Setup

1.  **Clone or Download** this repository.
2.  Open the `.ino` file in the Arduino IDE.
3.  Select Board: **ESP32 Dev Module**.
4.  Set Partition Scheme: **Huge APP (3MB No OTA/1MB SPIFFS)** or **Default 4MB with spiffs**.
5.  Compile and Upload the code to your CYD.

---

## 📱 How to Connect and Use

### Step 1: Initial Wi-Fi Setup (On Boot)
1. When you power on the CYD for the first time, it will open the **Phone Wi-Fi Setup** screen.
2. Scan the **first QR code** with your smartphone to connect to the CYD's local Access Point:
    *   **SSID:** `CYD-HUB-CONTROL`
    *   **Password:** `CYDControl24`
3. Tap "NEXT" on the CYD screen.
4. Scan the **second QR code** to open the Control Center in your phone's browser (or navigate manually to `http://192.168.4.1/`).

### Step 2: Connect to Home Network
1. In the Web Control Center on your phone, click **"Scan Nearby Networks"**.
2. Select your home Wi-Fi from the list and enter the password.
3. The CYD will connect to your router, sync the time via NTP, and download the latest weather data.

### Step 3: Customization
From the Web Control Center, you can:
*   Set your exact City, Latitude, and Longitude for weather.
*   Change the UTC offset (default is `+6` for Bangladesh).
*   Apply custom HEX colors to the UI.
*   Force screen changes on the device remotely.

---

## ⚙️ Settings Overview (On-Device)

If you don't want to use the web app, you can change core settings directly on the device:
*   **Theme:** Cycle through Midnight, Sunset, Forest, Light, and Amoled.
*   **Brightness:** Drag the slider to adjust screen backlight intensity.
*   **24-hour clock:** Toggle AM/PM vs 24h format.
*   **Sound:** Enable/disable UI beeps and game sounds.
*   **Temperature:** Switch between Celsius and Fahrenheit.
*   **Wi-Fi Setup / Scanner:** Opens the AP mode for phone reconfiguration.

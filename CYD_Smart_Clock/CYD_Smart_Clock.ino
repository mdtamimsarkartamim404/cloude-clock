/***************************************************************************
 *  CYD HUB  -  ESP32-2432S028R  (2.8" Cheap Yellow Display, ST7789 version)
 *
 *  Clock (digital + analog) | Timer + Stopwatch | Weather (Open-Meteo)
 *  8 Games | Settings (5 themes, brightness, 12/24h, sound, C/F)
 *  Phone Remote Control (built-in web page over WiFi)
 *
 *  LIBRARIES (Library Manager):
 *    - TFT_eSPI               (Bodmer)   -> use the supplied User_Setup file
 *    - XPT2046_Touchscreen    (Paul Stoffregen)
 *    - ArduinoJson            (v7.x, Benoit Blanchon)
 *    - WebServer is built into the ESP32 Arduino core, no install needed
 *  BOARD: "ESP32 Dev Module"  (Arduino-ESP32 core 2.0.x or 3.x both work)
 *         Partition: "Huge APP" or "Default 4MB" both fine.
 *
 *  Edit the USER SETTINGS block below (WiFi + location), then upload.
 *
 *  PHONE CONTROL:
 *    Once the board connects to WiFi, open Serial Monitor (115200 baud) or
 *    check your router's client list for its IP address, then visit
 *    http://<that-ip>/  in your phone's browser. You can also check the
 *    Settings screen on the device itself, which shows the IP address.
 ***************************************************************************/
#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>

// ============================== USER SETTINGS ==============================
const char* WIFI_SSID = "Home Network";
const char* WIFI_PASS = "tamim24@#";
const long  GMT_OFFSET_SEC = 6 * 3600;      // Bangladesh = UTC+6
const int   DST_OFFSET_SEC = 0;
const char* CITY_NAME = "Rajshahi";
const float LATITUDE  = 24.3745f;           // used for weather
const float LONGITUDE = 88.6042f;

#define TFT_ROT        1     // 1 = landscape. If upside-down use 3 and set TOUCH_FLIP 1
#define TOUCH_FLIP     0
#define INVERT_COLORS  0     // set 1 if colours look "negative"

// ---- Touch calibration (from your calibration map, averaged per edge) ----
//  X: left = (483+396)/2 = 440   right = (3634+3502)/2 = 3568
//  Y: top  = (547+461)/2 = 504   bottom = (3575+3548)/2 = 3561
#define TS_X_MIN 440
#define TS_X_MAX 3568
#define TS_Y_MIN 504
#define TS_Y_MAX 3561
#define TS_Z_MIN 250
// ===========================================================================

// ---- CYD pins ----
#define PIN_BL      21
#define PIN_SPK     26
#define TOUCH_CLK   25
#define TOUCH_MISO  39
#define TOUCH_MOSI  32
#define TOUCH_CS    33

#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  #define CORE3 1
#else
  #define CORE3 0
#endif

// ---- screens ----
#define SCR_HOME     0
#define SCR_CLOCK    1
#define SCR_TIMER    2
#define SCR_WEATHER  3
#define SCR_GAMES    4
#define SCR_SETTINGS 5
#define SCR_GAME     6

#define C565(r,g,b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

const int SW = 320, SH = 240, BAR = 26;

TFT_eSPI tft = TFT_eSPI();
SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(TOUCH_CS);
Preferences prefs;

int screen = SCR_HOME;
int curGame = 0;

// ============================ FORWARD DECLARATIONS ============================
void goScreen(int s);
void gameStart(int g);
void homeHeader(bool force);
void wxDraw();
void tmDrawButtons();
void beep(int f, int ms);
void submitScore(int g, uint32_t s);
void flushHi();
void saveCfg();

// ================================ THEMES ====================================
struct Theme {
  const char* name;
  uint16_t bg, panel, text, dim, accent, accent2, good, bad, onacc;
};
const Theme themes[] = {
  {"Midnight", C565(10,12,26),  C565(26,30,56),  C565(236,241,255), C565(124,134,166), C565(0,200,255),  C565(255,90,200), C565(70,225,140),  C565(255,90,90),  C565(10,12,26)},
  {"Sunset",   C565(32,12,22),  C565(62,26,42),  C565(255,238,222), C565(178,128,116), C565(255,146,64), C565(255,84,124), C565(150,225,110), C565(255,72,72),  C565(32,12,22)},
  {"Forest",   C565(8,24,17),   C565(18,50,34),  C565(228,255,234), C565(112,164,134), C565(88,232,122), C565(244,222,92), C565(130,255,170), C565(255,104,84), C565(8,24,17)},
  {"Light",    C565(238,243,250), C565(255,255,255), C565(20,26,44), C565(118,128,150), C565(30,110,255), C565(255,90,140), C565(20,170,90),  C565(220,50,50),  C565(255,255,255)},
  {"Amoled",   C565(0,0,0),     C565(22,22,26),  C565(255,255,255), C565(130,130,140), C565(176,112,255), C565(0,224,200), C565(80,230,120),  C565(255,80,80),  C565(0,0,0)}
};
const int NTHEMES = sizeof(themes) / sizeof(themes[0]);

struct Cfg { uint8_t theme; uint8_t bright; bool h24; bool sound; bool fahr; bool analog; };
Cfg cfg = {0, 200, false, true, false, false};
#define TH themes[cfg.theme]

uint32_t hi[8];
bool hiDirty = false;

// ============================ STORAGE =======================================
void saveCfg() {
  prefs.putUChar("theme", cfg.theme);
  prefs.putUChar("bright", cfg.bright);
  prefs.putBool("h24", cfg.h24);
  prefs.putBool("snd", cfg.sound);
  prefs.putBool("fahr", cfg.fahr);
  prefs.putBool("ana", cfg.analog);
}
void loadCfg() {
  prefs.begin("cydhub", false);
  cfg.theme  = prefs.getUChar("theme", 0);
  if (cfg.theme >= NTHEMES) cfg.theme = 0;
  cfg.bright = prefs.getUChar("bright", 200);
  if (cfg.bright < 10) cfg.bright = 10;
  cfg.h24    = prefs.getBool("h24", false);
  cfg.sound  = prefs.getBool("snd", true);
  cfg.fahr   = prefs.getBool("fahr", false);
  cfg.analog = prefs.getBool("ana", false);
  for (int i = 0; i < 8; i++) {
    char k[4]; snprintf(k, sizeof(k), "h%d", i);
    hi[i] = prefs.getUInt(k, 0);
  }
}
void flushHi() {
  if (!hiDirty) return;
  hiDirty = false;
  for (int i = 0; i < 8; i++) {
    char k[4]; snprintf(k, sizeof(k), "h%d", i);
    prefs.putUInt(k, hi[i]);
  }
}
bool lowerBetter(int g) { return g == 2 || g == 4; }
void submitScore(int g, uint32_t s) {
  bool better;
  if (hi[g] == 0) better = (s > 0);
  else better = lowerBetter(g) ? (s < hi[g]) : (s > hi[g]);
  if (better) { hi[g] = s; hiDirty = true; }
}

// ============================ SOUND / BACKLIGHT =============================
uint32_t beepEnd = 0;
void soundTone(int f) {
#if CORE3
  ledcWriteTone(PIN_SPK, f);
#else
  ledcWriteTone(2, f);
#endif
}
void beep(int f, int ms) {
  if (!cfg.sound) return;
  soundTone(f);
  beepEnd = millis() + ms;
  if (beepEnd == 0) beepEnd = 1;
}
void beepTick() {
  if (beepEnd && (int32_t)(millis() - beepEnd) >= 0) { soundTone(0); beepEnd = 0; }
}
void setBacklight(uint8_t v) {
#if CORE3
  ledcWrite(PIN_BL, v);
#else
  ledcWrite(0, v);
#endif
}
void hwPwmInit() {
#if CORE3
  ledcAttachChannel(PIN_BL, 5000, 8, 0);
  ledcAttachChannel(PIN_SPK, 2000, 8, 2);
#else
  ledcSetup(0, 5000, 8);  ledcAttachPin(PIN_BL, 0);
  ledcSetup(2, 2000, 8);  ledcAttachPin(PIN_SPK, 2);
#endif
  setBacklight(cfg.bright);
  soundTone(0);
}

// ============================ TOUCH =========================================
struct TouchState { bool down, was, press, release; int x, y; uint32_t lastSeen; };
TouchState tc = {false, false, false, false, 0, 0, 0};

bool readTouchRaw(int &x, int &y) {
  if (!ts.touched()) return false;
  TS_Point p = ts.getPoint();
  if (p.z < TS_Z_MIN) return false;
  long mx = map(p.x, TS_X_MIN, TS_X_MAX, 0, SW - 1);
  long my = map(p.y, TS_Y_MIN, TS_Y_MAX, 0, SH - 1);
  mx = constrain(mx, 0, SW - 1);
  my = constrain(my, 0, SH - 1);
  if (TOUCH_FLIP) { mx = SW - 1 - mx; my = SH - 1 - my; }
  x = (int)mx; y = (int)my;
  return true;
}
void updateTouch() {
  int x = 0, y = 0;
  bool raw = readTouchRaw(x, y);
  uint32_t now = millis();
  if (raw) { tc.lastSeen = now; tc.x = x; tc.y = y; }
  bool d = raw || (tc.was && (now - tc.lastSeen) < 40);   // small release debounce
  tc.press = d && !tc.was;
  tc.release = !d && tc.was;
  tc.down = d;
  tc.was = d;
}

// ============================ DRAW HELPERS ==================================
void txtp(const String &s, int x, int y, uint8_t font, uint16_t fg, uint16_t bg,
          uint8_t datum, uint8_t size, uint16_t pad) {
  tft.setTextDatum(datum);
  tft.setTextColor(fg, bg);
  tft.setTextSize(size);
  tft.setTextPadding(pad);
  tft.drawString(s, x, y, font);
  tft.setTextPadding(0);
  tft.setTextSize(1);
}
void txt(const String &s, int x, int y, uint8_t font, uint16_t fg, uint16_t bg, uint8_t datum) {
  txtp(s, x, y, font, fg, bg, datum, 1, 0);
}
bool inRect(int px, int py, int x, int y, int w, int h) {
  return px >= x && px < x + w && py >= y && py < y + h;
}
bool pressIn(int x, int y, int w, int h) {
  return tc.press && inRect(tc.x, tc.y, x, y, w, h);
}
void thickLine(int x0, int y0, int x1, int y1, int t, uint16_t c) {
  int r = t / 2;
  for (int k = -r; k <= r; k++) {
    tft.drawLine(x0 + k, y0, x1 + k, y1, c);
    tft.drawLine(x0, y0 + k, x1, y1 + k, c);
  }
}
void button(int x, int y, int w, int h, const String &label, uint16_t fill, uint16_t fg, uint8_t font) {
  tft.fillRoundRect(x, y, w, h, 7, fill);
  txt(label, x + w / 2, y + h / 2 + 1, font, fg, fill, MC_DATUM);
}
void drawWifi(int x, int y) {
  int bars = 0;
  if (WiFi.status() == WL_CONNECTED) {
    int r = WiFi.RSSI();
    bars = (r > -55) ? 4 : (r > -65) ? 3 : (r > -75) ? 2 : 1;
  }
  for (int i = 0; i < 4; i++) {
    int h = 3 + i * 3;
    tft.fillRect(x + i * 4, y + 12 - h, 3, h, i < bars ? TH.accent : TH.dim);
  }
}
void drawBar(const char* title, const char* action, bool wifi) {
  tft.fillRect(0, 0, SW, BAR, TH.panel);
  tft.fillRoundRect(4, 3, 42, 20, 6, TH.accent);
  tft.fillTriangle(16, 13, 25, 7, 25, 19, TH.onacc);
  tft.fillRect(25, 11, 10, 5, TH.onacc);
  txtp(title, 54, BAR / 2 + 1, 2, TH.text, TH.panel, ML_DATUM, 1, 0);
  if (action) button(SW - 68, 3, 40, 20, action, TH.accent2, TH.onacc, 1);
  if (wifi) drawWifi(SW - 20, 7);
}
void clearContent() { tft.fillRect(0, BAR, SW, SH - BAR, TH.bg); }

// ============================ TIME ==========================================
struct tm nowTm;
bool ntpStarted = false;
uint32_t lastWifiTry = 0;
const char* DOW3[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char* MON3[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

bool timeValid() {
  time_t n = time(nullptr);
  if (n < 1700000000L) return false;
  localtime_r(&n, &nowTm);
  return true;
}
int hour12(int h) { int x = h % 12; return x == 0 ? 12 : x; }
int dowOf(int y, int m, int d) {
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) y -= 1;
  return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}
void wifiTick() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!ntpStarted) {
      configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
      ntpStarted = true;
    }
  } else if (millis() - lastWifiTry > 20000) {
    lastWifiTry = millis();
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASS);
  }
}

// ============================ WEATHER DATA ==================================
struct WxData {
  bool ok; float t, feels, wind; int hum, code;
  float hi[4], lo[4]; int dcode[4]; int dow[4];
  uint32_t updated; char stamp[8];
};
WxData wx;
bool wxTried = false;
uint32_t wxLastTry = 0;

const uint16_t COL_SUN = C565(255, 200, 50);
const uint16_t COL_CLOUD = C565(150, 168, 200);
const uint16_t COL_RAIN = C565(70, 150, 255);
const uint16_t COL_SNOW = C565(150, 190, 240);

const char* wmoText(int c) {
  if (c == 0) return "Clear sky";
  if (c == 1) return "Mainly clear";
  if (c == 2) return "Partly cloudy";
  if (c == 3) return "Overcast";
  if (c == 45 || c == 48) return "Fog";
  if (c >= 51 && c <= 57) return "Drizzle";
  if (c >= 61 && c <= 67) return "Rain";
  if (c >= 71 && c <= 77) return "Snow";
  if (c >= 80 && c <= 82) return "Rain showers";
  if (c == 85 || c == 86) return "Snow showers";
  if (c >= 95) return "Thunderstorm";
  return "Cloudy";
}
// 0 sun, 1 partly, 2 cloud, 3 rain, 4 storm, 5 snow, 6 fog
int wmoIcon(int c) {
  if (c <= 1) return 0;
  if (c == 2) return 1;
  if (c == 3) return 2;
  if (c == 45 || c == 48) return 6;
  if ((c >= 51 && c <= 67) || (c >= 80 && c <= 82)) return 3;
  if ((c >= 71 && c <= 77) || c == 85 || c == 86) return 5;
  if (c >= 95) return 4;
  return 2;
}
void drawSun(int cx, int cy, int r, uint16_t col) {
  tft.fillCircle(cx, cy, r, col);
  for (int i = 0; i < 8; i++) {
    float a = i * PI / 4.0f;
    int x0 = cx + (int)(cosf(a) * (r + r * 0.45f));
    int y0 = cy + (int)(sinf(a) * (r + r * 0.45f));
    int x1 = cx + (int)(cosf(a) * (r + r * 0.95f));
    int y1 = cy + (int)(sinf(a) * (r + r * 0.95f));
    thickLine(x0, y0, x1, y1, r > 9 ? 3 : 1, col);
  }
}
void drawCloud(int cx, int cy, int u, uint16_t col) {
  int a = (5 * u) / 2, b = u / 2, c = (9 * u) / 2;
  tft.fillCircle(cx - a, cy + u, 2 * u, col);
  tft.fillCircle(cx + b, cy, 3 * u, col);
  tft.fillCircle(cx + a, cy + u, 2 * u, col);
  tft.fillRect(cx - c, cy + u, 2 * c, 2 * u, col);
}
void drawWxIcon(int cx, int cy, int u, int type) {
  switch (type) {
    case 0: drawSun(cx, cy, u * 3, COL_SUN); break;
    case 1:
      drawSun(cx - u * 2, cy - u * 2, u * 2 + u / 2, COL_SUN);
      drawCloud(cx + u / 2, cy + u, u, COL_CLOUD);
      break;
    case 2: drawCloud(cx, cy, u, COL_CLOUD); break;
    case 3:
      drawCloud(cx, cy - u, u, COL_CLOUD);
      for (int k = -1; k <= 1; k++) {
        int x = cx + k * 2 * u;
        thickLine(x, cy + 2 * u + u / 2, x - u / 2, cy + 4 * u, 2, COL_RAIN);
      }
      break;
    case 4:
      drawCloud(cx, cy - u, u, COL_CLOUD);
      tft.fillTriangle(cx + u, cy + u, cx - u, cy + 3 * u, cx + u / 3, cy + 3 * u, COL_SUN);
      tft.fillTriangle(cx - u / 2, cy + 3 * u, cx + u, cy + 3 * u, cx - u, cy + 5 * u, COL_SUN);
      break;
    case 5:
      drawCloud(cx, cy - u, u, COL_CLOUD);
      for (int k = -1; k <= 1; k++) tft.fillCircle(cx + k * 2 * u, cy + 3 * u + (k == 0 ? u / 2 : 0), max(2, u / 2), COL_SNOW);
      break;
    default:
      for (int i = 0; i < 3; i++)
        tft.fillRoundRect(cx - 4 * u + (i % 2) * u, cy - 2 * u + i * 2 * u, 7 * u, max(2, u / 2 + 1), 2, COL_CLOUD);
      break;
  }
}
int tempNum(float c) { return (int)lroundf(cfg.fahr ? c * 9.0f / 5.0f + 32.0f : c); }
int drawTemp(int x, int y, float c, int font, int size, uint16_t fg, uint16_t bg) {
  String s = String(tempNum(c));
  txtp(s, x, y, font, fg, bg, TL_DATUM, size, 0);
  tft.setTextSize(size);
  int w = tft.textWidth(s, font);
  tft.setTextSize(1);
  int cx = x + w + 3 * size + 2, cy = y + 3 * size + 2;
  tft.drawCircle(cx, cy, size + 1, fg);
  if (size > 1) tft.drawCircle(cx, cy, size, fg);
  txtp(cfg.fahr ? "F" : "C", cx + size + 3, y, font, fg, bg, TL_DATUM, size, 0);
  return w;
}

bool fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return false;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(8000);
  String url = String("https://api.open-meteo.com/v1/forecast?latitude=") + String(LATITUDE, 4) +
               "&longitude=" + String(LONGITUDE, 4) +
               "&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,wind_speed_10m"
               "&daily=weather_code,temperature_2m_max,temperature_2m_min&forecast_days=4&timezone=auto";
  if (!http.begin(client, url)) return false;
  int code = http.GET();
  if (code != HTTP_CODE_OK) { http.end(); return false; }
  String body = http.getString();
  http.end();
  JsonDocument doc;
  if (deserializeJson(doc, body)) return false;
  JsonObject cur = doc["current"];
  if (cur.isNull()) return false;
  wx.t     = cur["temperature_2m"] | 0.0f;
  wx.feels = cur["apparent_temperature"] | 0.0f;
  wx.hum   = cur["relative_humidity_2m"] | 0;
  wx.wind  = cur["wind_speed_10m"] | 0.0f;
  wx.code  = cur["weather_code"] | 0;
  JsonObject d = doc["daily"];
  for (int i = 0; i < 4; i++) {
    wx.hi[i]    = (d["temperature_2m_max"][i] | 0.0f);
    wx.lo[i]    = (d["temperature_2m_min"][i] | 0.0f);
    wx.dcode[i] = (d["weather_code"][i] | 0);
    const char* ds = d["time"][i] | "2000-01-01";
    wx.dow[i] = dowOf(atoi(ds), atoi(ds + 5), atoi(ds + 8));
  }
  wx.ok = true;
  wx.updated = millis();
  if (timeValid()) snprintf(wx.stamp, sizeof(wx.stamp), "%02d:%02d", nowTm.tm_hour, nowTm.tm_min);
  else wx.stamp[0] = 0;
  return true;
}
void weatherTick() {
  if (screen == SCR_GAME) return;
  if (WiFi.status() != WL_CONNECTED) return;
  uint32_t now = millis();
  if (wxTried && (now - wxLastTry) < (wx.ok ? 1200000UL : 30000UL)) return;
  wxTried = true;
  wxLastTry = now;
  fetchWeather();
  if (screen == SCR_WEATHER) wxDraw();
  else if (screen == SCR_HOME) homeHeader(true);
}

// ============================ ICONS (home / games tiles) ====================
void iconClock(int cx, int cy, uint16_t c) {
  for (int r = 18; r <= 20; r++) tft.drawCircle(cx, cy, r, c);
  thickLine(cx, cy, cx, cy - 12, 2, c);
  thickLine(cx, cy, cx + 9, cy + 5, 2, c);
  tft.fillCircle(cx, cy, 2, c);
}
void iconTimer(int cx, int cy, uint16_t c) {
  for (int r = 15; r <= 17; r++) tft.drawCircle(cx, cy + 3, r, c);
  tft.fillRect(cx - 4, cy - 19, 8, 4, c);
  tft.fillRect(cx - 2, cy - 15, 4, 3, c);
  thickLine(cx, cy + 3, cx + 8, cy - 6, 2, c);
}
void iconGear(int cx, int cy, uint16_t c, uint16_t bg) {
  for (int i = 0; i < 8; i++) {
    float a = i * PI / 4.0f;
    tft.fillCircle(cx + (int)(cosf(a) * 16), cy + (int)(sinf(a) * 16), 4, c);
  }
  tft.fillCircle(cx, cy, 14, c);
  tft.fillCircle(cx, cy, 6, bg);
}
void iconPad(int cx, int cy, uint16_t c, uint16_t bg) {
  tft.fillRoundRect(cx - 26, cy - 13, 52, 28, 12, c);
  tft.fillRect(cx - 17, cy - 1, 12, 4, bg);
  tft.fillRect(cx - 13, cy - 5, 4, 12, bg);
  tft.fillCircle(cx + 10, cy + 3, 3, bg);
  tft.fillCircle(cx + 17, cy - 2, 3, bg);
}

// ============================ HOME ==========================================
struct Rect { int x, y, w, h; };
const Rect TILES[5] = {{10, 56, 96, 84}, {112, 56, 96, 84}, {214, 56, 96, 84}, {10, 148, 198, 84}, {214, 148, 96, 84}};
const char* TILE_NAME[5] = {"Clock", "Timer", "Weather", "Games", "Settings"};
String homeKey = "";

void drawTile(int i) {
  int x = TILES[i].x, y = TILES[i].y, w = TILES[i].w, h = TILES[i].h;
  tft.fillRoundRect(x, y, w, h, 12, TH.panel);
  int cx = x + w / 2, cy = y + 34;
  switch (i) {
    case 0: iconClock(cx, cy, TH.accent); break;
    case 1: iconTimer(cx, cy, TH.accent2); break;
    case 2: drawWxIcon(cx, cy, 4, 1); break;
    case 3: iconPad(cx, cy, TH.good, TH.panel); break;
    default: iconGear(cx, cy, TH.text, TH.panel); break;
  }
  txt(TILE_NAME[i], cx, y + h - 16, 2, TH.text, TH.panel, MC_DATUM);
}
void homeInit() {
  tft.fillScreen(TH.bg);
  for (int i = 0; i < 5; i++) drawTile(i);
  homeKey = "";
  homeHeader(true);
}
void homeHeader(bool force) {
  String t = "--:--", d = "Connecting WiFi...";
  if (timeValid()) {
    char b[24];
    if (cfg.h24) snprintf(b, sizeof(b), "%02d:%02d", nowTm.tm_hour, nowTm.tm_min);
    else snprintf(b, sizeof(b), "%d:%02d %s", hour12(nowTm.tm_hour), nowTm.tm_min, nowTm.tm_hour >= 12 ? "PM" : "AM");
    t = String(b);
    snprintf(b, sizeof(b), "%s, %d %s", DOW3[nowTm.tm_wday], nowTm.tm_mday, MON3[nowTm.tm_mon]);
    d = String(b);
  } else if (WiFi.status() == WL_CONNECTED) d = "Syncing time...";
  int bars = WiFi.status() == WL_CONNECTED ? (WiFi.RSSI() / 5) : -99;
  String key = t + "|" + d + "|" + String(wx.ok ? tempNum(wx.t) : -999) + "|" + String(bars) + String(cfg.fahr);
  if (!force && key == homeKey) return;
  homeKey = key;
  txtp(t, 12, 6, 4, TH.text, TH.bg, TL_DATUM, 1, 140);
  txtp(d, 12, 34, 2, TH.dim, TH.bg, TL_DATUM, 1, 150);
  tft.fillRect(SW - 110, 4, 106, 46, TH.bg);
  drawWifi(SW - 22, 8);
  if (wx.ok) {
    drawWxIcon(SW - 92, 30, 2, wmoIcon(wx.code));
    drawTemp(SW - 62, 26, wx.t, 2, 1, TH.text, TH.bg);
  }
}
void homeLoop() {
  static uint32_t last = 0;
  if (millis() - last > 1000) { last = millis(); homeHeader(false); }
  if (!tc.press) return;
  for (int i = 0; i < 5; i++) {
    if (pressIn(TILES[i].x, TILES[i].y, TILES[i].w, TILES[i].h)) {
      beep(1500, 20);
      switch (i) {
        case 0: goScreen(SCR_CLOCK); break;
        case 1: goScreen(SCR_TIMER); break;
        case 2: goScreen(SCR_WEATHER); break;
        case 3: goScreen(SCR_GAMES); break;
        default: goScreen(SCR_SETTINGS); break;
      }
      return;
    }
  }
}

// ============================ CLOCK =========================================
int clkLastSec = -1, clkLastMin = -1;
bool clkNeedFull = true, clkPrevValid = false, clkMsgShown = false;
int clkPH = 0, clkPM = 0, clkPS = 0;
const int ACX = 108, ACY = 134, AR = 92;

void clockMarks() {
  for (int i = 0; i < 60; i++) {
    float a = i * PI / 30.0f, c = cosf(a), s = sinf(a);
    if (i % 5 == 0) {
      thickLine(ACX + (int)(s * (AR - 14)), ACY - (int)(c * (AR - 14)),
                ACX + (int)(s * (AR - 5)),  ACY - (int)(c * (AR - 5)), 2, TH.accent);
    } else {
      tft.drawPixel(ACX + (int)(s * (AR - 6)), ACY - (int)(c * (AR - 6)), TH.dim);
    }
  }
  txt("12", ACX, ACY - AR + 26, 2, TH.text, TH.panel, MC_DATUM);
  txt("3", ACX + AR - 26, ACY, 2, TH.text, TH.panel, MC_DATUM);
  txt("6", ACX, ACY + AR - 26, 2, TH.text, TH.panel, MC_DATUM);
  txt("9", ACX - AR + 26, ACY, 2, TH.text, TH.panel, MC_DATUM);
}
void clockFace() {
  tft.fillCircle(ACX, ACY, AR, TH.panel);
  tft.drawCircle(ACX, ACY, AR, TH.accent);
  tft.drawCircle(ACX, ACY, AR - 1, TH.accent);
  clockMarks();
}
void handLine(float ang, int len, int t, uint16_t c) {
  int x = ACX + (int)(sinf(ang) * len), y = ACY - (int)(cosf(ang) * len);
  if (t <= 1) tft.drawLine(ACX, ACY, x, y, c); else thickLine(ACX, ACY, x, y, t, c);
}
void drawHands(int h, int m, int s, bool erase) {
  float ah = ((h % 12) * 30 + m * 0.5f) * DEG_TO_RAD;
  float am = (m * 6 + s * 0.1f) * DEG_TO_RAD;
  float as = (s * 6) * DEG_TO_RAD;
  handLine(ah, 46, 3, erase ? TH.panel : TH.text);
  handLine(am, 66, 3, erase ? TH.panel : TH.text);
  handLine(as, 76, 1, erase ? TH.panel : TH.accent2);
  if (!erase) {
    tft.fillCircle(ACX, ACY, 5, TH.accent);
    tft.fillCircle(ACX, ACY, 2, TH.bg);
  }
}
void clockInit() {
  tft.fillScreen(TH.bg);
  drawBar("Clock", nullptr, true);
  clkLastSec = -1; clkLastMin = -1; clkNeedFull = true; clkPrevValid = false; clkMsgShown = false;
  if (cfg.analog) clockFace();
  else txt("tap to switch style", 160, 230, 1, TH.dim, TH.bg, MC_DATUM);
}
void clockDigitalTick() {
  int h = nowTm.tm_hour, m = nowTm.tm_min, s = nowTm.tm_sec;
  char b[32];
  if (m != clkLastMin || clkNeedFull) {
    clkLastMin = m;
    if (cfg.h24) snprintf(b, sizeof(b), "%02d:%02d", h, m);
    else snprintf(b, sizeof(b), "%d:%02d", hour12(h), m);
    txtp(b, 160, 92, 7, TH.text, TH.bg, MC_DATUM, 2, 296);
    snprintf(b, sizeof(b), "%s, %d %s %d", DOW3[nowTm.tm_wday], nowTm.tm_mday, MON3[nowTm.tm_mon], nowTm.tm_year + 1900);
    txtp(b, 160, 208, 4, TH.dim, TH.bg, MC_DATUM, 1, 300);
  }
  if (s == 0 || clkNeedFull) tft.fillRoundRect(20, 148, 280, 8, 4, TH.panel);
  int f = (s + 1) * 280 / 60;
  if (f >= 8) tft.fillRoundRect(20, 148, f, 8, 4, TH.accent); else tft.fillRect(20, 148, f, 8, TH.accent);
  snprintf(b, sizeof(b), "%s%02d", cfg.h24 ? "" : (h >= 12 ? "PM  " : "AM  "), s);
  txtp(b, 160, 176, 4, TH.accent2, TH.bg, MC_DATUM, 1, 200);
  clkNeedFull = false;
}
void clockAnalogTick() {
  int h = nowTm.tm_hour, m = nowTm.tm_min, s = nowTm.tm_sec;
  if (clkPrevValid) drawHands(clkPH, clkPM, clkPS, true);
  clockMarks();
  drawHands(h, m, s, false);
  clkPH = h; clkPM = m; clkPS = s; clkPrevValid = true;
  if (m != clkLastMin || clkNeedFull) {
    clkLastMin = m;
    char b[24];
    if (cfg.h24) snprintf(b, sizeof(b), "%02d:%02d", h, m);
    else snprintf(b, sizeof(b), "%d:%02d", hour12(h), m);
    txtp(b, 265, 62, 4, TH.text, TH.bg, MC_DATUM, 1, 100);
    txtp(cfg.h24 ? "" : (h >= 12 ? "PM" : "AM"), 265, 86, 2, TH.accent2, TH.bg, MC_DATUM, 1, 100);
    txtp(DOW3[nowTm.tm_wday], 265, 132, 4, TH.accent, TH.bg, MC_DATUM, 1, 100);
    snprintf(b, sizeof(b), "%d %s %d", nowTm.tm_mday, MON3[nowTm.tm_mon], nowTm.tm_year + 1900);
    txtp(b, 265, 160, 2, TH.dim, TH.bg, MC_DATUM, 1, 104);
    clkNeedFull = false;
  }
}
void clockLoop() {
  if (tc.press && tc.y > BAR + 4) {
    cfg.analog = !cfg.analog;
    saveCfg();
    beep(1300, 20);
    clockInit();
    return;
  }
  if (!timeValid()) {
    if (!clkMsgShown) {
      clkMsgShown = true;
      txt(WiFi.status() == WL_CONNECTED ? "Syncing time..." : "Waiting for WiFi...", cfg.analog ? 265 : 160, cfg.analog ? 110 : 100, 2, TH.dim, TH.bg, MC_DATUM);
    }
    return;
  }
  if (clkMsgShown) { clkMsgShown = false; clockInit(); return; }
  if (nowTm.tm_sec == clkLastSec) return;
  clkLastSec = nowTm.tm_sec;
  if (cfg.analog) clockAnalogTick(); else clockDigitalTick();
}

// ============================ TIMER / STOPWATCH =============================
int tmTab = 0;
uint32_t cdTotal = 300000, cdLeft = 300000, cdLast = 0, cdDoneAt = 0, alarmNext = 0;
bool cdRun = false, cdDone = false;
uint32_t swMs = 0, swLast = 0;
bool swRun = false;
uint32_t lapMs[2] = {0, 0};
int lapNo = 0;
int tmLastA = -1, tmLastB = -1;
bool tmFlash = false;

void timerBackground() {
  uint32_t now = millis();
  if (cdRun) {
    uint32_t dt = now - cdLast;
    cdLast = now;
    if (dt >= cdLeft) {
      cdLeft = 0; cdRun = false; cdDone = true; cdDoneAt = now; alarmNext = 0;
      if (screen != SCR_TIMER) { tmTab = 0; goScreen(SCR_TIMER); }
    } else cdLeft -= dt;
  }
  if (swRun) { swMs += now - swLast; swLast = now; }
}
void tmDrawTabs() {
  button(8, 30, 150, 24, "Timer", tmTab == 0 ? TH.accent : TH.panel, tmTab == 0 ? TH.onacc : TH.text, 2);
  button(162, 30, 150, 24, "Stopwatch", tmTab == 1 ? TH.accent : TH.panel, tmTab == 1 ? TH.onacc : TH.text, 2);
}
void tmDrawMain() {
  char b[16];
  uint16_t c = TH.text;
  if (tmTab == 0) {
    int s = (int)((cdLeft + 999) / 1000);
    snprintf(b, sizeof(b), "%02d:%02d", s / 60, s % 60);
    if (cdDone && tmFlash) c = TH.bad;
  } else {
    uint32_t s = swMs / 1000;
    snprintf(b, sizeof(b), "%02d:%02d", (int)((s / 60) % 100), (int)(s % 60));
    if (swRun) c = TH.text; else c = swMs ? TH.accent2 : TH.text;
  }
  txtp(b, 160, 104, 7, c, TH.bg, MC_DATUM, 2, 296);
}
int tmBarW() { return cdTotal ? (int)((uint64_t)cdLeft * 280 / cdTotal) : 0; }
void tmDrawSub() {
  if (tmTab == 0) {
    int f = tmBarW();
    tft.fillRoundRect(20, 160, 280, 8, 4, TH.panel);
    uint16_t c = cdDone ? TH.bad : TH.accent;
    if (f >= 8) tft.fillRoundRect(20, 160, f, 8, 4, c); else if (f > 0) tft.fillRect(20, 160, f, 8, c);
  } else {
    char b[8];
    snprintf(b, sizeof(b), ".%02d", (int)((swMs / 10) % 100));
    txtp(b, 160, 166, 4, TH.accent, TH.bg, MC_DATUM, 1, 90);
  }
}
void tmDrawLaps() {
  tft.fillRect(0, 178, SW, 32, TH.bg);
  if (tmTab != 1) return;
  for (int i = 0; i < 2; i++) {
    int n = lapNo - i;
    if (n < 1) break;
    char b[32];
    uint32_t ms = lapMs[i];
    snprintf(b, sizeof(b), "Lap %d   %02d:%02d.%02d", n, (int)((ms / 60000) % 100), (int)((ms / 1000) % 60), (int)((ms / 10) % 100));
    txt(b, 160, 186 + i * 15, 2, i == 0 ? TH.text : TH.dim, TH.bg, MC_DATUM);
  }
}
void tmDrawButtons() {
  if (tmTab == 0) {
    bool en = !cdRun && !cdDone;
    uint16_t f = en ? TH.panel : TH.bg, t = en ? TH.text : TH.dim;
    button(4, 180, 74, 26, "-1m", f, t, 2);
    button(84, 180, 74, 26, "+1m", f, t, 2);
    button(164, 180, 74, 26, "-10s", f, t, 2);
    button(244, 180, 74, 26, "+10s", f, t, 2);
    if (cdDone) button(8, 212, 150, 24, "Stop", TH.bad, C565(255, 255, 255), 2);
    else if (cdRun) button(8, 212, 150, 24, "Pause", TH.accent2, TH.onacc, 2);
    else button(8, 212, 150, 24, "Start", TH.good, TH.onacc, 2);
    button(162, 212, 150, 24, "Reset", TH.panel, TH.text, 2);
  } else {
    button(8, 212, 150, 24, swRun ? "Stop" : "Start", swRun ? TH.accent2 : TH.good, TH.onacc, 2);
    button(162, 212, 150, 24, swRun ? "Lap" : "Reset", TH.panel, TH.text, 2);
  }
}
void timerInit() {
  tft.fillScreen(TH.bg);
  drawBar("Timer", nullptr, true);
  tmDrawTabs();
  tmLastA = -1; tmLastB = -1; tmFlash = false;
  tmDrawMain(); tmDrawSub(); tmDrawButtons(); tmDrawLaps();
}
void tmPress() {
  if (pressIn(8, 30, 150, 24)) { tmTab = 0; timerInit(); return; }
  if (pressIn(162, 30, 150, 24)) { tmTab = 1; timerInit(); return; }
  if (tmTab == 0) {
    int delta = 0;
    if (pressIn(4, 180, 74, 26)) delta = -60;
    else if (pressIn(84, 180, 74, 26)) delta = 60;
    else if (pressIn(164, 180, 74, 26)) delta = -10;
    else if (pressIn(244, 180, 74, 26)) delta = 10;
    if (delta && !cdRun && !cdDone) {
      int t = (int)(cdTotal / 1000) + delta;
      t = constrain(t, 10, 5990);
      cdTotal = (uint32_t)t * 1000UL;
      cdLeft = cdTotal;
      beep(1800, 15);
      tmLastA = -1; tmLastB = -1;
    }
    if (pressIn(8, 212, 150, 24)) {
      if (cdDone) { cdDone = false; cdLeft = cdTotal; }
      else if (cdRun) cdRun = false;
      else { if (cdLeft == 0) cdLeft = cdTotal; cdRun = true; cdLast = millis(); }
      beep(1500, 20);
      tmDrawButtons(); tmLastA = -1; tmLastB = -1;
    }
    if (pressIn(162, 212, 150, 24)) {
      cdRun = false; cdDone = false; cdLeft = cdTotal;
      beep(1000, 20);
      tmDrawButtons(); tmLastA = -1; tmLastB = -1;
    }
  } else {
    if (pressIn(8, 212, 150, 24)) {
      swRun = !swRun;
      if (swRun) swLast = millis();
      beep(1500, 20);
      tmDrawButtons(); tmLastA = -1; tmLastB = -1;
    }
    if (pressIn(162, 212, 150, 24)) {
      if (swRun) {
        lapMs[1] = lapMs[0]; lapMs[0] = swMs; lapNo++;
        beep(1900, 20);
        tmDrawLaps();
      } else {
        swMs = 0; lapNo = 0; lapMs[0] = lapMs[1] = 0;
        beep(1000, 20);
        tmDrawButtons(); tmDrawLaps(); tmLastA = -1; tmLastB = -1;
      }
    }
  }
}
void timerLoop() {
  uint32_t now = millis();
  if (tc.press) tmPress();
  if (tmTab == 0) {
    int s = (int)((cdLeft + 999) / 1000);
    bool fl = cdDone && ((now / 450) % 2);
    if (s != tmLastA || fl != tmFlash) { tmLastA = s; tmFlash = fl; tmDrawMain(); }
    int f = tmBarW();
    if (f != tmLastB) { tmLastB = f; tmDrawSub(); }
    if (cdDone) {
      if (now - cdDoneAt > 30000) { cdDone = false; cdLeft = cdTotal; tmDrawButtons(); tmLastA = -1; tmLastB = -1; }
      else if ((int32_t)(now - alarmNext) >= 0) { alarmNext = now + 450; beep(2400, 220); }
    }
  } else {
    int a = (int)(swMs / 1000), b = (int)(swMs / 50);
    if (a != tmLastA) { tmLastA = a; tmDrawMain(); }
    if (b != tmLastB) { tmLastB = b; tmDrawSub(); }
  }
}

// ============================ WEATHER SCREEN ================================
void wxDraw() {
  clearContent();
  drawBar(CITY_NAME, "REF", false);
  if (wx.stamp[0]) txtp(String("Upd ") + wx.stamp, SW - 74, BAR / 2 + 1, 2, TH.dim, TH.panel, MR_DATUM, 1, 100);
  if (!wx.ok) {
    txt("No weather data", 160, 100, 4, TH.dim, TH.bg, MC_DATUM);
    txt(WiFi.status() == WL_CONNECTED ? "Tap REF to retry" : "Waiting for WiFi...", 160, 136, 2, TH.dim, TH.bg, MC_DATUM);
    return;
  }
  drawWxIcon(62, 84, 8, wmoIcon(wx.code));
  drawTemp(124, 36, wx.t, 4, 2, TH.text, TH.bg);
  txt(wmoText(wx.code), 124, 96, 4, TH.accent, TH.bg, TL_DATUM);
  txt(String("H ") + tempNum(wx.hi[0]) + "   L " + tempNum(wx.lo[0]), 124, 126, 2, TH.dim, TH.bg, TL_DATUM);

  const char* labels[3] = {"FEELS LIKE", "HUMIDITY", "WIND"};
  for (int i = 0; i < 3; i++) {
    int x = 8 + i * 104;
    tft.fillRoundRect(x, 146, 96, 38, 9, TH.panel);
    txt(labels[i], x + 8, 152, 1, TH.dim, TH.panel, TL_DATUM);
    if (i == 0) drawTemp(x + 8, 165, wx.feels, 2, 1, TH.text, TH.panel);
    else if (i == 1) txt(String(wx.hum) + "%", x + 8, 165, 2, TH.text, TH.panel, TL_DATUM);
    else txt(String((int)lroundf(wx.wind)) + " km/h", x + 8, 165, 2, TH.text, TH.panel, TL_DATUM);
  }
  for (int i = 1; i <= 3; i++) {
    int x = 8 + (i - 1) * 104;
    tft.fillRoundRect(x, 190, 96, 46, 9, TH.panel);
    txt(DOW3[wx.dow[i]], x + 8, 197, 1, TH.accent2, TH.panel, TL_DATUM);
    drawWxIcon(x + 26, 216, 3, wmoIcon(wx.dcode[i]));
    txt(String(tempNum(wx.hi[i])), x + 60, 198, 2, TH.text, TH.panel, TL_DATUM);
    txt(String(tempNum(wx.lo[i])), x + 60, 216, 2, TH.dim, TH.panel, TL_DATUM);
  }
}
void wxRefresh() {
  txtp("Updating...", SW - 74, BAR / 2 + 1, 2, TH.accent, TH.panel, MR_DATUM, 1, 100);
  wxTried = true; wxLastTry = millis();
  fetchWeather();
  wxDraw();
}
void weatherInit() {
  tft.fillScreen(TH.bg);
  wxDraw();
  if (WiFi.status() == WL_CONNECTED && (!wx.ok || millis() - wx.updated > 600000UL)) wxRefresh();
}
void weatherLoop() {
  if (pressIn(SW - 70, 0, 66, BAR + 2)) { beep(1500, 20); wxRefresh(); }
}

// ============================ SETTINGS ======================================
const int SET_Y0 = 32, SET_STEP = 38, SET_H = 34;
bool sliderDrag = false;

void setRowBase(int i, const char* label) {
  int y = SET_Y0 + i * SET_STEP;
  tft.fillRoundRect(6, y, 308, SET_H, 9, TH.panel);
  txt(label, 16, y + SET_H / 2 + 1, 2, TH.text, TH.panel, ML_DATUM);
}
void toggleSwitch(int x, int y, bool on) {
  tft.fillRoundRect(x, y, 46, 22, 11, on ? TH.accent : TH.dim);
  tft.fillCircle(on ? x + 35 : x + 11, y + 11, 8, C565(255, 255, 255));
}
void setRowTheme() {
  setRowBase(0, "Theme");
  int y = SET_Y0;
  button(170, y + 4, 30, 26, "<", TH.bg, TH.accent, 2);
  button(276, y + 4, 30, 26, ">", TH.bg, TH.accent, 2);
  txt(TH.name, 238, y + SET_H / 2 + 1, 2, TH.text, TH.panel, MC_DATUM);
}
void setRowBright() {
  setRowBase(1, "Brightness");
  int y = SET_Y0 + SET_STEP;
  int kx = 140 + (int)(cfg.bright - 10) * 150 / 245;
  tft.fillRoundRect(140, y + 13, 150, 8, 4, TH.bg);
  tft.fillRoundRect(140, y + 13, kx - 140 + 4, 8, 4, TH.accent);
  tft.fillCircle(kx, y + 17, 10, TH.accent);
  tft.fillCircle(kx, y + 17, 4, TH.onacc);
}
void setRowH24() { setRowBase(2, "24-hour clock"); toggleSwitch(256, SET_Y0 + 2 * SET_STEP + 6, cfg.h24); }
void setRowSound() { setRowBase(3, "Sound"); toggleSwitch(256, SET_Y0 + 3 * SET_STEP + 6, cfg.sound); }
void setRowTemp() {
  setRowBase(4, "Temperature");
  int y = SET_Y0 + 4 * SET_STEP + 5;
  button(226, y, 36, 24, "C", cfg.fahr ? TH.bg : TH.accent, cfg.fahr ? TH.text : TH.onacc, 2);
  button(266, y, 36, 24, "F", cfg.fahr ? TH.accent : TH.bg, cfg.fahr ? TH.onacc : TH.text, 2);
}
void setInfo() {
  String s;
  if (WiFi.status() == WL_CONNECTED) s = String("WiFi ") + WiFi.localIP().toString() + "   " + String(WiFi.RSSI()) + " dBm";
  else s = String("WiFi: connecting...");
  txtp(s, 160, 230, 1, TH.dim, TH.bg, MC_DATUM, 1, 300);
}
void settingsInit() {
  tft.fillScreen(TH.bg);
  drawBar("Settings", nullptr, false);
  setRowTheme(); setRowBright(); setRowH24(); setRowSound(); setRowTemp();
  setInfo();
}
void settingsLoop() {
  static uint32_t lastInfo = 0;
  if (tc.press) {
    if (pressIn(170, SET_Y0 + 2, 34, 30)) { cfg.theme = (cfg.theme + NTHEMES - 1) % NTHEMES; saveCfg(); beep(1200, 20); settingsInit(); return; }
    if (pressIn(272, SET_Y0 + 2, 38, 30)) { cfg.theme = (cfg.theme + 1) % NTHEMES; saveCfg(); beep(1200, 20); settingsInit(); return; }
    if (pressIn(120, SET_Y0 + SET_STEP, 192, SET_H)) sliderDrag = true;
    if (pressIn(236, SET_Y0 + 2 * SET_STEP, 78, SET_H)) { cfg.h24 = !cfg.h24; saveCfg(); beep(1400, 20); setRowH24(); }
    if (pressIn(236, SET_Y0 + 3 * SET_STEP, 78, SET_H)) { cfg.sound = !cfg.sound; saveCfg(); beep(1400, 20); setRowSound(); }
    if (pressIn(222, SET_Y0 + 4 * SET_STEP, 40, SET_H)) { cfg.fahr = false; saveCfg(); beep(1400, 20); setRowTemp(); }
    if (pressIn(264, SET_Y0 + 4 * SET_STEP, 44, SET_H)) { cfg.fahr = true; saveCfg(); beep(1400, 20); setRowTemp(); }
  }
  if (sliderDrag) {
    if (tc.down) {
      int v = constrain((int)map(tc.x, 140, 290, 10, 255), 10, 255);
      if (v != cfg.bright) { cfg.bright = v; setBacklight(v); setRowBright(); }
    }
    if (tc.release) { sliderDrag = false; saveCfg(); }
  }
  if (millis() - lastInfo > 2500) { lastInfo = millis(); setInfo(); }
}

// ============================ GAME COMMON ===================================
const char* GAME_TITLE[8] = {"X & O", "Snake", "Memory", "Whack", "Reflex", "2048", "Bricks", "Simon"};
uint32_t gameOverAt = 0;

void gameScoreText(const String &s) { txtp(s, SW - 74, BAR / 2 + 1, 2, TH.text, TH.panel, MR_DATUM, 1, 110); }
void overlay(const char* title, const String &sub, uint16_t col) {
  int w = 220, h = 84, x = (SW - w) / 2, y = (SH + BAR - h) / 2;
  tft.fillRoundRect(x, y, w, h, 12, TH.panel);
  tft.drawRoundRect(x, y, w, h, 12, col);
  tft.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 11, col);
  txt(title, SW / 2, y + 24, 4, col, TH.panel, MC_DATUM);
  txt(sub, SW / 2, y + 52, 2, TH.text, TH.panel, MC_DATUM);
  txt("tap to play again", SW / 2, y + 72, 1, TH.dim, TH.panel, MC_DATUM);
  gameOverAt = millis();
}
bool tapAfterOver() { return tc.press && tc.y > BAR && (millis() - gameOverAt) > 350; }

// ---------------------------- 0: TIC-TAC-TOE -------------------------------
const uint8_t TLINES[8][3] = {{0,1,2},{3,4,5},{6,7,8},{0,3,6},{1,4,7},{2,5,8},{0,4,8},{2,4,6}};
const int TX0 = 12, TY0 = 34, TCELL = 66;
int ttt[9], tttState = 0, tttWin = -1, tttW = 0, tttL = 0, tttD = 0;

int tttResult() {
  tttWin = -1;
  for (int i = 0; i < 8; i++) {
    int a = ttt[TLINES[i][0]], b = ttt[TLINES[i][1]], c = ttt[TLINES[i][2]];
    if (a && a == b && b == c) { tttWin = i; return a; }
  }
  for (int i = 0; i < 9; i++) if (!ttt[i]) return 0;
  return 3;
}
void tttCell(int i, bool hl) {
  int x = TX0 + (i % 3) * TCELL, y = TY0 + (i / 3) * TCELL;
  tft.fillRoundRect(x + 2, y + 2, TCELL - 4, TCELL - 4, 9, TH.panel);
  int cx = x + TCELL / 2, cy = y + TCELL / 2;
  if (ttt[i] == 1) {
    thickLine(cx - 15, cy - 15, cx + 15, cy + 15, 5, TH.accent);
    thickLine(cx - 15, cy + 15, cx + 15, cy - 15, 5, TH.accent);
  } else if (ttt[i] == 2) {
    for (int r = 15; r <= 19; r++) tft.drawCircle(cx, cy, r, TH.accent2);
  }
  if (hl) {
    tft.drawRoundRect(x + 2, y + 2, TCELL - 4, TCELL - 4, 9, TH.good);
    tft.drawRoundRect(x + 3, y + 3, TCELL - 6, TCELL - 6, 8, TH.good);
  }
}
void tttStatus(const String &s, uint16_t c) { txtp(s, 268, 56, 2, c, TH.bg, MC_DATUM, 1, 100); }
void tttScore() {
  txtp(String("You  ") + tttW, 268, 100, 2, TH.accent, TH.bg, MC_DATUM, 1, 100);
  txtp(String("CPU  ") + tttL, 268, 122, 2, TH.accent2, TH.bg, MC_DATUM, 1, 100);
  txtp(String("Draw ") + tttD, 268, 144, 2, TH.dim, TH.bg, MC_DATUM, 1, 100);
}
void tttNew() {
  memset(ttt, 0, sizeof(ttt));
  tttState = 0; tttWin = -1;
  clearContent();
  for (int i = 0; i < 9; i++) tttCell(i, false);
  tttStatus("Your turn", TH.text);
  tttScore();
  txt("You = X", 268, 180, 1, TH.accent, TH.bg, MC_DATUM);
  txt("CPU = O", 268, 194, 1, TH.accent2, TH.bg, MC_DATUM);
}
int tttAI() {
  for (int p = 0; p < 2; p++) {
    int who = (p == 0) ? 2 : 1;
    if (p == 1 && random(100) < 15) continue;          // sometimes "forgets" to block
    for (int i = 0; i < 8; i++) {
      int cnt = 0, e = -1;
      for (int k = 0; k < 3; k++) {
        int idx = TLINES[i][k];
        if (ttt[idx] == who) cnt++; else if (!ttt[idx]) e = idx;
      }
      if (cnt == 2 && e >= 0) return e;
    }
  }
  if (!ttt[4]) return 4;
  int corners[4] = {0, 2, 6, 8}, cl[4], n = 0;
  for (int i = 0; i < 4; i++) if (!ttt[corners[i]]) cl[n++] = corners[i];
  if (n) return cl[random(n)];
  int all[9]; n = 0;
  for (int i = 0; i < 9; i++) if (!ttt[i]) all[n++] = i;
  return n ? all[random(n)] : -1;
}
bool tttFinish() {
  int r = tttResult();
  if (!r) return false;
  tttState = 1;
  if (r == 3) { tttD++; tttStatus("Draw!", TH.dim); beep(500, 150); }
  else {
    for (int k = 0; k < 3; k++) tttCell(TLINES[tttWin][k], true);
    if (r == 1) { tttW++; hi[0]++; hiDirty = true; tttStatus("You win!", TH.good); beep(1800, 200); }
    else { tttL++; tttStatus("CPU wins", TH.bad); beep(300, 250); }
  }
  tttScore();
  txt("tap to continue", 268, 166, 1, TH.dim, TH.bg, MC_DATUM);
  return true;
}
void tttInit() { tttW = tttL = tttD = 0; tttNew(); }
void tttLoop() {
  if (!tc.press || tc.y < TY0) return;
  if (tttState == 1) { tttNew(); return; }
  if (tc.x < TX0 || tc.x >= TX0 + 3 * TCELL || tc.y >= TY0 + 3 * TCELL) return;
  int i = ((tc.y - TY0) / TCELL) * 3 + (tc.x - TX0) / TCELL;
  if (ttt[i]) return;
  ttt[i] = 1; tttCell(i, false); beep(880, 40);
  if (tttFinish()) return;
  tttStatus("Thinking...", TH.dim);
  delay(260);
  int m = tttAI();
  if (m >= 0) { ttt[m] = 2; tttCell(m, false); beep(600, 40); }
  if (tttFinish()) return;
  tttStatus("Your turn", TH.text);
}

// ---------------------------- 1: SNAKE -------------------------------------
const int SN_C = 16, SN_COLS = 20, SN_ROWS = 13, SN_Y = 28;
int snx[SN_COLS * SN_ROWS + 2], sny[SN_COLS * SN_ROWS + 2];
int snLen, snDir, snNext, snFx, snFy, snState, snScore, snAx, snAy;
uint32_t snLast;
const int SDX[4] = {0, 1, 0, -1}, SDY[4] = {-1, 0, 1, 0};

void snCell(int x, int y, uint16_t c) { tft.fillRoundRect(x * SN_C + 1, SN_Y + y * SN_C + 1, SN_C - 2, SN_C - 2, 4, c); }
void snClear(int x, int y) { tft.fillRect(x * SN_C, SN_Y + y * SN_C, SN_C, SN_C, TH.bg); }
void snFood() {
  for (int tries = 0; tries < 400; tries++) {
    int x = random(SN_COLS), y = random(SN_ROWS);
    bool ok = true;
    for (int i = 0; i < snLen; i++) if (snx[i] == x && sny[i] == y) { ok = false; break; }
    if (ok) { snFx = x; snFy = y; break; }
  }
  tft.fillCircle(snFx * SN_C + SN_C / 2, SN_Y + snFy * SN_C + SN_C / 2, 6, TH.bad);
}
void snInit() {
  clearContent();
  snLen = 3; snDir = 1; snNext = 1; snScore = 0; snState = 0;
  for (int i = 0; i < snLen; i++) { snx[i] = 10 - i; sny[i] = 6; }
  for (int i = 1; i < snLen; i++) snCell(snx[i], sny[i], TH.good);
  snCell(snx[0], sny[0], TH.accent);
  snFood();
  gameScoreText("Score 0");
  txt("swipe to steer - tap to start", SW / 2, SN_Y + 9 * SN_C, 2, TH.dim, TH.bg, MC_DATUM);
}
void snOver() {
  snState = 2;
  submitScore(1, snScore); flushHi();
  beep(200, 400);
  overlay("Game Over", String("Score ") + snScore + "   Best " + hi[1], TH.bad);
}
void snStep() {
  if (snNext != (snDir + 2) % 4) snDir = snNext;
  int nx = (snx[0] + SDX[snDir] + SN_COLS) % SN_COLS;
  int ny = (sny[0] + SDY[snDir] + SN_ROWS) % SN_ROWS;
  bool grow = (nx == snFx && ny == snFy);
  int chk = grow ? snLen : snLen - 1;
  for (int i = 0; i < chk; i++) if (snx[i] == nx && sny[i] == ny) { snOver(); return; }
  int tx = snx[snLen - 1], ty = sny[snLen - 1];
  if (grow && snLen < SN_COLS * SN_ROWS) snLen++;
  for (int i = snLen - 1; i > 0; i--) { snx[i] = snx[i - 1]; sny[i] = sny[i - 1]; }
  snx[0] = nx; sny[0] = ny;
  if (!grow) snClear(tx, ty);
  snCell(snx[1], sny[1], TH.good);
  snCell(nx, ny, TH.accent);
  if (grow) {
    snScore++; beep(1500, 40);
    gameScoreText(String("Score ") + snScore);
    snFood();
  }
}
void snLoop() {
  if (snState == 2) { if (tapAfterOver()) snInit(); return; }
  if (tc.press) {
    snAx = tc.x; snAy = tc.y;
    if (snState == 0) {
      snState = 1; snLast = millis();
      tft.fillRect(0, SN_Y + 8 * SN_C, SW, SN_C * 2, TH.bg);
      snCell(snx[0], sny[0], TH.accent);
      for (int i = 1; i < snLen; i++) snCell(snx[i], sny[i], TH.good);
      tft.fillCircle(snFx * SN_C + SN_C / 2, SN_Y + snFy * SN_C + SN_C / 2, 6, TH.bad);
    }
  }
  if (tc.down) {
    int dx = tc.x - snAx, dy = tc.y - snAy;
    if (abs(dx) > 16 || abs(dy) > 16) {
      int nd = (abs(dx) > abs(dy)) ? (dx > 0 ? 1 : 3) : (dy > 0 ? 2 : 0);
      if (nd != (snDir + 2) % 4) snNext = nd;
      snAx = tc.x; snAy = tc.y;
    }
  }
  if (snState == 1) {
    int iv = max(70, 150 - snScore * 3);
    if (millis() - snLast >= (uint32_t)iv) { snLast = millis(); snStep(); }
  }
}

// ---------------------------- 2: MEMORY ------------------------------------
const int MM_X0 = 12, MM_Y0 = 32, MM_W = 68, MM_H = 44, MM_G = 8;
const uint16_t MMC[8] = {C565(255,90,90), C565(255,170,50), C565(250,225,70), C565(90,220,120),
                         C565(60,200,230), C565(90,130,255), C565(190,110,255), C565(255,110,190)};
uint8_t mmCard[16];
bool mmUp[16], mmDone[16];
int mmFirst, mmSecond, mmMoves, mmPairs, mmState;
uint32_t mmCheckAt;

void mmSymbol(int cx, int cy, int id, uint16_t col, uint16_t bg) {
  int s = 13;
  switch (id) {
    case 0: tft.fillCircle(cx, cy, s, col); break;
    case 1: tft.fillRoundRect(cx - s, cy - s, 2 * s, 2 * s, 4, col); break;
    case 2: tft.fillTriangle(cx, cy - s, cx - s, cy + s, cx + s, cy + s, col); break;
    case 3:
      tft.fillTriangle(cx, cy - s, cx - s, cy, cx + s, cy, col);
      tft.fillTriangle(cx, cy + s, cx - s, cy, cx + s, cy, col);
      break;
    case 4: tft.fillCircle(cx, cy, s, col); tft.fillCircle(cx, cy, s - 5, bg); break;
    case 5: tft.fillRect(cx - 3, cy - s, 6, 2 * s, col); tft.fillRect(cx - s, cy - 3, 2 * s, 6, col); break;
    case 6: tft.fillRoundRect(cx - s, cy - s, 2 * s, 2 * s, 3, col); tft.fillRect(cx - s + 4, cy - s + 4, 2 * s - 8, 2 * s - 8, bg); break;
    default:
      tft.fillRect(cx - s, cy - s, 2 * s, 5, col);
      tft.fillRect(cx - s, cy - 2, 2 * s, 5, col);
      tft.fillRect(cx - s, cy + s - 5, 2 * s, 5, col);
      break;
  }
}
void mmDraw(int i) {
  int x = MM_X0 + (i % 4) * (MM_W + MM_G), y = MM_Y0 + (i / 4) * (MM_H + MM_G);
  int cx = x + MM_W / 2, cy = y + MM_H / 2;
  if (mmDone[i]) {
    tft.fillRoundRect(x, y, MM_W, MM_H, 8, TH.panel);
    tft.drawRoundRect(x, y, MM_W, MM_H, 8, TH.good);
    mmSymbol(cx, cy, mmCard[i], MMC[mmCard[i]], TH.panel);
  } else if (mmUp[i]) {
    tft.fillRoundRect(x, y, MM_W, MM_H, 8, TH.panel);
    tft.drawRoundRect(x, y, MM_W, MM_H, 8, TH.accent2);
    mmSymbol(cx, cy, mmCard[i], MMC[mmCard[i]], TH.panel);
  } else {
    tft.fillRoundRect(x, y, MM_W, MM_H, 8, TH.accent);
    txt("?", cx, cy + 1, 4, TH.onacc, TH.accent, MC_DATUM);
  }
}
void mmInit() {
  clearContent();
  for (int i = 0; i < 16; i++) { mmCard[i] = i / 2; mmUp[i] = false; mmDone[i] = false; }
  for (int i = 15; i > 0; i--) { int j = random(i + 1); uint8_t t = mmCard[i]; mmCard[i] = mmCard[j]; mmCard[j] = t; }
  mmFirst = mmSecond = -1; mmMoves = 0; mmPairs = 0; mmState = 0;
  for (int i = 0; i < 16; i++) mmDraw(i);
  gameScoreText("Moves 0");
}
void mmLoop() {
  if (mmState == 1) { if (tapAfterOver()) mmInit(); return; }
  if (mmSecond >= 0 && (int32_t)(millis() - mmCheckAt) >= 0) {
    if (mmCard[mmFirst] == mmCard[mmSecond]) {
      mmDone[mmFirst] = mmDone[mmSecond] = true; mmPairs++; beep(1600, 80);
    } else {
      mmUp[mmFirst] = mmUp[mmSecond] = false; beep(300, 60);
    }
    mmDraw(mmFirst); mmDraw(mmSecond);
    mmFirst = mmSecond = -1;
    if (mmPairs == 8) {
      mmState = 1; submitScore(2, mmMoves); flushHi();
      beep(2000, 250);
      overlay("Solved!", String(mmMoves) + " moves   Best " + hi[2], TH.good);
    }
    return;
  }
  if (!tc.press || mmSecond >= 0) return;
  int rx = tc.x - MM_X0, ry = tc.y - MM_Y0;
  if (rx < 0 || ry < 0) return;
  int col = rx / (MM_W + MM_G), row = ry / (MM_H + MM_G);
  if (col > 3 || row > 3) return;
  if (rx % (MM_W + MM_G) >= MM_W || ry % (MM_H + MM_G) >= MM_H) return;
  int i = row * 4 + col;
  if (mmUp[i] || mmDone[i]) return;
  mmUp[i] = true; mmDraw(i); beep(1100, 30);
  if (mmFirst < 0) mmFirst = i;
  else {
    mmSecond = i; mmMoves++;
    gameScoreText(String("Moves ") + mmMoves);
    mmCheckAt = millis() + 650;
  }
}

// ---------------------------- 3: WHACK-A-MOLE ------------------------------
const int WK_X[3] = {64, 160, 256}, WK_Y[3] = {74, 140, 206};
int wkMole, wkScore, wkState, wkLastHole, wkSecShown;
uint32_t wkUntil, wkNextAt, wkEnd;

void wkHole(int i, bool mole) {
  int cx = WK_X[i % 3], cy = WK_Y[i / 3];
  tft.fillRect(cx - 36, cy - 34, 72, 66, TH.bg);
  tft.fillEllipse(cx, cy + 12, 32, 11, C565(55, 34, 22));
  if (mole) {
    tft.fillCircle(cx, cy - 4, 22, C565(166, 112, 70));
    tft.fillCircle(cx - 8, cy - 9, 3, C565(20, 14, 10));
    tft.fillCircle(cx + 8, cy - 9, 3, C565(20, 14, 10));
    tft.fillCircle(cx, cy, 5, C565(255, 150, 160));
  }
}
void wkInit() {
  clearContent();
  for (int i = 0; i < 9; i++) wkHole(i, false);
  wkMole = -1; wkScore = 0; wkState = 0; wkLastHole = -1; wkSecShown = -1;
  gameScoreText("0 | 30s");
  txt("tap to start (30s)", SW / 2, 40, 2, TH.dim, TH.bg, MC_DATUM);
}
void wkLoop() {
  uint32_t now = millis();
  if (wkState == 2) { if (tapAfterOver()) wkInit(); return; }
  if (wkState == 0) {
    if (tc.press && tc.y > BAR) {
      wkState = 1; wkEnd = now + 30000UL; wkNextAt = now + 500;
      tft.fillRect(0, BAR + 2, SW, 22, TH.bg);
    }
    return;
  }
  int left = (int)((wkEnd - now + 999) / 1000);
  if ((int32_t)(now - wkEnd) >= 0) {
    if (wkMole >= 0) { wkHole(wkMole, false); wkMole = -1; }
    wkState = 2; submitScore(3, wkScore); flushHi();
    beep(700, 300);
    overlay("Time's up!", String("Score ") + wkScore + "   Best " + hi[3], TH.accent);
    return;
  }
  if (left != wkSecShown) { wkSecShown = left; gameScoreText(String(wkScore) + " | " + left + "s"); }
  if (wkMole < 0 && (int32_t)(now - wkNextAt) >= 0) {
    int h; do { h = random(9); } while (h == wkLastHole);
    wkMole = h; wkLastHole = h; wkHole(h, true);
    wkUntil = now + max(420, 950 - wkScore * 18);
  } else if (wkMole >= 0 && (int32_t)(now - wkUntil) >= 0) {
    wkHole(wkMole, false); wkMole = -1; wkNextAt = now + random(150, 420);
  }
  if (tc.press && wkMole >= 0) {
    int dx = tc.x - WK_X[wkMole % 3], dy = tc.y - WK_Y[wkMole / 3];
    if (dx * dx + dy * dy < 34 * 34) {
      wkScore++; beep(1700, 40);
      wkHole(wkMole, false); wkMole = -1; wkNextAt = now + random(100, 320);
      gameScoreText(String(wkScore) + " | " + left + "s");
    }
  }
}

// ---------------------------- 4: REFLEX ------------------------------------
int rxState;
uint32_t rxAt, rxT0, rxLast;
void rxFill(uint16_t col, const String &a, const String &b, uint16_t fg) {
  tft.fillRect(0, BAR, SW, SH - BAR, col);
  txt(a, SW / 2, 110, 4, fg, col, MC_DATUM);
  txt(b, SW / 2, 150, 2, fg, col, MC_DATUM);
}
void rxInit() {
  rxState = 0;
  rxFill(TH.panel, "Reflex Test", hi[4] ? String("Best ") + hi[4] + " ms   -  tap to start" : String("Tap to start"), TH.text);
  gameScoreText("");
}
void rxLoop() {
  uint32_t now = millis();
  if (rxState == 1 && (int32_t)(now - rxAt) >= 0) {
    rxState = 2;
    rxFill(C565(40, 190, 90), "TAP NOW!", "", C565(255, 255, 255));
    rxT0 = millis();
    return;
  }
  if (!tc.press || tc.y <= BAR) return;
  if (rxState == 0 || rxState == 3 || rxState == 4) {
    rxState = 1;
    rxAt = now + random(1500, 4500);
    rxFill(C565(200, 50, 50), "Wait for green...", "", C565(255, 255, 255));
  } else if (rxState == 1) {
    rxState = 4; beep(250, 200);
    rxFill(TH.panel, "Too early!", "tap to try again", TH.bad);
  } else if (rxState == 2) {
    uint32_t t = millis() - rxT0;
    rxState = 3;
    submitScore(4, t); flushHi(); beep(1800, 60);
    rxFill(TH.panel, String(t) + " ms", String("Best ") + hi[4] + " ms   -  tap to retry", TH.accent);
    gameScoreText(String(t) + " ms");
  }
}

// ---------------------------- 5: 2048 --------------------------------------
const int G48X = 10, G48Y = 31, G48T = 44, G48G = 5;
int g48[4][4], g48Score, g48Ax, g48Ay;
bool g48Over, g48Consumed;

uint16_t g48Col(int v) {
  switch (v) {
    case 0: return TH.bg;
    case 2: return C565(238, 228, 218);
    case 4: return C565(237, 224, 200);
    case 8: return C565(242, 177, 121);
    case 16: return C565(245, 149, 99);
    case 32: return C565(246, 124, 95);
    case 64: return C565(246, 94, 59);
    case 128: return C565(237, 207, 114);
    case 256: return C565(237, 204, 97);
    case 512: return C565(237, 200, 80);
    case 1024: return C565(237, 197, 63);
    case 2048: return C565(237, 194, 46);
    default: return C565(60, 58, 50);
  }
}
void g48Tile(int r, int c) {
  int x = G48X + c * (G48T + G48G), y = G48Y + r * (G48T + G48G), v = g48[r][c];
  uint16_t col = g48Col(v);
  tft.fillRoundRect(x, y, G48T, G48T, 6, col);
  if (v) {
    uint16_t fg = (v <= 4) ? C565(110, 100, 92) : C565(255, 255, 255);
    txtp(String(v), x + G48T / 2, y + G48T / 2 + 1, 2, fg, col, MC_DATUM, (v < 100) ? 2 : 1, 0);
  }
}
void g48Side() {
  txt("SCORE", 222, 40, 1, TH.dim, TH.bg, TL_DATUM);
  txtp(String(g48Score), 222, 52, 4, TH.text, TH.bg, TL_DATUM, 1, 92);
  txt("BEST", 222, 96, 1, TH.dim, TH.bg, TL_DATUM);
  uint32_t b = (hi[5] > (uint32_t)g48Score) ? hi[5] : (uint32_t)g48Score;
  txtp(String(b), 222, 108, 4, TH.accent, TH.bg, TL_DATUM, 1, 92);
}
void g48All() { for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) g48Tile(r, c); }
void g48Spawn() {
  int cells[16], n = 0;
  for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) if (!g48[r][c]) cells[n++] = r * 4 + c;
  if (!n) return;
  int k = cells[random(n)];
  g48[k / 4][k % 4] = (random(10) < 9) ? 2 : 4;
}
int &g48Ref(int dir, int i, int j) {
  switch (dir) {
    case 0: return g48[i][j];
    case 1: return g48[i][3 - j];
    case 2: return g48[j][i];
    default: return g48[3 - j][i];
  }
}
bool g48Move(int dir) {
  bool moved = false;
  for (int i = 0; i < 4; i++) {
    int line[4], out[4] = {0, 0, 0, 0}, k = 0;
    bool cm = false;
    for (int j = 0; j < 4; j++) line[j] = g48Ref(dir, i, j);
    for (int j = 0; j < 4; j++) {
      int v = line[j];
      if (!v) continue;
      if (k > 0 && cm && out[k - 1] == v) { out[k - 1] *= 2; g48Score += out[k - 1]; cm = false; }
      else { out[k++] = v; cm = true; }
    }
    for (int j = 0; j < 4; j++) {
      if (g48Ref(dir, i, j) != out[j]) { moved = true; g48Ref(dir, i, j) = out[j]; }
    }
  }
  return moved;
}
bool g48CanMove() {
  for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) {
    if (!g48[r][c]) return true;
    if (c < 3 && g48[r][c] == g48[r][c + 1]) return true;
    if (r < 3 && g48[r][c] == g48[r + 1][c]) return true;
  }
  return false;
}
void g48Init() {
  clearContent();
  memset(g48, 0, sizeof(g48));
  g48Score = 0; g48Over = false; g48Consumed = false;
  tft.fillRoundRect(G48X - 5, G48Y - 5, 4 * G48T + 3 * G48G + 10, 4 * G48T + 3 * G48G + 10, 8, TH.panel);
  g48Spawn(); g48Spawn();
  g48All(); g48Side();
  txt("Swipe to", 222, 160, 2, TH.dim, TH.bg, TL_DATUM);
  txt("slide tiles", 222, 178, 2, TH.dim, TH.bg, TL_DATUM);
}
void g48Loop() {
  if (g48Over) { if (tapAfterOver()) g48Init(); return; }
  if (tc.press) { g48Ax = tc.x; g48Ay = tc.y; g48Consumed = false; }
  if (tc.release) g48Consumed = false;
  if (tc.down && !g48Consumed) {
    int dx = tc.x - g48Ax, dy = tc.y - g48Ay;
    if (abs(dx) > 24 || abs(dy) > 24) {
      g48Consumed = true;
      int dir = (abs(dx) > abs(dy)) ? (dx > 0 ? 1 : 0) : (dy > 0 ? 3 : 2);
      if (g48Move(dir)) {
        beep(900, 20);
        g48Spawn(); g48All(); g48Side();
        if (!g48CanMove()) {
          g48Over = true;
          submitScore(5, g48Score); flushHi();
          beep(250, 400);
          overlay("Game Over", String("Score ") + g48Score, TH.bad);
        }
      }
    }
  }
}

// ---------------------------- 6: BREAKOUT ----------------------------------
const int BR_COLS = 8, BR_ROWS = 5, BR_BW = 36, BR_BH = 12, BR_GAP = 2, BR_X0 = 8, BR_Y0 = 40, BR_PY = 222, BR_PW = 54;
const uint16_t BRC[5] = {C565(255,90,90), C565(255,160,60), C565(250,220,70), C565(90,220,120), C565(80,170,255)};
bool brBrick[BR_ROWS][BR_COLS];
float brX, brY, brVX, brVY, brPX, brSpeed;
int brLives, brScore, brLeft, brState, brLevel, brOldPX, brDrawX, brDrawY;
uint32_t brLast;

void brDrawBrick(int r, int c, bool on) {
  int x = BR_X0 + c * (BR_BW + BR_GAP), y = BR_Y0 + r * (BR_BH + BR_GAP);
  if (on) tft.fillRoundRect(x, y, BR_BW, BR_BH, 3, BRC[r]); else tft.fillRect(x, y, BR_BW, BR_BH, TH.bg);
}
void brBricks() {
  for (int r = 0; r < BR_ROWS; r++) for (int c = 0; c < BR_COLS; c++) { brBrick[r][c] = true; brDrawBrick(r, c, true); }
  brLeft = BR_ROWS * BR_COLS;
}
void brStatus() { gameScoreText(String(brScore) + "  x" + brLives); }
void brBallReady() {
  brState = 0;
  brX = brPX; brY = BR_PY - 5;
  txt("tap to launch", SW / 2, 150, 2, TH.dim, TH.bg, MC_DATUM);
}
void brPaddle() { tft.fillRoundRect((int)brPX - BR_PW / 2, BR_PY, BR_PW, 8, 4, TH.accent); }
void brInit() {
  clearContent();
  brLives = 3; brScore = 0; brLevel = 1; brSpeed = 3.4f; brPX = SW / 2; brOldPX = (int)brPX; brDrawX = -50; brDrawY = -50;
  brBricks();
  brPaddle();
  brBallReady();
  brStatus();
}
bool brHit(float px, float py) {
  int lx = (int)px - BR_X0, ly = (int)py - BR_Y0;
  if (lx < 0 || ly < 0) return false;
  int c = lx / (BR_BW + BR_GAP), r = ly / (BR_BH + BR_GAP);
  if (c >= BR_COLS || r >= BR_ROWS) return false;
  if (lx % (BR_BW + BR_GAP) >= BR_BW || ly % (BR_BH + BR_GAP) >= BR_BH) return false;
  if (!brBrick[r][c]) return false;
  brBrick[r][c] = false; brDrawBrick(r, c, false);
  brScore += 10; brLeft--; beep(1200 + r * 150, 25);
  brStatus();
  return true;
}
void brLoop() {
  if (brState == 2) { if (tapAfterOver()) brInit(); return; }
  uint32_t now = millis();
  if (now - brLast < 16) return;
  brLast = now;
  if (tc.down) brPX = constrain(tc.x, BR_PW / 2, SW - BR_PW / 2);
  if (brState == 0 && tc.press && tc.y > BAR) {
    brState = 1;
    tft.fillRect(0, 140, SW, 24, TH.bg);
    brVX = brSpeed * 0.45f * (random(2) ? 1 : -1);
    brVY = -brSpeed * 0.89f;
  }
  // erase ball
  if (brDrawX > -40) tft.fillCircle(brDrawX, brDrawY, 4, TH.bg);
  if (brState == 0) { brX = brPX; brY = BR_PY - 5; }
  else {
    brX += brVX; brY += brVY;
    if (brX < 4) { brX = 4; brVX = -brVX; }
    if (brX > SW - 5) { brX = SW - 5; brVX = -brVX; }
    if (brY < BAR + 4) { brY = BAR + 4; brVY = -brVY; }
    // paddle
    if (brVY > 0 && brY + 4 >= BR_PY && brY + 4 <= BR_PY + 10 && brX >= brPX - BR_PW / 2 - 4 && brX <= brPX + BR_PW / 2 + 4) {
      float rel = constrain((brX - brPX) / (BR_PW / 2.0f), -1.0f, 1.0f);
      float sp = sqrtf(brVX * brVX + brVY * brVY);
      float ang = rel * 1.05f;
      brVX = sp * sinf(ang); brVY = -sp * cosf(ang);
      brY = BR_PY - 5; beep(500, 20);
    }
    // bricks (leading points)
    float lpy = brY + (brVY > 0 ? 4 : -4);
    if (brHit(brX, lpy)) brVY = -brVY;
    else {
      float lpx = brX + (brVX > 0 ? 4 : -4);
      if (brHit(lpx, brY)) brVX = -brVX;
    }
    if (brLeft == 0) {
      brLevel++; brSpeed = min(6.0f, brSpeed + 0.5f);
      brBricks(); brBallReady(); beep(2000, 150);
    }
    if (brY > SH + 6) {
      brLives--; brStatus(); beep(250, 250);
      if (brLives <= 0) {
        brState = 2; submitScore(6, brScore); flushHi();
        brDrawX = -50;
        overlay("Game Over", String("Score ") + brScore + "   Best " + hi[6], TH.bad);
        return;
      }
      brBallReady();
    }
  }
  // paddle redraw
  if ((int)brPX != brOldPX) {
    tft.fillRect(brOldPX - BR_PW / 2 - 1, BR_PY, BR_PW + 2, 8, TH.bg);
    brOldPX = (int)brPX;
  }
  brPaddle();
  brDrawX = (int)brX; brDrawY = (int)brY;
  tft.fillCircle(brDrawX, brDrawY, 4, TH.text);
}

// ---------------------------- 7: SIMON -------------------------------------
const uint16_t SIMB[4] = {C565(60,220,100), C565(255,70,70), C565(255,220,60), C565(70,140,255)};
const uint16_t SIMD[4] = {C565(18,74,34), C565(84,24,24), C565(84,74,20), C565(24,46,84)};
const int SIMF[4] = {330, 262, 392, 523};
uint8_t simSeq[90];
int simLevel, simIdx, simIn, simState, simLit;
uint32_t simLitUntil, simGapUntil;

void simHub() {
  tft.fillCircle(160, 133, 27, TH.bg);
  txt(String(simLevel), 160, 133, 4, TH.text, TH.bg, MC_DATUM);
}
void simPad(int i, bool lit) {
  int x = (i % 2) ? 164 : 6, y = (i / 2) ? 134 : 32;
  tft.fillRoundRect(x, y, 150, 98, 14, lit ? SIMB[i] : SIMD[i]);
  simHub();
}
void simInit() {
  clearContent();
  simLevel = 1; simIdx = 0; simIn = 0; simState = 0; simLit = -1;
  simSeq[0] = random(4);
  for (int i = 0; i < 4; i++) simPad(i, false);
  gameScoreText("Level 1");
  txt("Tap", 160, 133, 2, TH.dim, TH.bg, MC_DATUM);
}
void simOver() {
  simState = 3;
  submitScore(7, simLevel - 1); flushHi();
  beep(150, 500);
  overlay("Wrong!", String("Rounds ") + (simLevel - 1) + "   Best " + hi[7], TH.bad);
}
void simLoop() {
  uint32_t now = millis();
  if (simState == 3) { if (tapAfterOver()) simInit(); return; }
  if (simLit >= 0 && (int32_t)(now - simLitUntil) >= 0) { simPad(simLit, false); simLit = -1; }
  if (simState == 0) {
    if (tc.press && tc.y > BAR) {
      simState = 1; simIdx = 0; simGapUntil = now + 500;
      simHub();
    }
    return;
  }
  if (simState == 1 && simLit < 0 && (int32_t)(now - simGapUntil) >= 0) {
    if (simIdx < simLevel) {
      simLit = simSeq[simIdx++];
      simPad(simLit, true); beep(SIMF[simLit], 250);
      int dur = max(200, 520 - simLevel * 14);
      simLitUntil = now + dur; simGapUntil = now + dur + 150;
    } else { simState = 2; simIn = 0; }
  }
  if (simState == 2 && tc.press && tc.y >= 32) {
    int pad = ((tc.y < 133) ? 0 : 2) + ((tc.x < 160) ? 0 : 1);
    if (simLit >= 0) { simPad(simLit, false); simLit = -1; }
    simLit = pad; simPad(pad, true); beep(SIMF[pad], 180);
    simLitUntil = now + 200;
    if (pad == simSeq[simIn]) {
      simIn++;
      if (simIn == simLevel) {
        simLevel++;
        simSeq[simLevel - 1] = random(4);
        submitScore(7, simLevel - 1);
        gameScoreText(String("Level ") + simLevel);
        simState = 1; simIdx = 0; simGapUntil = now + 900;
      }
    } else simOver();
  }
}

// ---------------------------- game dispatcher -------------------------------
void gameInit() {
  clearContent();
  drawBar(GAME_TITLE[curGame], "NEW", false);
  switch (curGame) {
    case 0: tttInit(); break;
    case 1: snInit(); break;
    case 2: mmInit(); break;
    case 3: wkInit(); break;
    case 4: rxInit(); break;
    case 5: g48Init(); break;
    case 6: brInit(); break;
    default: simInit(); break;
  }
}
void gameStart(int g) {
  flushHi();
  curGame = g;
  screen = SCR_GAME;
  gameInit();
}
void gameLoop() {
  if (pressIn(SW - 70, 0, 66, BAR + 2)) { beep(1500, 20); gameStart(curGame); return; }
  switch (curGame) {
    case 0: tttLoop(); break;
    case 1: snLoop(); break;
    case 2: mmLoop(); break;
    case 3: wkLoop(); break;
    case 4: rxLoop(); break;
    case 5: g48Loop(); break;
    case 6: brLoop(); break;
    default: simLoop(); break;
  }
}

// ============================ GAMES MENU ====================================
const char* GAME_NAME[8] = {"X & O", "Snake", "Memory", "Whack", "Reflex", "2048", "Bricks", "Simon"};
int gtx(int i) { return 4 + (i % 4) * 80; }
int gty(int i) { return 32 + (i / 4) * 102; }

String hiText(int g) {
  if (hi[g] == 0) return String("-");
  if (g == 0) return String("Wins ") + hi[0];
  if (g == 2) return String(hi[2]) + " moves";
  if (g == 4) return String(hi[4]) + " ms";
  if (g == 7) return String("Lv ") + hi[7];
  return String("Best ") + hi[g];
}
void gameIcon(int g, int cx, int cy, uint16_t bg) {
  switch (g) {
    case 0:
      tft.drawLine(cx - 6, cy - 18, cx - 6, cy + 18, TH.dim); tft.drawLine(cx + 6, cy - 18, cx + 6, cy + 18, TH.dim);
      tft.drawLine(cx - 18, cy - 6, cx + 18, cy - 6, TH.dim); tft.drawLine(cx - 18, cy + 6, cx + 18, cy + 6, TH.dim);
      thickLine(cx - 16, cy - 16, cx - 8, cy - 8, 3, TH.accent); thickLine(cx - 16, cy - 8, cx - 8, cy - 16, 3, TH.accent);
      for (int r = 4; r <= 5; r++) tft.drawCircle(cx, cy, r + 2, TH.accent2);
      break;
    case 1:
      for (int i = 0; i < 5; i++) tft.fillRoundRect(cx - 20 + i * 8, cy + 6, 7, 7, 2, TH.good);
      for (int i = 0; i < 3; i++) tft.fillRoundRect(cx + 12, cy - 10 + i * 8 - 2, 7, 7, 2, TH.good);
      tft.fillRoundRect(cx + 12, cy - 18, 7, 7, 2, TH.accent);
      tft.fillCircle(cx - 12, cy - 10, 4, TH.bad);
      break;
    case 2:
      tft.fillRoundRect(cx - 24, cy - 15, 22, 30, 4, TH.accent);
      tft.fillRoundRect(cx + 2, cy - 15, 22, 30, 4, TH.accent2);
      txt("?", cx - 13, cy, 2, TH.onacc, TH.accent, MC_DATUM);
      txt("?", cx + 13, cy, 2, TH.onacc, TH.accent2, MC_DATUM);
      break;
    case 3:
      tft.fillEllipse(cx, cy + 12, 20, 7, C565(55, 34, 22));
      tft.fillCircle(cx, cy - 2, 13, C565(166, 112, 70));
      tft.fillCircle(cx - 5, cy - 5, 2, C565(20, 14, 10)); tft.fillCircle(cx + 5, cy - 5, 2, C565(20, 14, 10));
      tft.fillCircle(cx, cy + 1, 3, C565(255, 150, 160));
      break;
    case 4:
      tft.fillTriangle(cx + 4, cy - 20, cx - 10, cy + 3, cx + 2, cy + 3, COL_SUN);
      tft.fillTriangle(cx - 4, cy + 20, cx + 10, cy - 3, cx - 2, cy - 3, COL_SUN);
      break;
    case 5:
      tft.fillRoundRect(cx - 22, cy - 18, 44, 36, 7, C565(237, 194, 46));
      txt("2048", cx, cy + 1, 2, C565(255, 255, 255), C565(237, 194, 46), MC_DATUM);
      break;
    case 6:
      for (int r = 0; r < 3; r++) for (int c = 0; c < 4; c++) tft.fillRect(cx - 22 + c * 12, cy - 20 + r * 7, 10, 5, BRC[r]);
      tft.fillCircle(cx + 6, cy + 4, 3, TH.text);
      tft.fillRoundRect(cx - 12, cy + 16, 24, 5, 2, TH.accent);
      break;
    default:
      tft.fillRoundRect(cx - 20, cy - 20, 19, 19, 5, SIMB[0]); tft.fillRoundRect(cx + 1, cy - 20, 19, 19, 5, SIMB[1]);
      tft.fillRoundRect(cx - 20, cy + 1, 19, 19, 5, SIMB[2]); tft.fillRoundRect(cx + 1, cy + 1, 19, 19, 5, SIMB[3]);
      break;
  }
}
void gamesInit() {
  tft.fillScreen(TH.bg);
  drawBar("Games", nullptr, false);
  for (int i = 0; i < 8; i++) {
    int x = gtx(i), y = gty(i);
    tft.fillRoundRect(x, y, 72, 94, 10, TH.panel);
    gameIcon(i, x + 36, y + 36, TH.panel);
    txt(GAME_NAME[i], x + 36, y + 68, 2, TH.text, TH.panel, MC_DATUM);
    txt(hiText(i), x + 36, y + 84, 1, TH.dim, TH.panel, MC_DATUM);
  }
}
void gamesLoop() {
  if (!tc.press) return;
  for (int i = 0; i < 8; i++) {
    if (pressIn(gtx(i), gty(i), 72, 94)) { beep(1500, 20); gameStart(i); return; }
  }
}

// ============================ PHONE REMOTE (WEB SERVER) =====================
WebServer server(80);
const char* SCREEN_NAMES[7] = {"Home", "Clock", "Timer", "Weather", "Games", "Settings", "Game"};

void wsRefreshTimerUI() {
  if (screen != SCR_TIMER) return;
  tmLastA = -1; tmLastB = -1;
  tmDrawMain(); tmDrawSub(); tmDrawButtons(); tmDrawLaps();
}

void handleWebRoot() {
  String html = F(
    "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>CYD Hub Remote</title><style>"
    "body{font-family:sans-serif;background:#10121a;color:#eef1ff;margin:0;padding:16px;}"
    "h2{margin:4px 0 12px;} .row{display:flex;flex-wrap:wrap;gap:8px;margin-bottom:14px;}"
    "button{flex:1 1 auto;min-width:70px;padding:12px 6px;border:none;border-radius:10px;"
    "background:#1a1e38;color:#eef1ff;font-size:14px;}"
    "button:active{background:#00c8ff;color:#10121a;}"
    ".sec{background:#161a2c;border-radius:12px;padding:10px 12px;margin-bottom:14px;}"
    ".sec h3{margin:0 0 8px;font-size:13px;color:#9099b8;text-transform:uppercase;}"
    "#status{font-size:13px;color:#9099b8;margin-bottom:10px;}"
    "input[type=range]{width:100%;margin-top:8px;}"
    "</style></head><body>"
    "<h2>CYD Hub Remote</h2><div id='status'>loading...</div>"
    "<div class='sec'><h3>Navigate</h3><div class='row'>"
    "<button onclick=\"go('home')\">Home</button><button onclick=\"go('clock')\">Clock</button>"
    "<button onclick=\"go('timer')\">Timer</button><button onclick=\"go('weather')\">Weather</button>"
    "<button onclick=\"go('games')\">Games</button><button onclick=\"go('settings')\">Settings</button>"
    "</div></div>"
    "<div class='sec'><h3>Games</h3><div class='row' id='games'></div></div>"
    "<div class='sec'><h3>Timer / Stopwatch</h3><div class='row'>"
    "<button onclick=\"act('timer','-1m')\">-1m</button><button onclick=\"act('timer','+1m')\">+1m</button>"
    "<button onclick=\"act('timer','-10s')\">-10s</button><button onclick=\"act('timer','+10s')\">+10s</button>"
    "</div><div class='row'>"
    "<button onclick=\"act('timer','start')\">CD Start/Pause</button><button onclick=\"act('timer','reset')\">CD Reset</button>"
    "<button onclick=\"act('sw','toggle')\">SW Start/Stop</button><button onclick=\"act('sw','lap')\">SW Lap/Reset</button>"
    "</div></div>"
    "<div class='sec'><h3>Display &amp; Settings</h3><div class='row'>"
    "<button onclick=\"act('theme','prev')\">Theme -</button><button onclick=\"act('theme','next')\">Theme +</button>"
    "<button onclick=\"act('toggle','h24')\">12/24h</button><button onclick=\"act('toggle','sound')\">Sound</button>"
    "<button onclick=\"act('toggle','fahr')\">C/F</button>"
    "</div><input type='range' min='10' max='255' id='bright' onchange=\"setBright(this.value)\"></div>"
    "<script>"
    "const games=['X & O','Snake','Memory','Whack','Reflex','2048','Bricks','Simon'];"
    "let g=document.getElementById('games');"
    "games.forEach((n,i)=>{let b=document.createElement('button');b.textContent=n;"
    "b.onclick=()=>act('game',i);g.appendChild(b);});"
    "function go(s){fetch('/goto?screen='+s).then(refresh);}"
    "function act(k,v){fetch('/act?k='+k+'&v='+v).then(refresh);}"
    "function setBright(v){fetch('/act?k=bright&v='+v);}"
    "function refresh(){fetch('/status').then(r=>r.json()).then(d=>{"
    "document.getElementById('status').textContent='Screen: '+d.screen+'   '+d.time+"
    "(d.wx!==null?('   '+d.wx+'\\u00B0'):'');"
    "document.getElementById('bright').value=d.bright;});}"
    "refresh(); setInterval(refresh,3000);"
    "</script></body></html>"
  );
  server.send(200, "text/html", html);
}

void handleWebGoto() {
  String s = server.arg("screen");
  int target = -1;
  if (s == "home") target = SCR_HOME;
  else if (s == "clock") target = SCR_CLOCK;
  else if (s == "timer") target = SCR_TIMER;
  else if (s == "weather") target = SCR_WEATHER;
  else if (s == "games") target = SCR_GAMES;
  else if (s == "settings") target = SCR_SETTINGS;
  if (target >= 0) { flushHi(); goScreen(target); }
  server.send(200, "text/plain", "ok");
}

void handleWebAct() {
  String k = server.arg("k");
  String v = server.arg("v");

  if (k == "game") {
    int idx = v.toInt();
    if (idx >= 0 && idx < 8) gameStart(idx);
  } else if (k == "timer") {
    int delta = 0;
    if (v == "-1m") delta = -60;
    else if (v == "+1m") delta = 60;
    else if (v == "-10s") delta = -10;
    else if (v == "+10s") delta = 10;
    if (delta && !cdRun && !cdDone) {
      int t = (int)(cdTotal / 1000) + delta;
      t = constrain(t, 10, 5990);
      cdTotal = (uint32_t)t * 1000UL;
      cdLeft = cdTotal;
    } else if (v == "start") {
      if (cdDone) { cdDone = false; cdLeft = cdTotal; }
      else if (cdRun) cdRun = false;
      else { if (cdLeft == 0) cdLeft = cdTotal; cdRun = true; cdLast = millis(); }
    } else if (v == "reset") {
      cdRun = false; cdDone = false; cdLeft = cdTotal;
    }
    wsRefreshTimerUI();
  } else if (k == "sw") {
    if (v == "toggle") {
      swRun = !swRun;
      if (swRun) swLast = millis();
    } else if (v == "lap") {
      if (swRun) { lapMs[1] = lapMs[0]; lapMs[0] = swMs; lapNo++; }
      else { swMs = 0; lapNo = 0; lapMs[0] = lapMs[1] = 0; }
    }
    wsRefreshTimerUI();
  } else if (k == "theme") {
    if (v == "prev") cfg.theme = (cfg.theme + NTHEMES - 1) % NTHEMES;
    else cfg.theme = (cfg.theme + 1) % NTHEMES;
    saveCfg();
    if (screen == SCR_SETTINGS) settingsInit();
    else if (screen == SCR_HOME) homeInit();
  } else if (k == "toggle") {
    if (v == "h24") cfg.h24 = !cfg.h24;
    else if (v == "sound") cfg.sound = !cfg.sound;
    else if (v == "fahr") cfg.fahr = !cfg.fahr;
    saveCfg();
    if (screen == SCR_SETTINGS) settingsInit();
  } else if (k == "bright") {
    int b = constrain(v.toInt(), 10, 255);
    cfg.bright = b;
    setBacklight(b);
    saveCfg();
    if (screen == SCR_SETTINGS) setRowBright();
  }
  server.send(200, "text/plain", "ok");
}

void handleWebStatus() {
  String t = "--:--";
  if (timeValid()) {
    char b[8];
    snprintf(b, sizeof(b), "%02d:%02d", nowTm.tm_hour, nowTm.tm_min);
    t = String(b);
  }
  String json = "{";
  json += "\"screen\":\"" + String(SCREEN_NAMES[screen]) + "\",";
  json += "\"time\":\"" + t + "\",";
  json += "\"wx\":" + (wx.ok ? String(tempNum(wx.t)) : String("null")) + ",";
  json += "\"bright\":" + String(cfg.bright);
  json += "}";
  server.send(200, "application/json", json);
}

void setupWebServer() {
  server.on("/", handleWebRoot);
  server.on("/goto", handleWebGoto);
  server.on("/act", handleWebAct);
  server.on("/status", handleWebStatus);
  server.begin();
}

// ============================ SCREEN ROUTER =================================
void goScreen(int s) {
  if (screen == SCR_GAME && s != SCR_GAME) flushHi();
  screen = s;
  switch (s) {
    case SCR_HOME: homeInit(); break;
    case SCR_CLOCK: clockInit(); break;
    case SCR_TIMER: timerInit(); break;
    case SCR_WEATHER: weatherInit(); break;
    case SCR_GAMES: gamesInit(); break;
    case SCR_SETTINGS: settingsInit(); break;
    default: break;
  }
}

// ============================ SETUP / LOOP ==================================
void splash() {
  tft.fillScreen(TH.bg);
  txtp("CYD HUB", SW / 2, 96, 4, TH.accent, TH.bg, MC_DATUM, 2, 0);
  txt("Clock  Timer  Weather  Games", SW / 2, 140, 2, TH.dim, TH.bg, MC_DATUM);
  tft.drawRoundRect(60, 170, 200, 10, 5, TH.dim);
  for (int i = 0; i <= 192; i += 6) { tft.fillRoundRect(64, 172, i, 6, 3, TH.accent); delay(6); }
}
void setup() {
  Serial.begin(115200);
  loadCfg();
  tft.init();
  tft.setRotation(TFT_ROT);
  tft.invertDisplay(INVERT_COLORS);
  tft.fillScreen(TH.bg);
  hwPwmInit();
  touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  ts.begin(touchSPI);
  ts.setRotation(1);
  randomSeed(esp_random());
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  lastWifiTry = millis();
  setupWebServer();
  splash();
  goScreen(SCR_HOME);
}
void loop() {
  updateTouch();
  beepTick();
  wifiTick();
  timerBackground();
  weatherTick();
  server.handleClient();

  if (screen != SCR_HOME && tc.press && tc.x < 52 && tc.y < BAR + 2) {
    beep(1000, 20);
    goScreen(screen == SCR_GAME ? SCR_GAMES : SCR_HOME);
    return;
  }
  switch (screen) {
    case SCR_HOME: homeLoop(); break;
    case SCR_CLOCK: clockLoop(); break;
    case SCR_TIMER: timerLoop(); break;
    case SCR_WEATHER: weatherLoop(); break;
    case SCR_GAMES: gamesLoop(); break;
    case SCR_SETTINGS: settingsLoop(); break;
    case SCR_GAME: gameLoop(); break;
  }
  delay(1);
}

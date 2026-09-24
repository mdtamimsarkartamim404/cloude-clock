/* CYD HUB Professional Control v3
 * ESP32-2432S028R (2.8" Cheap Yellow Display, ST7789)
 * Clock | Timer | Alarm | Weather | 10 Games | Islamic (Bangla) | Web Control
 *
 * FIX: islamicText() now takes an int index (not const IslamicItem&) so the
 *      Arduino auto-generated prototype doesn't reference IslamicItem before
 *      the struct is defined.
 */

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
#include <sys/time.h>

// ============================== USER SETTINGS ==============================
const char* WIFI_SSID = "";
const char* WIFI_PASS = "";
const char* CONTROL_AP_SSID = "CYD-HUB-CONTROL";
const char* CONTROL_AP_PASS = "CYDControl24";
const long  GMT_OFFSET_SEC = 6 * 3600;
const int   DST_OFFSET_SEC = 0;
const char* CITY_NAME = "Rajshahi";
const float LATITUDE  = 24.3745f;
const float LONGITUDE = 88.6042f;

#define TFT_ROT        1
#define TOUCH_FLIP     0
#define INVERT_COLORS  0

#define TS_X_MIN 440
#define TS_X_MAX 3568
#define TS_Y_MIN 504
#define TS_Y_MAX 3561
#define TS_Z_MIN 250

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

#define SCR_HOME     0
#define SCR_CLOCK    1
#define SCR_TIMER    2
#define SCR_WEATHER  3
#define SCR_GAMES    4
#define SCR_SETTINGS 5
#define SCR_GAME     6
#define SCR_SETUP    7
#define SCR_ISLAMIC  8
#define SCR_ALARM    9
#define SCR_KNOWLEDGE 10

#define C565(r,g,b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

const int SW = 320, SH = 240, BAR = 26;

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite clockSprite = TFT_eSprite(&tft);
bool clockSpriteReady = false;

SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(TOUCH_CS);
Preferences prefs;

int screen = SCR_HOME;
int curGame = 0;
uint8_t setupQrStage = 0;

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
void setupInit();
void islamicInit();
void islamicLoop();
void islamicDraw();
void clockInit();
long daysFromCivil(int y, unsigned m, unsigned d);
void alarmInit();
void alarmLoop();
void alarmTick();
void alarmDrawRow(int i);
void saveAlarms();
void loadAlarms();
void knowledgeInit();
void knowledgeLoop();

// ============================ AP QR CODES ==================================
const uint8_t CYD_AP_QR[33][33] PROGMEM = {
  {1,1,1,1,1,1,1,0,0,0,0,1,0,0,1,1,1,0,1,0,1,0,0,0,0,0,1,1,1,1,1,1,1},
  {1,0,0,0,0,0,1,0,0,0,1,0,1,0,0,1,1,0,1,0,1,0,0,0,1,0,1,0,0,0,0,0,1},
  {1,0,1,1,1,0,1,0,1,1,1,0,0,0,1,0,0,0,0,0,1,1,1,1,1,0,1,0,1,1,1,0,1},
  {1,0,1,1,1,0,1,0,1,1,0,0,0,0,0,0,0,0,1,1,0,1,0,1,0,0,1,0,1,1,1,0,1},
  {1,0,1,1,1,0,1,0,1,1,1,0,1,0,1,1,1,0,0,0,1,1,0,1,1,0,1,0,1,1,1,0,1},
  {1,0,0,0,0,0,1,0,1,1,1,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,1,0,0,0,0,0,1},
  {1,1,1,1,1,1,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,1,1,1,1,1,1},
  {0,0,0,0,0,0,0,0,1,1,0,0,1,0,0,0,1,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0},
  {1,0,1,1,1,1,1,0,0,1,1,0,0,1,0,0,1,0,0,1,0,1,1,1,0,0,1,1,1,1,1,0,0},
  {1,0,0,1,1,1,0,1,1,1,1,0,0,1,1,0,0,1,1,1,0,0,1,1,1,1,0,0,0,0,0,0,0},
  {0,1,0,1,1,0,1,1,0,0,0,1,1,1,1,1,0,1,0,0,0,0,1,0,0,0,0,1,1,1,1,1,0},
  {0,1,0,1,1,0,0,1,0,0,0,0,1,1,0,1,1,1,1,1,0,0,0,1,1,0,1,1,1,1,1,1,0},
  {1,0,1,1,0,1,1,1,1,1,1,1,0,0,1,0,1,0,0,0,1,1,1,0,0,1,0,0,1,0,1,0,1},
  {1,1,0,1,0,0,0,0,0,0,0,0,1,0,0,1,0,1,1,0,0,0,0,1,1,0,1,0,0,1,1,0,0},
  {0,1,1,0,1,0,1,1,0,0,1,1,0,1,1,1,1,1,0,0,1,1,0,0,1,1,0,1,0,0,1,1,0},
  {1,1,0,1,1,1,0,0,1,1,0,0,1,1,0,1,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,1,1},
  {0,1,0,0,0,0,1,1,1,0,1,0,0,0,0,1,0,0,0,0,0,1,1,0,0,1,0,0,1,0,1,1,0},
  {0,0,1,0,0,1,0,1,1,0,0,1,1,0,0,1,1,0,1,0,1,1,0,0,0,1,0,1,0,1,1,1,0},
  {1,1,0,0,0,1,1,0,1,0,1,0,1,1,1,1,1,0,1,0,1,1,0,0,0,1,1,1,0,0,0,1,0},
  {0,0,0,0,0,1,0,0,0,0,1,1,0,1,1,0,1,0,0,0,1,1,1,1,1,1,1,0,0,1,1,1,1},
  {0,0,0,1,0,0,1,0,1,1,1,0,0,0,0,0,0,0,1,0,0,1,0,1,0,1,0,0,1,0,1,0,0},
  {1,1,0,1,0,0,0,0,0,0,0,1,1,0,0,1,1,1,0,0,1,1,1,1,1,0,1,0,0,0,1,1,0},
  {1,0,0,1,1,1,1,0,1,0,1,1,1,0,0,0,0,1,0,1,0,1,1,0,1,0,0,1,1,1,0,1,0},
  {1,0,1,0,1,0,0,1,1,0,0,0,1,0,0,0,1,0,1,1,1,1,0,1,1,0,0,0,1,1,1,1,0},
  {1,0,1,1,1,1,1,0,1,0,1,0,0,1,1,0,0,0,0,1,1,0,0,0,1,1,1,1,1,0,0,1,1},
  {0,0,0,0,0,0,0,0,1,1,1,0,1,0,1,1,1,1,0,0,1,0,0,1,1,0,0,0,1,0,0,0,0},
  {1,1,1,1,1,1,1,0,0,0,0,0,1,0,0,1,0,1,0,0,0,0,1,1,1,0,1,0,1,1,1,1,0},
  {1,0,0,0,0,0,1,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,1,0,0,0,1,0,1,0,1},
  {1,0,1,1,1,0,1,0,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,1,1,1,1,1,1,0,0,1},
  {1,0,1,1,1,0,1,0,1,0,1,0,1,0,1,1,0,0,1,0,0,0,0,1,1,1,0,0,0,1,0,0,1},
  {1,0,1,1,1,0,1,0,1,0,1,1,0,0,1,1,1,0,0,0,1,1,1,0,1,0,0,0,0,1,0,0,0},
  {1,0,0,0,0,0,1,0,0,0,0,0,0,1,1,1,0,0,1,0,0,1,1,1,0,1,1,1,1,1,1,0,0},
  {1,1,1,1,1,1,1,0,1,0,1,1,0,0,1,1,1,0,0,0,0,1,1,0,1,0,0,1,1,0,0,1,0}
};

const uint8_t CYD_IP_QR[25][25] PROGMEM = {
  {1,1,1,1,1,1,1,0,1,1,1,0,1,1,0,0,1,0,1,1,1,1,1,1,1},
  {1,0,0,0,0,0,1,0,1,1,1,0,0,1,1,0,0,0,1,0,0,0,0,0,1},
  {1,0,1,1,1,0,1,0,1,1,0,0,1,1,1,0,0,0,1,0,1,1,1,0,1},
  {1,0,1,1,1,0,1,0,0,1,0,1,0,1,1,1,1,0,1,0,1,1,1,0,1},
  {1,0,1,1,1,0,1,0,1,0,1,1,0,1,1,0,0,0,1,0,1,1,1,0,1},
  {1,0,0,0,0,0,1,0,0,1,0,1,1,1,1,0,1,0,1,0,0,0,0,0,1},
  {1,1,1,1,1,1,1,0,1,0,1,0,1,0,1,0,1,0,1,1,1,1,1,1,1},
  {0,0,0,0,0,0,0,0,0,0,0,1,0,1,0,0,1,0,0,0,0,0,0,0,0},
  {1,0,0,1,1,1,1,1,1,0,0,0,0,1,0,1,0,1,0,0,1,0,1,1,1},
  {1,0,0,0,0,0,0,0,1,1,0,1,0,1,1,1,1,1,0,0,1,1,1,1,0},
  {0,1,1,1,0,0,1,1,0,1,0,1,0,1,0,1,0,1,1,1,1,1,0,0,1},
  {1,1,0,0,1,1,0,1,0,0,0,1,1,1,0,1,0,1,0,0,1,1,1,1,1},
  {0,0,1,1,1,0,1,1,0,1,0,0,1,1,0,0,1,1,1,0,0,0,0,0,1},
  {1,0,0,0,1,1,0,1,0,0,1,0,0,1,1,1,0,0,0,1,1,0,0,1,0},
  {1,1,1,1,1,1,1,0,1,1,0,1,1,1,0,1,1,1,0,0,0,1,1,1,1},
  {1,0,1,1,1,0,0,0,1,0,0,1,0,0,0,0,1,0,0,0,1,0,1,0,1},
  {1,0,0,1,1,0,1,1,0,1,0,0,0,0,1,0,1,1,1,1,1,0,1,1,0},
  {0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,1,0,0,1,0},
  {1,1,1,1,1,1,1,0,1,1,0,1,0,0,0,0,1,0,1,0,1,1,0,0,1},
  {1,0,0,0,0,0,1,0,1,1,0,0,1,0,1,0,1,0,0,0,1,0,0,1,0},
  {1,0,1,1,1,0,1,0,1,1,0,0,1,1,1,1,1,1,1,1,1,1,0,0,1},
  {1,0,1,1,1,0,1,0,1,1,0,0,0,0,1,0,0,0,1,1,0,1,0,1,1},
  {1,0,1,1,1,0,1,0,0,0,0,0,1,0,1,0,0,0,0,0,1,0,1,1,1},
  {1,0,0,0,0,0,1,0,0,0,0,0,0,0,1,1,0,1,1,1,1,0,1,1,1},
  {1,1,1,1,1,1,1,0,1,0,1,0,0,1,1,1,1,0,1,0,0,1,0,0,1}
};

template <size_t N>
void drawQR(const uint8_t (&qr)[N][N], int x, int y, int module) {
  const int quiet = 2;
  const int total = (N + quiet * 2) * module;
  tft.fillRect(x, y, total, total, TFT_WHITE);
  for (size_t r = 0; r < N; r++) {
    for (size_t c = 0; c < N; c++) {
      if (pgm_read_byte(&qr[r][c])) {
        tft.fillRect(x + (c + quiet) * module, y + (r + quiet) * module, module, module, TFT_BLACK);
      }
    }
  }
}

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
  {"Amoled",   C565(0,0,0),     C565(22,22,26),  C565(255,255,255), C565(130,130,140), C565(176,112,255), C565(0,224,200), C565(80,230,120),  C565(255,80,80),  C565(0,0,0)},
  {"Ocean",    C565(6,18,36),   C565(14,38,66),  C565(224,242,255), C565(110,150,190), C565(0,180,220),  C565(80,220,200), C565(80,230,150),  C565(255,110,110), C565(6,18,36)},
  {"Royal",    C565(20,10,40),  C565(40,22,72),  C565(245,235,255), C565(150,130,190), C565(180,120,255), C565(255,180,80), C565(100,230,150), C565(255,100,100), C565(20,10,40)},
  {"Cyber",    C565(8,6,20),    C565(20,14,42),  C565(230,255,250), C565(110,130,160), C565(0,255,200),  C565(255,60,180), C565(80,255,160),  C565(255,80,80),  C565(8,6,20)},
  {"Neon City",   C565(6,4,16),   C565(18,10,34),  C565(235,250,255), C565(120,110,150), C565(255,0,140),  C565(0,240,255), C565(100,255,80),  C565(255,45,85),  C565(6,4,16)},
  {"Synthwave",   C565(20,4,40),  C565(45,10,70),  C565(255,225,255), C565(150,110,180), C565(255,40,180), C565(255,140,40), C565(60,230,200),  C565(255,60,90),  C565(20,4,40)},
  {"Neo Tokyo",   C565(5,8,14),   C565(18,24,34),  C565(220,240,255), C565(100,120,140), C565(255,20,60),  C565(0,230,255), C565(80,255,140),  C565(255,40,60),  C565(5,8,14)},
  {"Glitch",      C565(4,10,6),   C565(14,26,18),  C565(180,255,190), C565(90,140,100),  C565(255,0,255),  C565(150,255,0), C565(80,255,120),  C565(255,60,60),  C565(4,10,6)},
  {"Chrome Noir", C565(6,6,10),   C565(18,18,26),  C565(220,225,235), C565(110,115,130), C565(90,190,255), C565(190,110,255), C565(90,230,160), C565(255,90,110), C565(6,6,10)},
  {"Acid Rain",   C565(4,12,10),  C565(12,30,26),  C565(200,255,150), C565(90,140,110),  C565(200,255,40), C565(0,220,180), C565(120,255,90),  C565(255,70,70),  C565(4,12,10)},
  {"Holo",        C565(6,10,18),  C565(16,22,38),  C565(225,245,255), C565(110,130,160), C565(80,200,255), C565(255,120,220), C565(120,255,190), C565(255,90,110), C565(6,10,18)},
  {"Nord",     C565(46,52,64),  C565(59,66,82),  C565(236,239,244), C565(143,153,170), C565(136,192,208), C565(180,142,173), C565(163,190,140), C565(191,97,106), C565(46,52,64)},
  {"Dracula",  C565(40,42,54),  C565(68,71,90),  C565(248,248,242), C565(98,114,164), C565(189,147,249), C565(255,121,198), C565(80,250,123),  C565(255,85,85),  C565(40,42,54)},
  {"Solarized",C565(0,43,54),   C565(7,54,66),   C565(253,246,227), C565(147,161,161), C565(38,139,210), C565(211,54,130), C565(133,153,0),  C565(220,50,47),  C565(0,43,54)},
  {"Gruvbox",  C565(40,40,40),  C565(60,56,54),  C565(235,219,178), C565(168,153,132), C565(215,153,33), C565(177,98,134), C565(152,151,26), C565(204,36,29),  C565(40,40,40)},
  {"Mocha",    C565(30,30,46),  C565(49,50,68),  C565(205,214,244), C565(127,132,156), C565(137,180,250), C565(245,194,231), C565(166,227,161), C565(243,139,168), C565(30,30,46)},
  {"Rose",     C565(28,18,22),  C565(48,28,36),  C565(255,240,245), C565(180,140,155), C565(255,120,160), C565(255,200,120), C565(120,220,180), C565(255,100,100), C565(28,18,22)}
};
const int NTHEMES = sizeof(themes) / sizeof(themes[0]);

struct Cfg {
  uint8_t theme;
  uint8_t bright;
  bool h24;
  bool sound;
  bool fahr;
  uint8_t clockStyle;
};
Cfg cfg = {0, 200, false, true, false, 0};
#define TH (themes[cfg.theme])

uint32_t hi[10];
bool hiDirty = false;

// ============================ STORAGE =======================================
void saveCfg() {
  prefs.putUChar("theme", cfg.theme);
  prefs.putUChar("bright", cfg.bright);
  prefs.putBool("h24", cfg.h24);
  prefs.putBool("snd", cfg.sound);
  prefs.putBool("fahr", cfg.fahr);
  prefs.putUChar("clock", cfg.clockStyle);
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
  if (prefs.isKey("clock")) cfg.clockStyle = prefs.getUChar("clock", 0);
  else cfg.clockStyle = prefs.getBool("ana", false) ? 1 : 0;
  if (cfg.clockStyle > 19) cfg.clockStyle = 0;
  for (int i = 0; i < 10; i++) {
    char k[4]; snprintf(k, sizeof(k), "h%d", i);
    hi[i] = prefs.getUInt(k, 0);
  }
  loadAlarms();
}
void flushHi() {
  if (!hiDirty) return;
  hiDirty = false;
  for (int i = 0; i < 10; i++) {
    char k[4]; snprintf(k, sizeof(k), "h%d", i);
    prefs.putUInt(k, hi[i]);
  }
}
bool lowerBetter(int g) { return g == 2 || g == 4; }
void submitScore(int g, uint32_t s) {
  if (g < 0 || g > 9) return;
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
extern bool alarmRinging;
void beepTick() {
  if (alarmRinging) return;
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
  bool d = raw || (tc.was && (now - tc.lastSeen) < 40);
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
  for (int i = 0; i < 7; i++) {
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
bool wifiConnectingFromWeb = false;
String wifiMessage = "";
uint32_t wifiMessageUntil = 0;
bool wxTried = false;

void setWifiMessage(const String &m) { wifiMessage = m; wifiMessageUntil = millis() + 5000; }
String savedWifiSSID() { return prefs.getString("wifi_ssid", String(WIFI_SSID)); }
String savedWifiPASS() { return prefs.getString("wifi_pass", String(WIFI_PASS)); }
String savedCity()     { return prefs.getString("city", String(CITY_NAME)); }
float savedLat()       { return prefs.getFloat("lat", LATITUDE); }
float savedLon()       { return prefs.getFloat("lon", LONGITUDE); }
long savedTzOffset()   { return prefs.getLong("tz", GMT_OFFSET_SEC); }
long savedDstOffset()  { return prefs.getLong("dst", DST_OFFSET_SEC); }

void saveLocationConfig(const String &city, float lat, float lon, long tz, long dst) {
  prefs.putString("city", city);
  prefs.putFloat("lat", lat);
  prefs.putFloat("lon", lon);
  prefs.putLong("tz", tz);
  prefs.putLong("dst", dst);
  ntpStarted = false;
  wxTried = false;
}
String tzLabel() {
  long sec = savedTzOffset();
  char b[12];
  long mins = sec / 60;
  long ah = labs(mins) / 60, am = labs(mins) % 60;
  snprintf(b, sizeof(b), "%s%02ld:%02ld", mins >= 0 ? "+" : "-", ah, am);
  return String(b);
}
void saveWifiCredentials(const String &ssid, const String &pass) {
  prefs.putString("wifi_ssid", ssid);
  prefs.putString("wifi_pass", pass);
}
void beginSavedWiFi() {
  String ssid = savedWifiSSID();
  String pass = savedWifiPASS();
  if (ssid.length()) {
    WiFi.begin(ssid.c_str(), pass.c_str());
    lastWifiTry = millis();
  }
}
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
    wifiConnectingFromWeb = false;
    if (!ntpStarted) {
      configTime(savedTzOffset(), savedDstOffset(), "pool.ntp.org", "time.google.com", "time.cloudflare.com");
      ntpStarted = true;
    }
  } else if (millis() - lastWifiTry > 20000) {
    beginSavedWiFi();
  }
}

// ============================ WEATHER =======================================
struct WxData {
  bool ok; float t, feels, wind; int hum, code;
  float pressure, precip, visibility, uv, dew, cloud;
  int windDir, rainProb[7];
  float hi[7], lo[7]; int dcode[7]; int dow[7];
  char sunrise[6], sunset[6];
  uint32_t updated; char stamp[8];
};
WxData wx;
uint32_t wxLastTry = 0;
uint8_t wxMode = 0; // NOW, 7-DAY, CONDITIONS, RAIN, SUN, WIND, DETAILS, EXTRA

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

uint16_t hexTo565(String h) {
  h.trim();
  if (h.startsWith("#")) h.remove(0, 1);
  if (h.length() != 6) return C565(255,255,255);
  long v = strtol(h.c_str(), nullptr, 16);
  return C565((v >> 16) & 255, (v >> 8) & 255, v & 255);
}
String colorHex(uint16_t c) {
  uint8_t r = ((c >> 11) & 0x1F) * 255 / 31;
  uint8_t g = ((c >> 5) & 0x3F) * 255 / 63;
  uint8_t b = (c & 0x1F) * 255 / 31;
  char s[8]; snprintf(s, sizeof(s), "#%02X%02X%02X", r, g, b);
  return String(s);
}
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
  String url = String("https://api.open-meteo.com/v1/forecast?latitude=") + String(savedLat(), 4) +
               "&longitude=" + String(savedLon(), 4) +
               "&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,wind_speed_10m,wind_direction_10m,pressure_msl,precipitation,visibility,uv_index,dew_point_2m,cloud_cover"
               "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,sunrise,sunset&forecast_days=7&timezone=auto";
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
  wx.windDir = cur["wind_direction_10m"] | 0;
  wx.dew = cur["dew_point_2m"] | 0.0f;
  wx.cloud = cur["cloud_cover"] | 0.0f;
  wx.pressure = cur["pressure_msl"] | 0.0f;
  wx.precip = cur["precipitation"] | 0.0f;
  wx.visibility = cur["visibility"] | 0.0f;
  wx.uv = cur["uv_index"] | 0.0f;
  wx.code  = cur["weather_code"] | 0;
  JsonObject d = doc["daily"];
  for (int i = 0; i < 4; i++) {
    wx.hi[i]    = (d["temperature_2m_max"][i] | 0.0f);
    wx.lo[i]    = (d["temperature_2m_min"][i] | 0.0f);
    wx.dcode[i] = (d["weather_code"][i] | 0);
    wx.rainProb[i] = d["precipitation_probability_max"][i] | 0;
    const char* ds = d["time"][i] | "2000-01-01";
    wx.dow[i] = dowOf(atoi(ds), atoi(ds + 5), atoi(ds + 8));
  }
  const char* sr = d["sunrise"][0] | "";
  const char* ss = d["sunset"][0] | "";
  strncpy(wx.sunrise, strlen(sr) >= 16 ? sr + 11 : sr, 5); wx.sunrise[5] = 0;
  strncpy(wx.sunset,  strlen(ss) >= 16 ? ss + 11 : ss, 5); wx.sunset[5] = 0;
  wx.ok = true;
  wx.updated = millis();
  if (timeValid()) snprintf(wx.stamp, sizeof(wx.stamp), "%02d:%02d", nowTm.tm_hour, nowTm.tm_min);
  else wx.stamp[0] = 0;
  return true;
}
void weatherTick() {
  if (screen == SCR_GAME) return;
  if (WiFi.status() != WL_CONNECTED) {
    if (wx.ok) {
      wx.ok = false;
      if (screen == SCR_WEATHER) wxDraw();
      else if (screen == SCR_HOME) homeHeader(true);
    }
    return;
  }
  uint32_t now = millis();
  if (wxTried && (now - wxLastTry) < (wx.ok ? 1200000UL : 30000UL)) return;
  wxTried = true;
  wxLastTry = now;
  fetchWeather();
  if (screen == SCR_WEATHER) wxDraw();
  else if (screen == SCR_HOME) homeHeader(true);
}

// ============================ ICONS =========================================
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
void iconIslamic(int cx, int cy, uint16_t c, uint16_t bg) {
  tft.fillCircle(cx + 2, cy, 16, c);
  tft.fillCircle(cx + 11, cy - 4, 15, bg);
  int sx = cx - 10, sy = cy - 8;
  for (int i = 0; i < 4; i++) {
    float a = i * PI / 2.0f;
    tft.drawLine(sx + (int)(cosf(a)*6), sy + (int)(sinf(a)*6),
                 sx - (int)(cosf(a)*6), sy - (int)(sinf(a)*6), c);
  }
}
void iconAlarm(int cx, int cy, uint16_t c) {
  tft.fillCircle(cx, cy - 2, 14, c);
  tft.fillRect(cx - 14, cy - 2, 28, 12, c);
  tft.fillRect(cx - 18, cy + 8, 36, 4, c);
  tft.fillCircle(cx, cy - 16, 3, c);
  tft.fillCircle(cx, cy + 14, 3, c);
}

// ============================ ALARM =========================================
struct Alarm {
  uint8_t hour;
  uint8_t minute;
  bool    enabled;
  uint8_t days;
  bool    dateMode;
  int32_t dateDay; // civil-day number for one-time/date alarms
};

Alarm alarms[3] = {
  {7,  0, false, 0x7F, false, 0},
  {12,30, false, 0x7F, false, 0},
  {18, 0, false, 0x7F, false, 0}
};

bool     alarmRinging  = false;
int      alarmRingIdx  = -1;
uint32_t alarmBeepAt   = 0;
bool     alarmBeepHigh = false;
uint32_t alarmSnoozeUntil = 0;
int      alarmSnoozeIdx   = -1;
int      alarmLastMinute  = -1;
int      alarmLastDay      = -1;
int      alarmLastFireMinute = -1;
int      alarmLastFireDay = -1;
bool     alarmSuppress    = false;
int      alarmEditIdx      = -1;
uint32_t alarmPopupUntil   = 0;
uint32_t alarmLastScanMs   = 0;

const char* alarmDaysLabel(uint8_t d) {
  if (d == 0x7F) return "DAILY";
  if (d == 0x3E) return "WEEKDAY";
  if (d == 0x41) return "WEEKEND";
  return "CUSTOM";
}
long alarmTodayDay() {
  if (!timeValid()) return 0;
  return daysFromCivil(nowTm.tm_year + 1900, nowTm.tm_mon + 1, nowTm.tm_mday);
}
String alarmDateLabel(int32_t day) {
  // Find a compact YYYY-MM-DD label by walking from the current date.
  if (!timeValid()) return "DATE";
  long cur = alarmTodayDay();
  long delta = (long)day - cur;
  struct tm tmp = nowTm;
  time_t base = time(nullptr) + (time_t)delta * 86400;
  localtime_r(&base, &tmp);
  char b[12]; snprintf(b, sizeof(b), "%04d-%02d-%02d", tmp.tm_year+1900, tmp.tm_mon+1, tmp.tm_mday);
  return String(b);
}

void saveAlarms() {
  for (int i = 0; i < 3; i++) {
    char k[12];
    snprintf(k, sizeof(k), "a%d_h", i); prefs.putUChar(k, alarms[i].hour);
    snprintf(k, sizeof(k), "a%d_m", i); prefs.putUChar(k, alarms[i].minute);
    snprintf(k, sizeof(k), "a%d_e", i); prefs.putBool(k, alarms[i].enabled);
    snprintf(k, sizeof(k), "a%d_d", i); prefs.putUChar(k, alarms[i].days);
    snprintf(k, sizeof(k), "a%d_dm", i); prefs.putBool(k, alarms[i].dateMode);
    snprintf(k, sizeof(k), "a%d_dd", i); prefs.putLong(k, alarms[i].dateDay);
  }
}
void loadAlarms() {
  for (int i = 0; i < 3; i++) {
    char k[12];
    snprintf(k, sizeof(k), "a%d_h", i); alarms[i].hour    = prefs.getUChar(k, alarms[i].hour);
    snprintf(k, sizeof(k), "a%d_m", i); alarms[i].minute  = prefs.getUChar(k, alarms[i].minute);
    snprintf(k, sizeof(k), "a%d_e", i); alarms[i].enabled = prefs.getBool(k, false);
    snprintf(k, sizeof(k), "a%d_d", i); alarms[i].days    = prefs.getUChar(k, 0x7F);
    snprintf(k, sizeof(k), "a%d_dm", i); alarms[i].dateMode = prefs.getBool(k, false);
    snprintf(k, sizeof(k), "a%d_dd", i); alarms[i].dateDay = prefs.getLong(k, 0);
    if (alarms[i].dateMode && alarms[i].dateDay == 0 && timeValid()) alarms[i].dateDay = alarmTodayDay();
  }
}

void alarmDrawRow(int i) {
  int y = 31 + i * 60, x = 5, w = 310, h = 56;
  tft.fillRoundRect(x, y, w, h, 10, TH.panel);
  tft.drawRoundRect(x, y, w, h, 10, alarms[i].enabled ? TH.accent : TH.dim);

  char lbl[12]; snprintf(lbl, sizeof(lbl), "ALARM %d", i + 1);
  txt(lbl, x + 8, y + 5, 1, TH.dim, TH.panel, TL_DATUM);
  txt(alarms[i].dateMode ? "DATE" : alarmDaysLabel(alarms[i].days), x + 62, y + 5, 1, TH.accent, TH.panel, TL_DATUM);
  if (alarms[i].dateMode) txtp(alarmDateLabel(alarms[i].dateDay), x + 308, y + 5, 1, TH.dim, TH.panel, MR_DATUM, 1, 70);

  char t[8]; snprintf(t, sizeof(t), "%02d:%02d", alarms[i].hour, alarms[i].minute);
  // Large touch target for direct on-device time editing.
  tft.fillRoundRect(x + 6, y + 18, 106, 31, 8, TH.bg);
  txtp(t, x + 59, y + 34, 4, alarms[i].enabled ? TH.text : TH.dim, TH.bg, MC_DATUM, 1, 100);
  txt("TAP TIME", x + 59, y + 48, 1, TH.dim, TH.bg, MC_DATUM);

  button(x + 118, y + 19, 34, 29, "-H", TH.bg, TH.accent, 2);
  button(x + 156, y + 19, 34, 29, "+H", TH.bg, TH.accent, 2);
  button(x + 194, y + 19, 34, 29, "-M", TH.bg, TH.accent2, 2);
  button(x + 232, y + 19, 34, 29, "+M", TH.bg, TH.accent2, 2);
  tft.fillRoundRect(x + 272, y + 4, 31, 22, 10, alarms[i].enabled ? TH.good : TH.dim);
  tft.fillCircle(alarms[i].enabled ? x + 294 : x + 281, y + 15, 7, C565(255,255,255));
}

void alarmDrawEditor() {
  if (alarmEditIdx < 0 || alarmEditIdx >= 3) return;
  int i = alarmEditIdx;
  tft.fillScreen(TH.bg);
  drawBar((String("SET ALARM ") + String(i + 1)).c_str(), nullptr, false);

  tft.fillRoundRect(12, 38, 296, 91, 16, TH.panel);
  tft.drawRoundRect(12, 38, 296, 91, 16, TH.accent);
  char t[8]; snprintf(t, sizeof(t), "%02d:%02d", alarms[i].hour, alarms[i].minute);
  txt(t, 160, 78, 7, TH.text, TH.panel, MC_DATUM);
  txt("HOUR        MINUTE", 160, 110, 1, TH.dim, TH.panel, MC_DATUM);
  if (alarms[i].dateMode) {
    txt("ONE-TIME DATE", 160, 125, 1, TH.accent2, TH.panel, MC_DATUM);
    txt(alarmDateLabel(alarms[i].dateDay), 160, 136, 2, TH.text, TH.panel, MC_DATUM);
  }

  button(18, 140, 62, 30, "- H", TH.panel, TH.accent, 2);
  button(88, 140, 62, 30, "+ H", TH.panel, TH.accent, 2);
  button(170, 140, 62, 30, "- M", TH.panel, TH.accent2, 2);
  button(240, 140, 62, 30, "+ M", TH.panel, TH.accent2, 2);

  button(18, 181, 88, 30, alarms[i].dateMode ? "DATE-" : "CANCEL", TH.panel, TH.text, 1);
  button(116, 181, 88, 30, alarms[i].dateMode ? "DAY MODE" : "DATE MODE", TH.panel, TH.accent, 1);
  button(214, 181, 88, 30, alarms[i].dateMode ? "DATE+" : "SAVE", alarms[i].dateMode ? TH.panel : TH.good, alarms[i].dateMode ? TH.accent2 : TH.onacc, 1);
  txt("Tap SAVE to keep the alarm", 160, 225, 1, TH.dim, TH.bg, MC_DATUM);
}

void alarmDrawRinging() {
  int w = 288, h = 178, x = (SW - w) / 2, y = (SH - h) / 2;
  tft.fillRoundRect(x, y, w, h, 16, TH.panel);
  for (int k = 0; k < 3; k++) tft.drawRoundRect(x + k, y + k, w - 2*k, h - 2*k, 16-k, k == 0 ? TH.accent2 : TH.accent);

  int cx = SW / 2, cy = y + 39;
  tft.fillCircle(cx, cy, 20, TH.accent2);
  tft.fillRect(cx - 20, cy, 40, 16, TH.accent2);
  tft.fillRect(cx - 24, cy + 14, 48, 5, TH.accent2);
  tft.fillCircle(cx, cy - 22, 4, TH.accent2);
  tft.fillCircle(cx, cy + 22, 4, TH.accent2);

  char t[12];
  snprintf(t, sizeof(t), "%02d:%02d", alarms[alarmRingIdx].hour, alarms[alarmRingIdx].minute);
  txt(t, SW / 2, y + 87, 4, TH.text, TH.panel, MC_DATUM);
  txt("ALARM RINGING", SW / 2, y + 111, 2, TH.accent2, TH.panel, MC_DATUM);

  button(x + 16,      y + h - 44, 118, 32, "SNOOZE 5m", TH.accent2, TH.onacc, 2);
  button(x + w - 134, y + h - 44, 118, 32, "STOP",      TH.bad, C565(255,255,255), 2);
}

void alarmInit() {
  alarmEditIdx = -1;
  tft.fillScreen(TH.bg);
  drawBar("ALARMS", nullptr, true);
  for (int i = 0; i < 3; i++) alarmDrawRow(i);
  button(5, 211, 72, 24, "TEST", TH.accent2, TH.onacc, 1);
  txt("TIME = edit  •  switch = ON/OFF", 196, 223, 1, TH.dim, TH.bg, MC_DATUM);
  if (alarmRinging) alarmDrawRinging();
}

void triggerAlarm(int idx) {
  if (idx < 0 || idx >= 3) return;
  alarmRinging = true;
  alarmRingIdx = idx;
  alarmBeepAt = millis();
  alarmBeepHigh = true;
  alarmSnoozeUntil = 0;
  alarmSnoozeIdx = -1;
  alarmPopupUntil = millis() + 600000UL;

  // Always force the alarm screen. This is intentionally independent of
  // the normal UI sound setting and of the current screen.
  screen = SCR_ALARM;
  tft.fillScreen(TH.bg);
  alarmDrawRinging();

  // Start the hardware buzzer immediately.
  soundTone(2800);
  beepEnd = 0;
  Serial.printf("ALARM TRIGGERED: %02u:%02u idx=%d\n", alarms[idx].hour, alarms[idx].minute, idx);
}

void dismissAlarm() {
  alarmRinging = false;
  alarmRingIdx = -1;
  soundTone(0); beepEnd = 0;
  alarmPopupUntil = 0;
  alarmSuppress = false;
  if (timeValid()) {
    alarmLastMinute = nowTm.tm_hour * 60 + nowTm.tm_min;
    alarmLastDay = nowTm.tm_yday;
    alarmLastFireMinute = alarmLastMinute;
    alarmLastFireDay = alarmLastDay;
  }
}

void snoozeAlarm() {
  if (alarmRingIdx < 0) return;
  alarmSnoozeIdx   = alarmRingIdx;
  alarmSnoozeUntil = millis() + 300000UL;
  dismissAlarm();
}

void alarmTick() {
  uint32_t ms = millis();

  // Ringing state must be serviced even if NTP/time becomes temporarily
  // unavailable. The popup and buzzer therefore never depend on Wi-Fi.
  if (alarmRinging) {
    if ((uint32_t)(ms - alarmBeepAt) >= 320UL) {
      alarmBeepAt = ms;
      alarmBeepHigh = !alarmBeepHigh;
      soundTone(alarmBeepHigh ? 2800 : 1700);
    }
    if (screen != SCR_ALARM) {
      screen = SCR_ALARM;
      tft.fillScreen(TH.bg);
      alarmDrawRinging();
    }
    static uint32_t redrawAt = 0;
    if ((int32_t)(ms - redrawAt) >= 0) {
      alarmDrawRinging();
      redrawAt = ms + 500;
    }
    return;
  }

  if (alarmSnoozeUntil && (int32_t)(ms - alarmSnoozeUntil) >= 0) {
    int idx = alarmSnoozeIdx;
    alarmSnoozeUntil = 0;
    alarmSnoozeIdx = -1;
    if (idx >= 0) triggerAlarm(idx);
    return;
  }

  // Alarm matching requires valid local time, but time is refreshed on every
  // pass. This also works immediately after the CYD/Web manual time is set.
  if (!timeValid()) return;

  const int nowMin = nowTm.tm_hour * 60 + nowTm.tm_min;
  const int today = nowTm.tm_yday;

  // Scan only once per minute, but do NOT mark a minute as handled before
  // checking the alarms. Enabling/editing an alarm during the current minute
  // will therefore fire correctly.
  if (nowMin == alarmLastFireMinute && today == alarmLastFireDay) return;
  alarmLastScanMs = ms;

  for (int i = 0; i < 3; ++i) {
    if (alarms[i].dateMode && alarms[i].dateDay == 0) alarms[i].dateDay = alarmTodayDay();
    if (!alarms[i].enabled) continue;
    if (alarms[i].hour != nowTm.tm_hour || alarms[i].minute != nowTm.tm_min) continue;
    bool match = alarms[i].dateMode ? (alarmTodayDay() == alarms[i].dateDay)
                                   : (alarms[i].days == 0x7F || (alarms[i].days & (1 << nowTm.tm_wday)));
    if (!match) continue;

    alarmLastFireMinute = nowMin;
    alarmLastFireDay = today;
    triggerAlarm(i);
    return;
  }

  alarmLastFireMinute = nowMin;
  alarmLastFireDay = today;
}

void alarmLoop() {
  if (alarmRinging) {
    if (!tc.press) return;
    int w = 288, h = 178, x = (SW - w) / 2, y = (SH - h) / 2;
    if (pressIn(x + 16, y + h - 44, 118, 32)) { beep(1200,20); snoozeAlarm(); alarmInit(); return; }
    if (pressIn(x + w - 134, y + h - 44, 118, 32)) { beep(1000,20); dismissAlarm(); alarmInit(); return; }
    return;
  }

  if (alarmEditIdx >= 0) {
    if (!tc.press) return;
    int i = alarmEditIdx;
    if (pressIn(18,140,62,30)) alarms[i].hour = (alarms[i].hour + 23) % 24;
    else if (pressIn(88,140,62,30)) alarms[i].hour = (alarms[i].hour + 1) % 24;
    else if (pressIn(170,140,62,30)) alarms[i].minute = (alarms[i].minute + 59) % 60;
    else if (pressIn(240,140,62,30)) alarms[i].minute = (alarms[i].minute + 1) % 60;
    else if (pressIn(18,181,88,30) && alarms[i].dateMode) { alarms[i].dateDay--; }
    else if (pressIn(18,181,88,30)) { beep(900,20); alarmEditIdx = -1; alarmInit(); return; }
    else if (pressIn(116,181,88,30) && alarms[i].dateMode) { alarms[i].dateMode = false; }
    else if (pressIn(116,181,88,30)) { alarms[i].dateMode = true; if (!alarms[i].dateDay) alarms[i].dateDay = alarmTodayDay(); }
    else if (pressIn(214,181,88,30) && alarms[i].dateMode) { alarms[i].dateDay++; }
    else if (pressIn(214,181,88,30)) { saveAlarms(); beep(1600,25); alarmEditIdx = -1; alarmInit(); return; }
    else return;
    // In DATE mode the lower buttons are DATE- / mode / DATE+. SAVE is available from web control.
    if (alarms[i].dateMode) { saveAlarms(); }
    else if (pressIn(116,181,88,30)) saveAlarms();
    beep(1300,15); alarmDrawEditor(); return;
  }

  if (!tc.press) return;
  if (pressIn(5, 211, 72, 24)) { beep(1500, 20); triggerAlarm(0); return; }
  for (int i = 0; i < 3; i++) {
    int y = 31 + i * 60, x = 5;
    if (pressIn(x + 272, y + 4, 31, 22)) {
      alarms[i].enabled = !alarms[i].enabled; saveAlarms(); beep(1400,20); alarmDrawRow(i); return;
    }
    if (pressIn(x + 6, y + 18, 106, 31)) { alarmEditIdx = i; beep(1300,20); alarmDrawEditor(); return; }
    if (pressIn(x + 118, y + 19, 34, 29)) { alarms[i].hour=(alarms[i].hour+23)%24; saveAlarms(); beep(1200,15); alarmDrawRow(i); return; }
    if (pressIn(x + 156, y + 19, 34, 29)) { alarms[i].hour=(alarms[i].hour+1)%24; saveAlarms(); beep(1200,15); alarmDrawRow(i); return; }
    if (pressIn(x + 194, y + 19, 34, 29)) { alarms[i].minute=(alarms[i].minute+59)%60; saveAlarms(); beep(1200,15); alarmDrawRow(i); return; }
    if (pressIn(x + 232, y + 19, 34, 29)) { alarms[i].minute=(alarms[i].minute+1)%60; saveAlarms(); beep(1200,15); alarmDrawRow(i); return; }
  }
}

// ============================ HOME ==========================================
struct Rect { int x, y, w, h; };
const Rect TILES[8] = {
  {  4, 54, 75, 88}, { 83, 54, 75, 88}, {162, 54, 75, 88}, {241, 54, 75, 88},
  {  4,146, 75, 88}, { 83,146, 75, 88}, {162,146, 75, 88}, {241,146, 75, 88}
};
const char* TILE_NAME[8] = {"Clock","Timer","Alarm","Weather","Games","Islamic","Settings","Knowledge"};

void drawTile(int i) {
  int x = TILES[i].x, y = TILES[i].y, w = TILES[i].w, h = TILES[i].h;
  tft.fillRoundRect(x, y, w, h, 12, TH.panel);
  int cx = x + w / 2, cy = y + 34;
  switch (i) {
    case 0: iconClock(cx, cy, TH.accent); break;
    case 1: iconTimer(cx, cy, TH.accent2); break;
    case 2: iconAlarm(cx, cy, TH.accent2); break;
    case 3: drawWxIcon(cx, cy, 4, 1); break;
    case 4: iconPad(cx, cy, TH.good, TH.panel); break;
    case 5: iconIslamic(cx, cy, TH.accent2, TH.panel); break;
    case 6: iconGear(cx, cy, TH.good, TH.panel); break;
    case 7:
      tft.drawCircle(cx, cy, 18, TH.accent);
      txt("?", cx, cy + 1, 4, TH.accent, TH.panel, MC_DATUM);
      break;
    default: break;
  }
  txt(TILE_NAME[i], cx, y + h - 14, 1, TH.text, TH.panel, MC_DATUM);
}
void homeInit() {
  tft.fillScreen(TH.bg);
  for (int i = 0; i < 8; i++) drawTile(i);
  homeHeader(true);
}
void homeHeader(bool force) {
  String t = "--:--", d = "Set time from Control";
  if (timeValid()) {
    char b[24];
    if (cfg.h24) snprintf(b, sizeof(b), "%02d:%02d", nowTm.tm_hour, nowTm.tm_min);
    else snprintf(b, sizeof(b), "%d:%02d %s", hour12(nowTm.tm_hour), nowTm.tm_min, nowTm.tm_hour >= 12 ? "PM" : "AM");
    t = String(b);
    snprintf(b, sizeof(b), "%s, %d %s", DOW3[nowTm.tm_wday], nowTm.tm_mday, MON3[nowTm.tm_mon]);
    d = String(b);
  } else if (WiFi.status() == WL_CONNECTED) d = "Syncing time...";

  static String lastT = "", lastD = "";
  static int lastWx = -9999, lastBars = -1;

  if (force || t != lastT) { lastT = t; txtp(t, 12, 6, 4, TH.text, TH.bg, TL_DATUM, 1, 140); }
  if (force || d != lastD) { lastD = d; txtp(d, 12, 34, 2, TH.dim, TH.bg, TL_DATUM, 1, 150); }

  int bars = WiFi.status() == WL_CONNECTED ? (WiFi.RSSI() / 5) : -99;
  if (force || bars != lastBars) {
    lastBars = bars;
    tft.fillRect(SW - 30, 6, 26, 24, TH.bg);
    drawWifi(SW - 22, 8);
  }

  int wt = (WiFi.status() == WL_CONNECTED && wx.ok) ? tempNum(wx.t) : -9999;
  if (force || wt != lastWx) {
    lastWx = wt;
    if (WiFi.status() == WL_CONNECTED && wx.ok) {
      tft.fillRect(SW - 110, 22, 106, 30, TH.bg);
      drawWxIcon(SW - 92, 30, 2, wmoIcon(wx.code));
      drawTemp(SW - 62, 26, wx.t, 2, 1, TH.text, TH.bg);
    }
  }
}
void homeLoop() {
  static uint32_t last = 0;
  if (millis() - last > 1000) { last = millis(); homeHeader(false); }
  if (!tc.press) return;
  for (int i = 0; i < 8; i++) {
    if (pressIn(TILES[i].x, TILES[i].y, TILES[i].w, TILES[i].h)) {
      beep(1500, 20);
      switch (i) {
        case 0: goScreen(SCR_CLOCK);  break;
        case 1: goScreen(SCR_TIMER);  break;
        case 2: goScreen(SCR_ALARM);  break;
        case 3: goScreen(SCR_WEATHER);break;
        case 4: goScreen(SCR_GAMES);  break;
        case 5: goScreen(SCR_ISLAMIC);break;
        case 6: goScreen(SCR_SETTINGS);break;
        case 7: goScreen(SCR_KNOWLEDGE);break;
        default: break;
      }
      return;
    }
  }
}

// ============================ CLOCK =========================================
int clkLastSec = -1;
bool clkMsgShown = false;
uint32_t clkLastFrame = 0;
int clkLastTemp = -9999;
const int CLOCK_CX = 160;

const char* CLOCK_STYLE_NAMES[20] = {
  "Digital", "Analog", "Big Digital", "Minimal", "Ring",
  "Neon", "Dashboard", "Split", "Clean", "Seconds",
  "Gear Train", "Pendulum", "Roman", "Moon Phase", "Compass",
  "Industrial", "Concentric", "Binary LED", "Sun Dial", "Balance"
};

String clockTimeString() {
  char b[20];
  if (cfg.h24) snprintf(b, sizeof(b), "%02d:%02d", nowTm.tm_hour, nowTm.tm_min);
  else snprintf(b, sizeof(b), "%d:%02d", hour12(nowTm.tm_hour), nowTm.tm_min);
  return String(b);
}
String clockSecondsString() {
  char b[8]; snprintf(b, sizeof(b), "%02d", nowTm.tm_sec); return String(b);
}
String clockDateLong() {
  char b[40];
  snprintf(b, sizeof(b), "%s  %02d %s %04d",
           DOW3[nowTm.tm_wday], nowTm.tm_mday, MON3[nowTm.tm_mon], nowTm.tm_year + 1900);
  return String(b);
}
String clockDateShort() {
  char b[32];
  snprintf(b, sizeof(b), "%s  %02d %s",
           DOW3[nowTm.tm_wday], nowTm.tm_mday, MON3[nowTm.tm_mon]);
  return String(b);
}
String clockTempString() {
  return String(tempNum(wx.t)) + (cfg.fahr ? " F" : " C");
}

void clockSpriteBegin() {
  if (clockSpriteReady) return;
  clockSprite.setColorDepth(16);
  if (clockSprite.createSprite(176, 176) != nullptr) clockSpriteReady = true;
}
void clockSpriteEnd() {
  if (!clockSpriteReady) return;
  clockSprite.deleteSprite(); clockSpriteReady = false;
}

void clockMarksNew(int cx, int cy, int r) {
  for (int i = 0; i < 60; i++) {
    float a = i * PI / 30.0f;
    float sn = sinf(a), cs = cosf(a);
    int r1 = (i % 5 == 0) ? r - 14 : r - 7;
    int r2 = r - 4;
    int x1 = cx + (int)(sn * r1), y1 = cy - (int)(cs * r1);
    int x2 = cx + (int)(sn * r2), y2 = cy - (int)(cs * r2);
    uint16_t c = (i % 5 == 0) ? TH.accent : TH.dim;
    if (i % 5 == 0) thickLine(x1, y1, x2, y2, 2, c);
    else tft.drawPixel(x2, y2, c);
  }
}
void clockFaceNew(int cx, int cy, int r) {
  tft.fillCircle(cx, cy, r, TH.panel);
  tft.drawCircle(cx, cy, r, TH.accent);
  tft.drawCircle(cx, cy, r - 2, TH.dim);
  clockMarksNew(cx, cy, r);
  txt("12", cx, cy - r + 15, 1, TH.text, TH.panel, MC_DATUM);
  txt("3",  cx + r - 13, cy, 1, TH.text, TH.panel, MC_DATUM);
  txt("6",  cx, cy + r - 15, 1, TH.text, TH.panel, MC_DATUM);
  txt("9",  cx - r + 13, cy, 1, TH.text, TH.panel, MC_DATUM);
}
void clockHandsSmooth(int cx, int cy, float h, float m, float s) {
  float ah = h * 30.0f * DEG_TO_RAD;
  float am = m * 6.0f * DEG_TO_RAD;
  float as = s * 6.0f * DEG_TO_RAD;
  thickLine(cx, cy, cx + (int)(sinf(ah) * 42), cy - (int)(cosf(ah) * 42), 4, TH.text);
  thickLine(cx, cy, cx + (int)(sinf(am) * 62), cy - (int)(cosf(am) * 62), 3, TH.accent);
  tft.drawLine(cx, cy, cx + (int)(sinf(as) * 72), cy - (int)(cosf(as) * 72), TH.accent2);
  tft.fillCircle(cx, cy, 5, TH.accent);
  tft.fillCircle(cx, cy, 2, TH.bg);
}

void drawAnalogSpriteFrame() {
  clockSpriteBegin();
  if (!clockSpriteReady) {
    tft.fillRect(0, BAR, SW, SH - BAR, TH.bg);
    clockFaceNew(108, 132, 88);
    float sF = nowTm.tm_sec + ((millis() % 1000UL) / 1000.0f);
    float mF = nowTm.tm_min + sF / 60.0f;
    float hF = (nowTm.tm_hour % 12) + mF / 60.0f;
    clockHandsSmooth(108, 132, hF, mF, sF);
    return;
  }
  const int sy = 44, cx = 88, cy = 88, r = 80;
  clockSprite.fillSprite(TH.bg);
  clockSprite.fillCircle(cx, cy, r, TH.panel);
  clockSprite.drawCircle(cx, cy, r, TH.accent);
  clockSprite.drawCircle(cx, cy, r - 2, TH.dim);
  for (int i = 0; i < 60; i++) {
    float a = i * PI / 30.0f;
    float sn = sinf(a), cs = cosf(a);
    int r1 = (i % 5 == 0) ? r - 13 : r - 7;
    int r2 = r - 4;
    int x1 = cx + (int)(sn * r1), y1 = cy - (int)(cs * r1);
    int x2 = cx + (int)(sn * r2), y2 = cy - (int)(cs * r2);
    uint16_t c = (i % 5 == 0) ? TH.accent : TH.dim;
    if (i % 5 == 0) {
      clockSprite.drawLine(x1, y1, x2, y2, c);
      clockSprite.drawLine(x1 + 1, y1, x2 + 1, y2, c);
      clockSprite.drawLine(x1, y1 + 1, x2, y2 + 1, c);
    } else clockSprite.drawPixel(x2, y2, c);
  }
  clockSprite.setTextDatum(MC_DATUM);
  clockSprite.setTextColor(TH.text, TH.panel);
  clockSprite.setTextSize(1);
  clockSprite.drawString("12", cx, cy - r + 14, 1);
  clockSprite.drawString("3",  cx + r - 12, cy, 1);
  clockSprite.drawString("6",  cx, cy + r - 14, 1);
  clockSprite.drawString("9",  cx - r + 12, cy, 1);
  float secF = nowTm.tm_sec + ((millis() % 1000UL) / 1000.0f);
  float minF = nowTm.tm_min + secF / 60.0f;
  float hourF = (nowTm.tm_hour % 12) + minF / 60.0f;
  float ah = hourF * 30.0f * DEG_TO_RAD;
  float am = minF  * 6.0f  * DEG_TO_RAD;
  float as = secF  * 6.0f  * DEG_TO_RAD;
  int hx = cx + (int)(sinf(ah) * 42), hy = cy - (int)(cosf(ah) * 42);
  int mx = cx + (int)(sinf(am) * 62), my = cy - (int)(cosf(am) * 62);
  int sx2 = cx + (int)(sinf(as) * 72), sy2 = cy - (int)(cosf(as) * 72);
  clockSprite.drawLine(cx, cy, hx, hy, TH.text);
  clockSprite.drawLine(cx+1, cy, hx+1, hy, TH.text);
  clockSprite.drawLine(cx, cy+1, hx, hy+1, TH.text);
  clockSprite.drawLine(cx, cy, mx, my, TH.accent);
  clockSprite.drawLine(cx+1, cy, mx+1, my, TH.accent);
  clockSprite.drawLine(cx, cy+1, mx, my+1, TH.accent);
  clockSprite.drawLine(cx, cy, sx2, sy2, TH.accent2);
  clockSprite.fillCircle(cx, cy, 5, TH.accent);
  clockSprite.fillCircle(cx, cy, 2, TH.bg);
  clockSprite.pushSprite(20, sy);
  txtp(clockTimeString(), 260, 76, 4, TH.text, TH.bg, MC_DATUM, 1, 90);
  if (!cfg.h24) {
    txtp(nowTm.tm_hour >= 12 ? "PM" : "AM", 260, 100, 2, TH.accent2, TH.bg, MC_DATUM, 1, 70);
  } else tft.fillRect(238, 90, 44, 24, TH.bg);
  txtp(DOW3[nowTm.tm_wday], 260, 137, 2, TH.accent, TH.bg, MC_DATUM, 1, 70);
  txtp(clockDateShort(), 260, 162, 1, TH.dim, TH.bg, MC_DATUM, 1, 100);
}

void clockDrawStatic() {
  tft.fillRect(0, BAR, SW, SH - BAR, TH.bg);
  switch (cfg.clockStyle) {
    case 0:
      tft.drawRoundRect(8, BAR + 8, SW - 16, SH - BAR - 16, 14, TH.panel);
      txtp(clockDateLong(), CLOCK_CX, 164, 2, TH.text, TH.bg, MC_DATUM, 1, 296);
      if (wx.ok) txtp(clockTempString(), CLOCK_CX, 194, 2, TH.dim, TH.bg, MC_DATUM, 1, 296);
      break;
    case 1: break;
    case 2:
      txtp(clockDateLong(), CLOCK_CX, 170, 2, TH.text, TH.bg, MC_DATUM, 1, 296);
      if (wx.ok) txtp(clockTempString(), CLOCK_CX, 201, 2, TH.dim, TH.bg, MC_DATUM, 1, 296);
      break;
    case 3:
      txtp(clockDateLong(), CLOCK_CX, 181, 2, TH.dim, TH.bg, MC_DATUM, 1, 296);
      break;
    case 4: {
      const int cx = CLOCK_CX, cy = 103, r = 61;
      tft.fillCircle(cx, cy, r, TH.panel);
      tft.drawCircle(cx, cy, r, TH.accent);
      tft.drawCircle(cx, cy, r - 2, TH.dim);
      for (int i = 0; i < 60; i += 5) {
        float a = i * PI / 30.0f;
        int x = cx + (int)(cosf(a) * (r - 7));
        int y = cy + (int)(sinf(a) * (r - 7));
        tft.fillCircle(x, y, 2, TH.accent);
      }
      txtp(clockDateShort(), cx, 208, 1, TH.dim, TH.bg, MC_DATUM, 1, 296);
    } break;
    case 5:
      tft.drawFastHLine(38, 177, 244, TH.accent);
      txtp(clockDateLong(), CLOCK_CX, 198, 2, TH.text, TH.bg, MC_DATUM, 1, 296);
      break;
    case 6:
      tft.drawRoundRect(8, BAR + 8, SW - 16, SH - BAR - 16, 14, TH.panel);
      txt("TIME", 30, 48, 1, TH.dim, TH.bg, ML_DATUM);
      txt("DATE", 30, 142, 1, TH.dim, TH.bg, ML_DATUM);
      txtp(clockDateLong(), 30, 168, 2, TH.accent, TH.bg, ML_DATUM, 1, 296);
      if (wx.ok) txtp("TEMP  " + clockTempString(), 30, 200, 1, TH.dim, TH.bg, ML_DATUM, 1, 296);
      break;
    case 7:
      tft.fillRoundRect(12, BAR + 10, 296, 82, 12, TH.panel);
      tft.fillRoundRect(12, BAR + 101, 296, 88, 12, TH.panel);
      txtp(clockDateLong(), CLOCK_CX, 151, 2, TH.text, TH.panel, MC_DATUM, 1, 296);
      if (wx.ok) txtp(clockTempString(), CLOCK_CX, 179, 2, TH.dim, TH.panel, MC_DATUM, 1, 296);
      break;
    case 8:
      tft.fillRoundRect(96, 128, 128, 3, 2, TH.accent);
      txtp(clockDateLong(), CLOCK_CX, 158, 2, TH.dim, TH.bg, MC_DATUM, 1, 296);
      if (wx.ok) txtp(clockTempString(), CLOCK_CX, 191, 2, TH.accent, TH.bg, MC_DATUM, 1, 296);
      break;
    case 9:
      txtp("SECONDS", CLOCK_CX, 139, 1, TH.dim, TH.bg, MC_DATUM, 1, 296);
      break;
  }
}

void clockDrawDynamic() {
  switch (cfg.clockStyle) {
    case 0:
      txtp(clockTimeString(), CLOCK_CX, 93, 5, TH.accent, TH.bg, MC_DATUM, 1, 296);
      txtp(clockSecondsString(), CLOCK_CX, 130, 2, TH.accent2, TH.bg, MC_DATUM, 1, 120);
      break;
    case 2:
      txtp(clockTimeString(), CLOCK_CX, 91, 6, TH.accent, TH.bg, MC_DATUM, 1, 296);
      txtp(clockSecondsString(), CLOCK_CX, 132, 4, TH.accent2, TH.bg, MC_DATUM, 1, 140);
      break;
    case 3:
      txtp(clockTimeString(), CLOCK_CX, 104, 7, TH.text, TH.bg, MC_DATUM, 1, 296);
      txtp(clockSecondsString(), CLOCK_CX, 145, 3, TH.accent, TH.bg, MC_DATUM, 1, 120);
      break;
    case 4: {
      const int cx = CLOCK_CX, cy = 103, r = 61;
      tft.fillCircle(cx, cy, r - 8, TH.panel);
      float secF = nowTm.tm_sec + ((millis() % 1000UL) / 1000.0f);
      float minF = nowTm.tm_min + secF / 60.0f;
      float hourF = (nowTm.tm_hour % 12) + minF / 60.0f;
      float sa = (secF * 6.0f - 90.0f) * DEG_TO_RAD;
      float ma = (minF * 6.0f - 90.0f) * DEG_TO_RAD;
      float ha = (hourF * 30.0f - 90.0f) * DEG_TO_RAD;
      thickLine(cx, cy, cx + (int)(30 * cosf(ha)), cy + (int)(30 * sinf(ha)), 4, TH.text);
      thickLine(cx, cy, cx + (int)(45 * cosf(ma)), cy + (int)(45 * sinf(ma)), 3, TH.accent);
      tft.drawLine(cx, cy, cx + (int)(53 * cosf(sa)), cy + (int)(53 * sinf(sa)), TH.good);
      tft.fillCircle(cx, cy, 4, TH.text);
      txtp(clockTimeString(), cx, 181, 2, TH.text, TH.bg, MC_DATUM, 1, 296);
    } break;
    case 5:
      txtp(clockTimeString(), CLOCK_CX, 96, 7, TH.accent, TH.bg, MC_DATUM, 1, 296);
      txtp(clockSecondsString(), CLOCK_CX, 151, 4, TH.accent2, TH.bg, MC_DATUM, 1, 140);
      break;
    case 6:
      txtp(clockTimeString(), 30, 91, 6, TH.text, TH.bg, ML_DATUM, 1, 240);
      txtp(clockSecondsString(), 287, 91, 4, TH.accent2, TH.bg, MR_DATUM, 1, 100);
      break;
    case 7:
      txtp(clockTimeString(), CLOCK_CX, 72, 5, TH.accent, TH.panel, MC_DATUM, 1, 296);
      txtp(clockSecondsString(), CLOCK_CX, 111, 3, TH.accent2, TH.panel, MC_DATUM, 1, 120);
      break;
    case 8:
      txtp(clockTimeString(), CLOCK_CX, 98, 6, TH.text, TH.bg, MC_DATUM, 1, 296);
      break;
    case 9:
      txtp(clockTimeString(), CLOCK_CX, 91, 5, TH.text, TH.bg, MC_DATUM, 1, 296);
      txtp(clockSecondsString(), CLOCK_CX, 174, 7, TH.accent2, TH.bg, MC_DATUM, 1, 200);
      break;
  }
}

void drawAnimatedClockNew(int style) {
  // All new animated clocks use the same 176x176 double-buffered sprite as
  // the original smooth analog clock. This prevents full-screen redraw
  // flicker and keeps animation fluid on the CYD ESP32.
  clockSpriteBegin();
  if (!clockSpriteReady) return;

  const int cx = 88, cy = 88, r = 78;
  const uint32_t ms = millis();
  const float t = ms / 1000.0f;
  const float secF = nowTm.tm_sec + fmodf(ms, 1000.0f) / 1000.0f;
  const float minF = nowTm.tm_min + secF / 60.0f;
  const float hourF = (nowTm.tm_hour % 12) + minF / 60.0f;

  clockSprite.fillSprite(TH.bg);

  if (style == 110) { // Orbit
    clockSprite.drawCircle(cx, cy, 68, TH.panel);
    clockSprite.drawCircle(cx, cy, 52, TH.dim);
    for (int i = 0; i < 3; ++i) {
      float a = t * (0.8f + i * 0.23f) + i * 2.0943951f;
      int rr = 42 + i * 7;
      int x = cx + (int)(cosf(a) * rr);
      int y = cy + (int)(sinf(a) * rr);
      uint16_t col = (i == 0) ? TH.accent : (i == 1 ? TH.accent2 : TH.good);
      clockSprite.fillCircle(x, y, 4 + i, col);
    }
    clockSprite.setTextDatum(MC_DATUM);
    clockSprite.setTextColor(TH.text, TH.bg);
    clockSprite.drawString(clockTimeString(), cx, cy, 4);
  }
  else if (style == 111) { // Pulse
    float p = 0.5f + 0.5f * sinf(t * 3.0f);
    int pr = 18 + (int)(p * 18.0f);
    for (int i = 3; i >= 0; --i) {
      int rr = pr + i * 12;
      uint16_t col = (i == 0) ? TH.accent : (i == 1 ? TH.accent2 : TH.panel);
      clockSprite.drawCircle(cx, cy, rr, col);
    }
    clockSprite.fillCircle(cx, cy, 8, TH.panel);
    clockSprite.setTextDatum(MC_DATUM);
    clockSprite.setTextColor(TH.text, TH.panel);
    clockSprite.drawString(clockTimeString(), cx, cy, 4);
  }
  else if (style == 112) { // Matrix rain
    for (int i = 0; i < 13; ++i) {
      float speed = 0.55f + (i % 4) * 0.17f;
      float yy = fmodf(t * 34.0f * speed + i * 29.0f, 190.0f) - 15.0f;
      int xx = 8 + i * 13;
      int len = 3 + (i % 5);
      for (int k = 0; k < len; ++k) {
        int y = (int)yy - k * 8;
        if (y >= 0 && y < 176) clockSprite.fillRect(xx, y, 3, 5, k == 0 ? TH.accent : TH.panel);
      }
    }
    clockSprite.fillRoundRect(20, 65, 136, 52, 10, TH.bg);
    clockSprite.drawRoundRect(20, 65, 136, 52, 10, TH.accent);
    clockSprite.setTextDatum(MC_DATUM);
    clockSprite.setTextColor(TH.text, TH.bg);
    clockSprite.drawString(clockTimeString(), cx, 84, 4);
    clockSprite.setTextColor(TH.accent2, TH.bg);
    clockSprite.drawString(clockSecondsString(), cx, 106, 2);
  }
  else if (style == 113) { // Solar system
    clockSprite.drawCircle(cx, cy, 68, TH.panel);
    clockSprite.fillCircle(cx, cy, 19, TH.accent);
    for (int i = 1; i <= 3; ++i) {
      int rr = 30 + i * 14;
      clockSprite.drawCircle(cx, cy, rr, TH.dim);
      float a = (t * (0.35f + i * 0.18f)) + i * 1.8f;
      int x = cx + (int)(cosf(a) * rr);
      int y = cy + (int)(sinf(a) * rr);
      clockSprite.fillCircle(x, y, 3 + i / 2, i == 1 ? TH.accent2 : TH.good);
    }
    clockSprite.setTextDatum(MC_DATUM);
    clockSprite.setTextColor(TH.text, TH.bg);
    clockSprite.drawString(clockTimeString(), cx, 151, 2);
  }
  else if (style == 114) { // Radar
    clockSprite.drawCircle(cx, cy, 70, TH.panel);
    clockSprite.drawCircle(cx, cy, 48, TH.dim);
    clockSprite.drawCircle(cx, cy, 25, TH.dim);
    float a = fmodf(t * 1.7f, 2.0f * PI);
    for (int k = 0; k < 14; ++k) {
      float aa = a - k * 0.055f;
      uint16_t col = (k < 3) ? TH.accent : TH.panel;
      int x = cx + (int)(cosf(aa) * 68);
      int y = cy + (int)(sinf(aa) * 68);
      clockSprite.drawLine(cx, cy, x, y, col);
    }
    clockSprite.fillCircle(cx + (int)(cosf(a) * 53), cy + (int)(sinf(a) * 53), 4, TH.accent2);
    clockSprite.fillCircle(cx, cy, 3, TH.good);
    clockSprite.setTextDatum(MC_DATUM);
    clockSprite.setTextColor(TH.text, TH.bg);
    clockSprite.drawString(clockTimeString(), cx, cy, 3);
  }
  else if (style == 115) { // Flip-style digital card
    float p = 0.5f + 0.5f * sinf(t * PI);
    int split = 87 + (int)(p * 5.0f);
    clockSprite.fillRoundRect(10, 30, 156, 116, 14, TH.panel);
    clockSprite.drawRoundRect(10, 30, 156, 116, 14, TH.accent);
    clockSprite.drawFastHLine(16, split, 144, TH.dim);
    clockSprite.setTextDatum(MC_DATUM);
    clockSprite.setTextColor(TH.text, TH.panel);
    clockSprite.drawString(clockTimeString(), cx, 78, 5);
    clockSprite.setTextColor(TH.accent2, TH.panel);
    clockSprite.drawString(clockSecondsString(), cx, 111, 2);
  }
  else if (style == 116) { // Equalizer bars
    for (int i = 0; i < 7; ++i) {
      float wave = 0.5f + 0.5f * sinf(t * 2.6f + i * 0.72f);
      int h = 20 + (int)(wave * 72.0f);
      int x = 10 + i * 25;
      uint16_t col = (i & 1) ? TH.accent : TH.accent2;
      clockSprite.fillRoundRect(x, 138 - h, 16, h, 5, col);
    }
    clockSprite.setTextDatum(MC_DATUM);
    clockSprite.setTextColor(TH.text, TH.bg);
    clockSprite.drawString(clockTimeString(), cx, 155, 3);
  }
  else if (style >= 10 && style <= 19) { // 10 genuinely different skeleton clocks
    const int v = style - 10;
    const float TAU = 2.0f * PI;
    const float secA = (secF / 60.0f) * TAU - PI / 2.0f;
    const float minA = (minF / 60.0f) * TAU - PI / 2.0f;
    const float hourA = (hourF / 12.0f) * TAU - PI / 2.0f;
    auto hand = [&](float a, int len, int w, uint16_t col) {
      int x = cx + (int)(cosf(a) * len);
      int y = cy + (int)(sinf(a) * len);
      for (int q = -w; q <= w; ++q) clockSprite.drawLine(cx + q, cy + q, x + q, y + q, col);
    };

    // All ten variants share the same smooth sprite, but each has a different face/mechanism.
    if (v == 0) { // Gear train
      clockSprite.drawCircle(cx, cy, 72, TH.panel);
      for (int g=0; g<4; ++g) {
        int gx = cx + (g<2 ? -27 : 27), gy = cy + (g%2 ? 25 : -24), rr = 18 + (g%3)*3;
        float ga = t * (g&1 ? -1.2f : 1.2f) + g;
        clockSprite.drawCircle(gx,gy,rr,TH.accent);
        clockSprite.drawCircle(gx,gy,rr-5,TH.bg);
        for(int n=0;n<8;n++){float a=ga+n*PI/4; int x1=gx+cosf(a)*rr,y1=gy+sinf(a)*rr; int x2=gx+cosf(a)*(rr+4),y2=gy+sinf(a)*(rr+4);clockSprite.drawLine(x1,y1,x2,y2,TH.accent2);}
      }
      hand(hourA,28,2,TH.text); hand(minA,48,1,TH.accent); hand(secA,61,0,TH.accent2);
    } else if (v == 1) { // Pendulum
      clockSprite.drawRoundRect(30,14,116,140,18,TH.panel);
      clockSprite.drawRoundRect(38,22,100,124,14,TH.dim);
      clockSprite.drawCircle(cx,70,48,TH.bg);
      for(int n=0;n<12;n++){float a=n*PI/6-PI/2;clockSprite.drawPixel(cx+cosf(a)*43,70+sinf(a)*43,TH.dim);}
      hand(hourA,24,2,TH.text); hand(minA,38,1,TH.accent); hand(secA,43,0,TH.accent2);
      float pa=sinf(t*2.4f)*0.48f; int px=cx+(int)(sinf(pa)*38); int py=103+(int)(cosf(pa)*18);
      clockSprite.drawLine(cx,96,px,py,TH.accent); clockSprite.fillCircle(px,py,9,TH.accent2);
    } else if (v == 2) { // Roman
      clockSprite.fillCircle(cx,cy,73,TH.panel); clockSprite.drawCircle(cx,cy,70,TH.accent);
      const char* rn[12]={"XII","I","II","III","IV","V","VI","VII","VIII","IX","X","XI"};
      clockSprite.setTextDatum(MC_DATUM); clockSprite.setTextColor(TH.text,TH.panel);
      for(int n=0;n<12;n++){float a=n*PI/6-PI/2;clockSprite.drawString(rn[n],cx+cosf(a)*57,cy+sinf(a)*57,1);}
      hand(hourA,30,2,TH.text); hand(minA,48,1,TH.accent); hand(secA,61,0,TH.accent2);
    } else if (v == 3) { // Moon phases
      clockSprite.drawCircle(cx,cy,72,TH.panel);
      for(int i=0;i<8;i++){float a=i*PI/4;clockSprite.drawLine(cx+cosf(a)*61,cy+sinf(a)*61,cx+cosf(a)*68,cy+sinf(a)*68,TH.dim);}
      float ph=0.5f+0.5f*sinf(t*0.35f); int rr=25; clockSprite.fillCircle(cx,cy,rr,TH.text);
      clockSprite.fillCircle(cx+(int)((ph-0.5f)*38),cy,rr,TH.panel);
      hand(hourA,31,2,TH.accent); hand(minA,51,1,TH.accent2); hand(secA,63,0,TH.good);
    } else if (v == 4) { // Nautical compass
      clockSprite.drawCircle(cx,cy,72,TH.panel); clockSprite.drawCircle(cx,cy,58,TH.dim);
      for(int n=0;n<16;n++){float a=n*PI/8; int l=(n%4==0)?66:61;clockSprite.drawLine(cx+cosf(a)*54,cy+sinf(a)*54,cx+cosf(a)*l,cy+sinf(a)*l,TH.dim);}
      float wave=sinf(t*1.5f)*8;
      clockSprite.fillTriangle(cx,cy-48,cx-7,cy-27,cx+7,cy-27,TH.accent);
      clockSprite.drawLine(cx,cy,cx+(int)(cosf(secA)*62),cy+(int)(sinf(secA)*62),TH.accent2);
      hand(hourA,28,2,TH.text); hand(minA,49,1,TH.accent);
      clockSprite.drawArc(cx,cy+20,25,18,(int)wave+20,(int)wave+160,TH.good,TH.bg); // gentle wave accent
    } else if (v == 5) { // Industrial gauge
      clockSprite.fillRoundRect(10,30,156,110,16,TH.panel);
      clockSprite.drawRoundRect(10,30,156,110,16,TH.dim);
      for(int n=0;n<=10;n++){float a=PI*0.75f + n*(PI*1.5f/10);int x1=cx+cosf(a)*48,y1=cy+sin(a)*48;int x2=cx+cosf(a)*61,y2=cy+sinf(a)*61;clockSprite.drawLine(x1,y1,x2,y2,TH.dim);}
      float gauge=PI*0.75f + (secF/60.0f)*PI*1.5f;
      clockSprite.drawLine(cx,cy,cx+cosf(gauge)*52,cy+sinf(gauge)*52,TH.accent2); clockSprite.fillCircle(cx,cy,5,TH.good);
      hand(hourA,24,2,TH.text); hand(minA,40,1,TH.accent);
    } else if (v == 6) { // Concentric rings
      for(int i=0;i<6;i++){int rr=70-i*10;float off=t*(0.3f+i*0.05f);clockSprite.drawCircle(cx,cy,rr,(i&1)?TH.accent:TH.panel);clockSprite.drawArc(cx,cy,rr,rr-2,(int)(off*57.3f)%360,(int)(off*57.3f+120)%360,TH.accent2,TH.bg);}
      hand(hourA,28,2,TH.text); hand(minA,47,1,TH.accent); hand(secA,63,0,TH.good);
    } else if (v == 7) { // Binary / LED skeleton
      clockSprite.fillRoundRect(9,24,158,128,12,TH.panel);
      clockSprite.setTextDatum(MC_DATUM); clockSprite.setTextColor(TH.dim,TH.panel); clockSprite.drawString("H  M  S",cx,42,1);
      int vals[3]={nowTm.tm_hour%24,nowTm.tm_min,nowTm.tm_sec};
      for(int row=0;row<6;row++) for(int col=0;col<3;col++){bool on=(vals[col]>>(5-row))&1;int x=57+col*28,y=58+row*13;clockSprite.fillCircle(x,y,4,on?TH.accent:TH.dim);}
      clockSprite.setTextColor(TH.text,TH.panel); clockSprite.drawString(clockTimeString(),cx,135,2);
    } else if (v == 8) { // Sun dial
      clockSprite.fillCircle(cx,cy,71,TH.panel); clockSprite.drawCircle(cx,cy,68,TH.accent);
      for(int n=0;n<24;n++){float a=n*PI/12-PI/2;int rr=(n%3==0)?62:66;clockSprite.drawLine(cx+cosf(a)*rr,cy+sinf(a)*rr,cx+cosf(a)*70,cy+sinf(a)*70,TH.dim);}
      float sunA=(hourF/12.0f)*TAU-PI/2; int sx=cx+(int)cosf(sunA)*46, sy=cy+(int)sinf(sunA)*46;
      clockSprite.fillCircle(sx,sy,9,TH.accent2); hand(hourA,28,2,TH.text); hand(minA,49,1,TH.accent); hand(secA,62,0,TH.good);
    } else { // Skeleton balance / escapement
      clockSprite.drawCircle(cx,cy,72,TH.panel); clockSprite.drawCircle(cx,cy,69,TH.dim);
      float ea=sinf(t*3.2f)*0.55f;
      int ex=cx+(int)(sinf(ea)*42), ey=cy+39+(int)(cosf(ea)*10);
      clockSprite.drawLine(cx,cy+14,ex,ey,TH.accent); clockSprite.fillCircle(ex,ey,10,TH.accent2);
      clockSprite.drawCircle(cx,cy,31,TH.dim);
      hand(hourA,27,2,TH.text); hand(minA,49,1,TH.accent); hand(secA,63,0,TH.good);
      clockSprite.fillCircle(cx,cy,5,TH.accent2);
    }
    clockSprite.setTextDatum(MC_DATUM); clockSprite.setTextColor(TH.text,TH.panel);
    clockSprite.drawString(clockTimeString(), cx, 154, 2);
  }
  else { // Arc
    for (int i = 0; i < 4; ++i) {
      int rr = 28 + i * 15;
      float center = t * (0.8f + i * 0.13f);
      float span = 1.6f + 0.55f * sinf(t * 1.8f + i);
      for (int k = 0; k < 50; ++k) {
        float u = -span * 0.5f + span * k / 49.0f;
        float a = center + u;
        int x = cx + (int)(cosf(a) * rr);
        int y = cy + (int)(sinf(a) * rr);
        clockSprite.drawPixel(x, y, (i & 1) ? TH.accent2 : TH.accent);
      }
    }
    clockSprite.setTextDatum(MC_DATUM);
    clockSprite.setTextColor(TH.text, TH.bg);
    clockSprite.drawString(clockTimeString(), cx, cy, 4);
  }

  clockSprite.pushSprite(72, 38);
  txtp(clockDateShort(), CLOCK_CX, 218, 1, TH.dim, TH.bg, MC_DATUM, 1, 296);
}

void clockInit() {
  if (cfg.clockStyle >= 20) cfg.clockStyle = 0;
  drawBar(CLOCK_STYLE_NAMES[cfg.clockStyle], nullptr, true);
  clkLastSec = -1; clkMsgShown = false; clkLastFrame = 0; clkLastTemp = -9999;
  if (cfg.clockStyle == 1 || cfg.clockStyle >= 10) clockSpriteBegin(); else clockSpriteEnd();
  tft.fillRect(0, BAR, SW, SH - BAR, TH.bg);
  if (timeValid()) {
    clockDrawStatic();
    clockDrawDynamic();
    if (cfg.clockStyle == 1) drawAnalogSpriteFrame();
  }
}

void clockLoop() {
  if (tc.press && tc.y <= BAR && tc.x > 220) {
    cfg.theme = (cfg.theme + 1) % NTHEMES;
    saveCfg(); beep(1700, 18); clockInit(); return;
  }
  if (tc.press && tc.y > BAR + 4) {
    cfg.clockStyle = (cfg.clockStyle + 1) % 20;
    saveCfg(); beep(1300, 20); clockInit(); return;
  }
  if (!timeValid()) {
    if (!clkMsgShown) {
      clkMsgShown = true;
      tft.fillRect(0, BAR, SW, SH - BAR, TH.bg);
      txt("Syncing time...", CLOCK_CX, 115, 2, TH.dim, TH.bg, MC_DATUM);
      if (WiFi.status() != WL_CONNECTED)
        txt("Set time from Control", CLOCK_CX, 140, 1, TH.dim, TH.bg, MC_DATUM);
    }
    return;
  }
  if (clkMsgShown) { clkMsgShown = false; clockInit(); return; }

  if (cfg.clockStyle >= 10) {
    uint32_t nowMs = millis();
    if ((uint32_t)(nowMs - clkLastFrame) < 33UL) return;
    clkLastFrame = nowMs;
    drawAnimatedClockNew(cfg.clockStyle);
    return;
  }
  if (cfg.clockStyle == 1) {
    uint32_t nowMs = millis();
    if ((uint32_t)(nowMs - clkLastFrame) < 33UL) return;
    clkLastFrame = nowMs;
    drawAnalogSpriteFrame();
    return;
  }
  if (nowTm.tm_sec == clkLastSec) return;
  clkLastSec = nowTm.tm_sec;
  clockDrawDynamic();
  if (wx.ok && tempNum(wx.t) != clkLastTemp) {
    clkLastTemp = tempNum(wx.t);
    if (cfg.clockStyle == 0)      txtp(clockTempString(), CLOCK_CX, 194, 2, TH.dim, TH.bg, MC_DATUM, 1, 296);
    else if (cfg.clockStyle == 2) txtp(clockTempString(), CLOCK_CX, 201, 2, TH.dim, TH.bg, MC_DATUM, 1, 296);
    else if (cfg.clockStyle == 6) txtp("TEMP  " + clockTempString(), 30, 200, 1, TH.dim, TH.bg, ML_DATUM, 1, 296);
    else if (cfg.clockStyle == 7) txtp(clockTempString(), CLOCK_CX, 179, 2, TH.dim, TH.panel, MC_DATUM, 1, 296);
    else if (cfg.clockStyle == 8) txtp(clockTempString(), CLOCK_CX, 191, 2, TH.accent, TH.bg, MC_DATUM, 1, 296);
  }
}

// ============================ TIMER =========================================
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
const char* wxModeName(uint8_t m) {
  const char* n[8] = {"NOW", "7-DAY", "CONDITIONS", "RAIN / UV", "SUN", "WIND", "DETAILS", "EXTRA"};
  return n[m % 8];
}
String wxShortCondition(int c) {
  String s = wmoText(c);
  if (s.length() > 15) s = s.substring(0, 15);
  return s;
}
void wxCard(int x, int y, int w, int h) {
  tft.fillRoundRect(x, y, w, h, 12, TH.panel);
  tft.drawRoundRect(x, y, w, h, 12, TH.dim);
}
void wxLabelValue(int x, int y, const String& label, const String& value, bool accent=false) {
  txt(label, x, y, 1, TH.dim, TH.panel, TL_DATUM);
  txtp(value, x, y + 15, 2, accent ? TH.accent : TH.text, TH.panel, TL_DATUM, 1, 90);
}
void wxDraw() {
  clearContent();
  drawBar("WEATHER", "REF", false);
  txtp(savedCity(), 8, 38, 1, TH.text, TH.bg, TL_DATUM, 1, 130);
  txtp(String(wxModeName(wxMode)), 312, 38, 1, TH.accent, TH.bg, TR_DATUM, 1, 110);
  if (wx.stamp[0]) txtp(String("UPD ") + wx.stamp, 250, BAR / 2 + 1, 1, TH.dim, TH.panel, MR_DATUM, 1, 60);

  if (!wx.ok) {
    wxCard(14, 62, 292, 128);
    drawWxIcon(70, 112, 6, 6);
    txt("WEATHER OFFLINE", 180, 96, 2, TH.text, TH.panel, MC_DATUM);
    txt(WiFi.status() == WL_CONNECTED ? "Tap REFRESH" : "CONNECT WI-FI", 180, 122, 1, TH.dim, TH.panel, MC_DATUM);
    button(103, 146, 114, 28, "REFRESH", TH.accent, TH.onacc, 2);
    txt("Tap screen to change mode", 160, 218, 1, TH.dim, TH.bg, MC_DATUM);
    return;
  }

  if (wxMode == 0) {
    wxCard(8, 50, 304, 90);
    drawWxIcon(48, 91, 6, wmoIcon(wx.code));
    drawTemp(94, 57, wx.t, 4, 2, TH.text, TH.panel);
    txt(wxShortCondition(wx.code), 94, 111, 1, TH.accent, TH.panel, TL_DATUM);
    txt(String("FEELS ") + tempNum(wx.feels) + (cfg.fahr ? "F" : "C"), 94, 126, 1, TH.dim, TH.panel, TL_DATUM);
    wxCard(8, 148, 73, 48); wxCard(85, 148, 73, 48); wxCard(162, 148, 73, 48); wxCard(239, 148, 73, 48);
    txt("H/L", 44, 157, 1, TH.dim, TH.panel, MC_DATUM); txt(String(tempNum(wx.hi[0]))+"/"+String(tempNum(wx.lo[0])),44,178,1,TH.text,TH.panel,MC_DATUM);
    txt("HUM",121,157,1,TH.dim,TH.panel,MC_DATUM); txt(String(wx.hum)+"%",121,178,1,TH.text,TH.panel,MC_DATUM);
    txt("WIND",198,157,1,TH.dim,TH.panel,MC_DATUM); txt(String((int)lroundf(wx.wind))+"",198,178,1,TH.text,TH.panel,MC_DATUM);
    txt("UV",275,157,1,TH.dim,TH.panel,MC_DATUM); txt(String(wx.uv,1),275,178,1,TH.text,TH.panel,MC_DATUM);
  } else if (wxMode == 1) {
    txt("7 DAY FORECAST", 160, 51, 1, TH.dim, TH.bg, MC_DATUM);
    for (int i=0;i<7;i++) {
      int x=3+i*45; wxCard(x,62,42,134);
      txt(i==0?"NOW":DOW3[wx.dow[i]],x+21,73,1,TH.accent,TH.panel,MC_DATUM);
      drawWxIcon(x+21,108,3,wmoIcon(i==0?wx.code:wx.dcode[i]));
      txt(String(tempNum(wx.hi[i]))+"°",x+21,149,1,TH.text,TH.panel,MC_DATUM);
      txt(String(tempNum(wx.lo[i]))+"°",x+21,168,1,TH.dim,TH.panel,MC_DATUM);
      txt(String(wx.rainProb[i])+"%",x+21,187,1,TH.accent2,TH.panel,MC_DATUM);
    }
  } else if (wxMode == 2) {
    wxCard(8, 52, 304, 145);
    txt("LIVE CONDITIONS",160,68,1,TH.accent,TH.panel,MC_DATUM);
    wxLabelValue(22,88,"TEMPERATURE",String(tempNum(wx.t))+(cfg.fahr?" F":" C"),true);
    wxLabelValue(118,88,"FEELS LIKE",String(tempNum(wx.feels))+(cfg.fahr?" F":" C"));
    wxLabelValue(214,88,"HUMIDITY",String(wx.hum)+"%");
    txt("HUMIDITY",22,132,1,TH.dim,TH.panel,TL_DATUM); tft.fillRoundRect(22,143,126,8,4,TH.bg); tft.fillRoundRect(22,143,min(126,wx.hum*126/100),8,4,TH.accent);
    txt("WIND",170,132,1,TH.dim,TH.panel,TL_DATUM); tft.fillRoundRect(170,143,126,8,4,TH.bg); tft.fillRoundRect(170,143,min(126,(int)lroundf(wx.wind*126.0f/60.0f)),8,4,TH.accent2);
    txt("PRESSURE",22,168,1,TH.dim,TH.panel,TL_DATUM); txt(String((int)lroundf(wx.pressure))+" hPa",22,183,1,TH.text,TH.panel,TL_DATUM);
    txt("VISIBILITY",170,168,1,TH.dim,TH.panel,TL_DATUM); txt(String((int)lroundf(wx.visibility/1000.0f))+" km",170,183,1,TH.text,TH.panel,TL_DATUM);
  } else if (wxMode == 3) {
    wxCard(8,52,148,145); wxCard(164,52,148,145);
    txt("RAIN",82,69,1,TH.accent,TH.panel,MC_DATUM);
    txt(String(wx.rainProb[0])+"%",82,108,5,TH.text,TH.panel,MC_DATUM);
    txt("TODAY",82,135,1,TH.dim,TH.panel,MC_DATUM);
    txt(String(wx.precip,1)+" mm",82,158,2,TH.text,TH.panel,MC_DATUM);
    txt("UV INDEX",238,69,1,TH.accent2,TH.panel,MC_DATUM);
    txt(String(wx.uv,1),238,108,5,TH.text,TH.panel,MC_DATUM);
    txt(wx.uv>=8?"VERY HIGH":wx.uv>=6?"HIGH":wx.uv>=3?"MODERATE":"LOW",238,137,1,TH.dim,TH.panel,MC_DATUM);
    tft.fillRoundRect(188,155,100,8,4,TH.bg); tft.fillRoundRect(188,155,min(100,(int)lroundf(wx.uv*100.0f/11.0f)),8,4,TH.accent2);
  } else if (wxMode == 4) {
    wxCard(8,52,304,145);
    txt("SUN & DAYLIGHT",160,69,1,TH.accent,TH.panel,MC_DATUM);
    drawSun(74,120,18,COL_SUN);
    txt("SUNRISE",74,153,1,TH.dim,TH.panel,MC_DATUM); txt(wx.sunrise[0]?wx.sunrise:"--:--",74,176,3,TH.text,TH.panel,MC_DATUM);
    txt("SUNSET",246,153,1,TH.dim,TH.panel,MC_DATUM); txt(wx.sunset[0]?wx.sunset:"--:--",246,176,3,TH.text,TH.panel,MC_DATUM);
    txt("TODAY HIGH / LOW",160,206,1,TH.dim,TH.bg,MC_DATUM);
  } else if (wxMode == 5) {
    wxCard(8,52,304,146);
    txt("WIND & AIR",160,68,1,TH.accent,TH.panel,MC_DATUM);
    txt("SPEED",22,94,1,TH.dim,TH.panel,TL_DATUM); txt(String((int)lroundf(wx.wind))+" km/h",22,112,2,TH.text,TH.panel,TL_DATUM);
    txt("DIRECTION",22,141,1,TH.dim,TH.panel,TL_DATUM); txt(String(wx.windDir)+" deg",22,159,2,TH.text,TH.panel,TL_DATUM);
    txt("CLOUD",170,94,1,TH.dim,TH.panel,TL_DATUM); txt(String((int)lroundf(wx.cloud))+"%",170,112,2,TH.text,TH.panel,TL_DATUM);
    txt("DEW POINT",170,141,1,TH.dim,TH.panel,TL_DATUM); txt(String(tempNum(wx.dew))+(cfg.fahr?" F":" C"),170,159,2,TH.text,TH.panel,TL_DATUM);
  } else if (wxMode == 6) {
    wxCard(8,52,304,146);
    txt("WEATHER DETAILS",160,68,1,TH.accent,TH.panel,MC_DATUM);
    const String unit=cfg.fahr?" F":" C";
    txt("CONDITION",20,88,1,TH.dim,TH.panel,TL_DATUM); txt(wxShortCondition(wx.code),300,88,1,TH.text,TH.panel,MR_DATUM);
    txt("TEMP",20,108,1,TH.dim,TH.panel,TL_DATUM); txt(String(tempNum(wx.t))+unit,300,108,1,TH.text,TH.panel,MR_DATUM);
    txt("FEELS",20,128,1,TH.dim,TH.panel,TL_DATUM); txt(String(tempNum(wx.feels))+unit,300,128,1,TH.text,TH.panel,MR_DATUM);
    txt("HUMIDITY",20,148,1,TH.dim,TH.panel,TL_DATUM); txt(String(wx.hum)+"%",300,148,1,TH.text,TH.panel,MR_DATUM);
    txt("WIND",20,168,1,TH.dim,TH.panel,TL_DATUM); txt(String((int)lroundf(wx.wind))+" km/h",300,168,1,TH.text,TH.panel,MR_DATUM);
    txt("PRESSURE",20,188,1,TH.dim,TH.panel,TL_DATUM); txt(String((int)lroundf(wx.pressure))+" hPa",300,188,1,TH.text,TH.panel,MR_DATUM);
  } else {
    wxCard(8,52,304,146);
    txt("EXTRA WEATHER",160,68,1,TH.accent,TH.panel,MC_DATUM);
    txt("DEW POINT",20,94,1,TH.dim,TH.panel,TL_DATUM); txt(String(tempNum(wx.dew))+(cfg.fahr?" F":" C"),300,94,1,TH.text,TH.panel,MR_DATUM);
    txt("CLOUD COVER",20,118,1,TH.dim,TH.panel,TL_DATUM); txt(String((int)lroundf(wx.cloud))+"%",300,118,1,TH.text,TH.panel,MR_DATUM);
    txt("WIND DIRECTION",20,142,1,TH.dim,TH.panel,TL_DATUM); txt(String(wx.windDir)+" deg",300,142,1,TH.text,TH.panel,MR_DATUM);
    txt("RAIN CHANCE",20,166,1,TH.dim,TH.panel,TL_DATUM); txt(String(wx.rainProb[0])+"%",300,166,1,TH.text,TH.panel,MR_DATUM);
    txt("VISIBILITY",20,190,1,TH.dim,TH.panel,TL_DATUM); txt(String((int)lroundf(wx.visibility/1000.0f))+" km",300,190,1,TH.text,TH.panel,MR_DATUM);
  }
  txt("TAP ANYWHERE • NEXT VIEW",160,225,1,TH.dim,TH.bg,MC_DATUM);
}
void wxRefresh() {
  txtp("UPDATING...", SW - 74, BAR / 2 + 1, 1, TH.accent, TH.panel, MR_DATUM, 1, 70);
  wxTried=true; wxLastTry=millis(); fetchWeather(); wxDraw();
}
void weatherInit() { tft.fillScreen(TH.bg); wxDraw(); if(WiFi.status()==WL_CONNECTED && (!wx.ok || millis()-wx.updated>600000UL)) wxRefresh(); }
void weatherLoop() {
  if(!tc.press) return;
  if(pressIn(SW-70,0,66,BAR+2)){beep(1500,20);wxRefresh();return;}
  if(tc.y>BAR+2){wxMode=(wxMode+1)%8;beep(1300,18);wxDraw();}
}

// ============================ SETTINGS ======================================
const int SET_Y0 = 28, SET_STEP = 29, SET_H = 26;
bool sliderDrag = false;

void setRowBase(int i, const char* label) {
  int y = SET_Y0 + i * SET_STEP;
  tft.fillRoundRect(6, y, 308, SET_H, 8, TH.panel);
  txt(label, 16, y + SET_H / 2 + 1, 2, TH.text, TH.panel, ML_DATUM);
}
void toggleSwitch(int x, int y, bool on) {
  tft.fillRoundRect(x, y, 54, 22, 11, on ? TH.good : TH.dim);
  tft.fillCircle(on ? x + 42 : x + 11, y + 11, 8, C565(255,255,255));
  txt(on ? "ON" : "OFF", x - 10, y + 11, 1, on ? TH.good : TH.dim, TH.panel, MR_DATUM);
}
void setRowTheme() {
  setRowBase(0, "Theme");
  int y = SET_Y0;
  button(168, y + 1, 30, 24, "<", TH.bg, TH.accent, 2);
  button(278, y + 1, 28, 24, ">", TH.bg, TH.accent, 2);
  txt(TH.name, 238, y + SET_H / 2 + 1, 2, TH.text, TH.panel, MC_DATUM);
}
void setRowBright() {
  setRowBase(1, "Brightness");
  int y = SET_Y0 + SET_STEP;
  int kx = 140 + (int)(cfg.bright - 10) * 150 / 245;
  tft.fillRoundRect(140, y + 9, 150, 7, 4, TH.bg);
  tft.fillRoundRect(140, y + 9, kx - 140 + 4, 7, 4, TH.accent);
  tft.fillCircle(kx, y + 12, 9, TH.accent);
  tft.fillCircle(kx, y + 12, 3, TH.onacc);
}
void setRowH24() { setRowBase(2, "24-hour clock"); toggleSwitch(242, SET_Y0 + 2 * SET_STEP + 2, cfg.h24); }
void setRowSound() { setRowBase(3, "Sound"); toggleSwitch(242, SET_Y0 + 3 * SET_STEP + 2, cfg.sound); }
void setRowTemp() {
  setRowBase(4, "Temperature");
  int y = SET_Y0 + 4 * SET_STEP + 1;
  button(226, y, 36, 23, "C", cfg.fahr ? TH.bg : TH.accent, cfg.fahr ? TH.text : TH.onacc, 2);
  button(266, y, 36, 23, "F", cfg.fahr ? TH.accent : TH.bg, cfg.fahr ? TH.onacc : TH.text, 2);
}
void setRowClockStyle() {
  setRowBase(5, "Clock style");
  int y = SET_Y0 + 5 * SET_STEP;
  button(166, y + 1, 30, 24, "<", TH.bg, TH.accent, 2);
  button(278, y + 1, 28, 24, ">", TH.bg, TH.accent, 2);
  txt(CLOCK_STYLE_NAMES[cfg.clockStyle], 238, y + SET_H / 2 + 1, 2, TH.text, TH.panel, MC_DATUM);
}
void drawSetupButton() {
  button(55, 204, 210, 28, "Wi-Fi Setup / Scanner", TH.accent, TH.onacc, 2);
}
void settingsInit() {
  tft.fillScreen(TH.bg);
  drawBar("Settings", nullptr, false);
  setRowTheme(); setRowBright(); setRowH24(); setRowSound(); setRowTemp(); setRowClockStyle();
  drawSetupButton();
}
void settingsLoop() {
  if (tc.press) {
    if (pressIn(168, SET_Y0 + 1, 34, 28)) {
      cfg.theme = (cfg.theme + NTHEMES - 1) % NTHEMES;
      saveCfg(); beep(1200, 20); settingsInit(); return;
    }
    if (pressIn(276, SET_Y0 + 1, 34, 28)) {
      cfg.theme = (cfg.theme + 1) % NTHEMES;
      saveCfg(); beep(1200, 20); settingsInit(); return;
    }
    if (pressIn(120, SET_Y0 + SET_STEP, 192, SET_H)) sliderDrag = true;
    if (pressIn(236, SET_Y0 + 2 * SET_STEP, 78, SET_H)) {
      cfg.h24 = !cfg.h24; saveCfg(); beep(1400, 20); setRowH24();
    }
    if (pressIn(236, SET_Y0 + 3 * SET_STEP, 78, SET_H)) {
      cfg.sound = !cfg.sound; saveCfg(); beep(1400, 20); setRowSound();
    }
    if (pressIn(220, SET_Y0 + 4 * SET_STEP, 86, SET_H)) {
      cfg.fahr = tc.x >= 264; saveCfg(); beep(1400, 20); setRowTemp();
    }
    if (pressIn(164, SET_Y0 + 5 * SET_STEP, 148, SET_H)) {
      if (tc.x < 205) cfg.clockStyle = (cfg.clockStyle + 19) % 20;
      else cfg.clockStyle = (cfg.clockStyle + 1) % 20;
      saveCfg(); beep(1200, 20); setRowClockStyle();
    }
    if (pressIn(50, 202, 220, 34)) {
      beep(1500, 20); goScreen(SCR_SETUP); return;
    }
  }
  if (sliderDrag) {
    if (tc.down) {
      int v = constrain((int)map(tc.x, 140, 290, 10, 255), 10, 255);
      if (v != cfg.bright) { cfg.bright = v; setBacklight(v); setRowBright(); }
    }
    if (tc.release) { sliderDrag = false; saveCfg(); }
  }
}

// ============================ SETUP / SCANNER ===============================
void setupWifiScan() {
  tft.fillRect(0, BAR, SW, SH - BAR, TH.bg);
  txt("Scanning nearby Wi-Fi...", 160, 105, 2, TH.accent, TH.bg, MC_DATUM);
  WiFi.scanNetworks(false, true);
  WiFi.scanDelete();
}
void setupInit() {
  tft.fillScreen(TH.bg);
  drawBar("PHONE WIFI SETUP", "NEXT", true);
  drawQR(CYD_AP_QR, (SW - ((33 + 4) * 3)) / 2, 28, 3);
  txt("SCAN TO CONNECT PHONE", 160, 152, 1, TH.text, TH.bg, MC_DATUM);
  txt("AP: " + String(CONTROL_AP_SSID), 160, 169, 1, TH.accent, TH.bg, MC_DATUM);
  txt("Password: " + String(CONTROL_AP_PASS), 160, 185, 1, TH.text, TH.bg, MC_DATUM);
  txt("Then press NEXT", 160, 202, 1, TH.dim, TH.bg, MC_DATUM);
}
void setupLoop() {
  if (!tc.release) return;
  int x = tc.x;
  int y = tc.y;
  if (x <= 220 || y >= 45) return;
  if (setupQrStage == 0) {
    setupQrStage = 1;
    tft.fillScreen(TH.bg);
    drawBar("PHONE WIFI SETUP", "BACK", true);
    drawQR(CYD_IP_QR, (SW - ((25 + 4) * 5)) / 2, 30, 5);
    txt("CONTROL CENTER", 160, 176, 2, TH.text, TH.bg, MC_DATUM);
    txt(WiFi.softAPIP().toString(), 160, 200, 2, TH.accent, TH.bg, MC_DATUM);
    txt("SCAN TO OPEN CONTROL", 160, 222, 1, TH.dim, TH.bg, MC_DATUM);
  } else {
    setupQrStage = 0;
    setupInit();
  }
}

// ============================ ISLAMIC (Bangla) ==============================
struct IslamicItem {
  const char* arabic;
  const char* translation;   // English
  const char* bangla;        // Banglish (romanised Bangla)
  const char* ref;
  uint8_t category;          // 1=Ayat, 2=Hadith, 3=Tip
};

const IslamicItem ISLAMIC_DB[] = {
  // ---- QURAN AYATS ----
  {"بِسْمِ اللَّهِ الرَّحْمَٰنِ الرَّحِيمِ",
   "In the name of Allah, the Entirely Merciful, the Especially Merciful.",
   "Porom korunamoy, maha dayalu Allah er name shuru korchi.",
   "Al-Fatihah 1:1", 1},
  {"إِنَّ مَعَ الْعُسْرِ يُسْرًا",
   "Indeed, with hardship [will be] ease.",
   "Nishchoi koshter sathe sohojota ache.",
   "Ash-Sharh 94:6", 1},
  {"وَمَن يَتَوَكَّلْ عَلَى اللَّهِ فَهُوَ حَسْبُهُ",
   "And whoever relies upon Allah - then He is sufficient for him.",
   "Je byakti Allah er upor bhorosa kore, tar jonno Allahi jotheshto.",
   "At-Talaq 65:3", 1},
  {"إِنَّ اللَّهَ مَعَ الصَّابِرِينَ",
   "Indeed, Allah is with the patient.",
   "Nishchoi Allah dhairjosholider sathe achen.",
   "Al-Baqarah 2:153", 1},
  {"وَاذْكُر رَّبَّكَ إِذَا نَسِيتَ",
   "And remember your Lord when you forget.",
   "Jokhon bhule jao, tokhon tomader Rob er smoron koro.",
   "Al-Kahf 18:24", 1},
  {"رَبِّ زِدْنِي عِلْمًا",
   "My Lord, increase me in knowledge.",
   "He amar Rob! Amar gyan bariye dao.",
   "Ta-Ha 20:114", 1},
  {"وَقُل رَّبِّ ارْحَمْهُمَا كَمَا رَبَّيَانِي صَغِيرًا",
   "And say: My Lord, have mercy upon them as they brought me up when I was small.",
   "Bolo: He amar Rob! Tader prati doya koro, jemon tara choto belay amake protipalon koreche.",
   "Al-Isra 17:24", 1},
  {"فَاذْكُرُونِي أَذْكُرْكُمْ وَاشْكُرُوا لِي وَلَا تَكْفُرُونِ",
   "So remember Me; I will remember you. And be grateful to Me and do not deny Me.",
   "Tomra amake smoron koro, ami tomader smoron korbo. Amar krito gyata hao, kufuri koro na.",
   "Al-Baqarah 2:152", 1},
  {"وَعَسَىٰ أَن تَكْرَهُوا شَيْئًا وَهُوَ خَيْرٌ لَّكُمْ",
   "Perhaps you hate a thing and it is good for you.",
   "Hote pare tumi kono jinis opochondo koro, athocho ta tomader jonno mongol.",
   "Al-Baqarah 2:216", 1},
  {"إِنَّ اللَّهَ لَا يُغَيِّرُ مَا بِقَوْمٍ حَتَّىٰ يُغَيِّرُوا مَا بِأَنفُسِهِمْ",
   "Indeed, Allah will not change the condition of a people until they change what is in themselves.",
   "Nishchoi Allah kono jati'r obostha poriborton koren na, jotokhon na tara nijedero poriborton kore.",
   "Ar-Ra'd 13:11", 1},
  {"وَاللَّهُ يُحِبُّ الْمُحْسِنِينَ",
   "And Allah loves the doers of good.",
   "Allah mohsin (uttom kormosholi) der bhalobasen.",
   "Al-Imran 3:134", 1},
  {"إِنَّ الصَّلَاةَ كَانَتْ عَلَى الْمُؤْمِنِينَ كِتَابًا مَّوْقُوتًا",
   "Indeed, prayer has been decreed upon the believers at specified times.",
   "Nishchoi namaj mumin der upor nirdisto somoye farj kora hoyeche.",
   "An-Nisa 4:103", 1},
  {"وَبَشِّرِ الصَّابِرِينَ",
   "And give good tidings to the patient.",
   "Aro shubhokhobor dao dhairjosholider.",
   "Al-Baqarah 2:155", 1},
  {"وَلَا تَقْرَبُوا الْفَوَاحِشَ مَا ظَهَرَ مِنْهَا وَمَا بَطَنَ",
   "And do not approach immoralities - what is apparent of them and what is concealed.",
   "Aslila (kharap kaj) er kache jao na - ja prokashyo ar ja gopon.",
   "Al-An'am 6:151", 1},
  {"يَا أَيُّهَا الَّذِينَ آمَنُوا اتَّقُوا اللَّهَ وَقُولُوا قَوْلًا سَدِيدًا",
   "O you who have believed, fear Allah and speak words of appropriate justice.",
   "He imandaro! Allah ke bhoyo koro ar sojha-sothik kotha bolo.",
   "Al-Ahzab 33:70", 1},
  {"وَقُلْ جَاءَ الْحَقُّ وَزَهَقَ الْبَاطِلُ ۚ إِنَّ الْبَاطِلَ كَانَ زَهُوقًا",
   "And say: Truth has come, and falsehood has departed. Indeed is falsehood ever bound to depart.",
   "Bolo: Sotyo eshe geche, mithya dur hoye geche. Nishchoi mithya dur hobar-i.",
   "Al-Isra 17:81", 1},
  {"فَإِنَّ مَعَ الْعُسْرِ يُسْرًا",
   "For indeed, with hardship [will be] ease.",
   "Nishchoi koshter sathe sohojota ache.",
   "Ash-Sharh 94:5", 1},
  {"وَاللَّهُ غَفُورٌ رَّحِيمٌ",
   "And Allah is Forgiving and Merciful.",
   "Allah khomashil, doyamoy.",
   "Al-Baqarah 2:218", 1},
  {"إِنَّ اللَّهَ يَأْمُرُ بِالْعَدْلِ وَالْإِحْسَانِ",
   "Indeed, Allah orders justice and good conduct.",
   "Nishchoi Allah bichar ar uttom achoron er adesh den.",
   "An-Nahl 16:90", 1},

  // ---- HADITH ----
  {"إِنَّمَا الأَعْمَالُ بِالنِّيَّاتِ",
   "Actions are judged by intentions.",
   "Niyot onujayi amol porigonyito hoy.",
   "Bukhari & Muslim", 2},
  {"الْمُسْلِمُ مَنْ سَلِمَ الْمُسْلِمُونَ مِنْ لِسَانِهِ وَيَدِهِ",
   "A Muslim is one from whose tongue and hand other Muslims are safe.",
   "Musolman shei byakti, jahar jibha ar hat theke onno musolman nirapod thake.",
   "Bukhari", 2},
  {"لَا يُؤْمِنُ أَحَدُكُمْ حَتَّى يُحِبَّ لِأَخِيهِ مَا يُحِبُّ لِنَفْسِهِ",
   "None of you truly believes until he loves for his brother what he loves for himself.",
   "Tomader keu prokrito mumin hobe na, jotokhon na se nijer jonno ja bhalobase ta nijer bhaiyer jonno bhalobase.",
   "Bukhari & Muslim", 2},
  {"مَنْ كَانَ يُؤْمِنُ بِاللَّهِ وَالْيَوْمِ الآخِرِ فَلْيَقُلْ خَيْرًا أَوْ لِيَصْمُتْ",
   "Whoever believes in Allah and the Last Day, let him speak good or remain silent.",
   "Je byakti Allah ar sesh dine iman rakhe, se jeno bhalo kotha bole othoba chup thake.",
   "Bukhari", 2},
  {"الدُّعَاءُ مُخُّ الْعِبَادَةِ",
   "Supplication is the essence of worship.",
   "Dua holo ibadat er morgo.",
   "Tirmidhi", 2},
  {"خَيْرُكُمْ مَنْ تَعَلَّمَ الْقُرْآنَ وَعَلَّمَهُ",
   "The best among you are those who learn the Quran and teach it.",
   "Tomader modhye shei byakti shrestho, je Quran shikhe ar onnoder shikhay.",
   "Bukhari", 2},
  {"اتَّقِ اللَّهَ حَيْثُمَا كُنْتَ",
   "Fear Allah wherever you are.",
   "Jekhanei thako, Allah ke bhoyo koro.",
   "Tirmidhi", 2},
  {"الطُّهُورُ شَطْرُ الإِيمَانِ",
   "Cleanliness is half of faith.",
   "Pobitrotai iman er ordhek.",
   "Muslim", 2},
  {"الْجَنَّةُ تَحْتَ أَقْدَامِ الأُمَّهَاتِ",
   "Paradise lies beneath the feet of mothers.",
   "Jannat ma er pa er niche.",
   "Nasa'i", 2},
  {"مَنْ لَا يَرْحَمُ لَا يُرْحَمُ",
   "Whoever shows no mercy will be shown no mercy.",
   "Je byakti doya kore na, takeo doya kora hoy na.",
   "Bukhari", 2},
  {"أَفْضَلُ الذِّكْرِ لَا إِلَهَ إِلَّا اللَّهُ",
   "The best remembrance is: There is no god but Allah.",
   "Shrestho zikir holo: La ilaha illallah.",
   "Tirmidhi", 2},
  {"الْمُؤْمِنُ لِلْمُؤْمِنِ كَالْبُنْيَانِ يَشُدُّ بَعْضُهُ بَعْضًا",
   "A believer to another believer is like a building whose parts support one another.",
   "Mumin onno mumin er jonno emon, jemon ekoti imarot - ek ongsho onno ongshoke shokti joge.",
   "Bukhari & Muslim", 2},
  {"مَنْ صَلَّى عَلَيَّ وَاحِدَةً صَلَّى اللَّهُ عَلَيْهِ عَشْرًا",
   "Whoever sends blessings upon me once, Allah will send blessings upon him ten times.",
   "Je byakti amar upor ekbar durood pore, Allah tar upor doshbar durood poren.",
   "Muslim", 2},

  // ---- PORAMORSO / TIPS ----
  {"", "Begin every task with Bismillah and end with Alhamdulillah.",
   "Prottek kaj Bismillah diye shuru koro ar Alhamdulillah diye shesh koro.", "", 3},
  {"", "Say Salam before speaking - it spreads love and peace.",
   "Kotha bolar age Salam dao - eta bhalobasha ar shanti choriye dey.", "", 3},
  {"", "Smiling at your brother is charity (Sadaqah).",
   "Tomar bhaiyer dike hese taka dekhano o sadakah.", "", 3},
  {"", "Read at least one page of the Quran every day.",
   "Prottek din kompokkhe ek pata Quran poro.", "", 3},
  {"", "Pray on time - the first question on the Day of Judgement is Salah.",
   "Somoye namaj poro - kiyamot er dine prothom proshno hobe namaj somporke.", "", 3},
  {"", "Forgive others so that Allah may forgive you.",
   "Onnoder khoma koro, jate Allah tomake khoma koren.", "", 3},
  {"", "Control your tongue - silence is better than useless talk.",
   "Jibha songjoto rakho - nirbobota opokarok kothar cheye bhalo.", "", 3},
  {"", "Be kind to your parents - Jannah lies beneath their feet.",
   "Pita-matar sathe sadaoy bhalo byabohar koro - Jannat tader pa er niche.", "", 3},
  {"", "Give charity daily - even a date or a glass of water.",
   "Prottek din sadakah dao - ekoti khejur othoba ek glass pani o hole.", "", 3},
  {"", "Seek knowledge from cradle to grave.",
   "Dol theke kobor porjonto gyan onusondhan koro.", "", 3},
  {"", "Say Alhamdulillah in every situation.",
   "Prottek obosthay Alhamdulillah bolo.", "", 3},
  {"", "Sleep in Wudu - angels make dua for you all night.",
   "Oju soho ghumiye poro - sararaat ferestara tomader jonno dua kore.", "", 3},
  {"", "Lower your gaze - it purifies the heart.",
   "Dristi nichu rakho - eta hridoy ke pobitro kore.", "", 3},
  {"", "Keep good company - a friend influences your deen.",
   "Bhalo songo rakho - bondhu tomar deen ke probhabito kore.", "", 3},
  {"", "Do not waste food - it is a blessing from Allah.",
   "Khabar nosto koro na - eta Allah er neamat.", "", 3},
  {"", "Say Bismillah before eating and Alhamdulillah after.",
   "Khawar age Bismillah ar pore Alhamdulillah bolo.", "", 3},
  {"", "Visit the sick - it brings Barakah in your life.",
   "Oshustho byaktike dekhote jao - eta tomader jibone borokot aniye dey.", "", 3},
  {"", "Repent daily - Allah loves those who turn to Him.",
   "Prottek din toba koro - Allah tader bhalobasen, jara tar dike phire ashe.", "", 3},
  {"", "Be patient in trials - they erase your sins.",
   "Bipode dhairjo dhoron koro - eta tomader gonah moche dey.", "", 3},
  {"", "Remember death often - it keeps you on the right path.",
   "Mrittur kotha proshongsha koro - eta tomake sojha pothe rakhe.", "", 3},
};
const int ISLAMIC_COUNT = sizeof(ISLAMIC_DB) / sizeof(ISLAMIC_DB[0]);

int islamicIdx = 0;
int islamicLastDay = -1;
uint8_t islamicCat = 0;
uint8_t islamicLang = 0;   // 0 = English, 1 = Bangla (Banglish)

const char* ISLAMIC_CATS[4] = {"All", "Ayat", "Hadith", "Tip"};
const uint16_t ISLAMIC_CAT_COLS[4] = {C565(120,180,255), C565(0,200,255), C565(70,225,140), C565(255,90,200)};

// >>> FIX: takes int index (not const IslamicItem&) so the Arduino
//     auto-generated prototype never references IslamicItem.
const char* islamicText(int idx) {
  const IslamicItem &it = ISLAMIC_DB[idx];
  if (islamicLang == 1 && it.bangla[0]) return it.bangla;
  return it.translation;
}

int islamicNext(int cur, int dir, uint8_t cat) {
  for (int i = 1; i <= ISLAMIC_COUNT; i++) {
    int n = (cur + dir * i + ISLAMIC_COUNT * 2) % ISLAMIC_COUNT;
    if (cat == 0 || ISLAMIC_DB[n].category == cat) return n;
  }
  return cur;
}

void islamicAutoPick() {
  time_t n = time(nullptr);
  struct tm tmv;
  localtime_r(&n, &tmv);
  int doy = tmv.tm_yday;
  if (doy != islamicLastDay) {
    islamicLastDay = doy;
    islamicIdx = (int)((doy * 7 + tmv.tm_year) % ISLAMIC_COUNT);
    if (islamicIdx < 0) islamicIdx = 0;
  }
}

void islamicDraw() {
  islamicAutoPick();
  tft.fillScreen(TH.bg);

  tft.fillRect(0, 0, SW, BAR, TH.panel);
  tft.fillRoundRect(4, 3, 42, 20, 6, TH.accent2);
  tft.fillTriangle(16, 13, 25, 7, 25, 19, TH.onacc);
  tft.fillRect(25, 11, 10, 5, TH.onacc);
  txtp("Daily Islamic", 54, BAR / 2 + 1, 2, TH.text, TH.panel, ML_DATUM, 1, 0);

  uint16_t lc = islamicLang ? TH.accent2 : TH.accent;
  tft.fillRoundRect(SW - 42, 4, 36, 18, 5, lc);
  txt(islamicLang ? "BN" : "EN", SW - 24, 13, 1, TH.onacc, lc, MC_DATUM);

  int tabY = BAR + 3, tabH = 22, tabW = 74;
  for (int i = 0; i < 4; i++) {
    int tx = 6 + i * (tabW + 3);
    bool active = (islamicCat == i);
    uint16_t fill = active ? ISLAMIC_CAT_COLS[i] : TH.panel;
    uint16_t fg   = active ? TH.onacc : TH.dim;
    tft.fillRoundRect(tx, tabY, tabW, tabH, 7, fill);
    txt(ISLAMIC_CATS[i], tx + tabW / 2, tabY + tabH / 2 + 1, 2, fg, fill, MC_DATUM);
  }

  const IslamicItem &it = ISLAMIC_DB[islamicIdx];
  int cardY = tabY + tabH + 4;
  int cardH = SH - cardY - 34;
  tft.fillRoundRect(8, cardY, SW - 16, cardH, 12, TH.panel);

  uint16_t kc = ISLAMIC_CAT_COLS[it.category];
  const char* kname = ISLAMIC_CATS[it.category];
  tft.fillRoundRect(14, cardY + 4, 56, 16, 5, kc);
  txt(kname, 14 + 28, cardY + 4 + 8, 1, TH.onacc, kc, MC_DATUM);

  int ty;
  if (it.arabic[0]) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TH.accent, TH.panel);
    tft.setTextSize(2);
    tft.drawString(it.arabic, SW / 2, cardY + 36, 4);
    tft.setTextSize(1);
    tft.drawFastHLine(30, cardY + 60, SW - 60, TH.dim);
    ty = cardY + 68;
  } else {
    ty = cardY + 26;
  }

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TH.text, TH.panel);
  tft.setTextSize(1);
  String text = String(islamicText(islamicIdx));   // <<< FIX: index, not struct
  int lineStart = 0;
  int maxW = SW - 40;
  while (lineStart < (int)text.length()) {
    int lineEnd = lineStart;
    int lastSpace = -1;
    while (lineEnd < (int)text.length()) {
      if (text[lineEnd] == ' ') lastSpace = lineEnd;
      String probe = text.substring(lineStart, lineEnd + 1);
      if (tft.textWidth(probe, 2) > maxW) break;
      lineEnd++;
    }
    if (lineEnd >= (int)text.length()) lineEnd = text.length();
    else if (lastSpace > lineStart) lineEnd = lastSpace;
    tft.drawString(text.substring(lineStart, lineEnd), 20, ty, 2);
    ty += 18;
    lineStart = lineEnd;
    while (lineStart < (int)text.length() && text[lineStart] == ' ') lineStart++;
    if (ty > cardY + cardH - 20) break;
  }

  if (it.ref[0]) {
    tft.setTextDatum(BR_DATUM);
    tft.setTextColor(TH.accent, TH.panel);
    tft.setTextSize(1);
    tft.drawString(String("- ") + it.ref, SW - 20, cardY + cardH - 8, 2);
  }

  button(8,   SH - 30, 76, 26, "Prev",   TH.panel,  TH.text,  2);
  button(88,  SH - 30, 76, 26, "Next",   TH.accent, TH.onacc, 2);
  button(168, SH - 30, 76, 26, "Random", TH.accent2,TH.onacc, 2);
  button(248, SH - 30, 64, 26, islamicLang ? "BN" : "EN", TH.good, TH.onacc, 2);
}

void islamicInit() { islamicDraw(); }

void islamicLoop() {
  if (!tc.press) return;

  int tabY = BAR + 3, tabH = 22, tabW = 74;
  for (int i = 0; i < 4; i++) {
    int tx = 6 + i * (tabW + 3);
    if (pressIn(tx, tabY, tabW, tabH)) {
      islamicCat = i;
      beep(1400, 20);
      if (islamicCat != 0 && ISLAMIC_DB[islamicIdx].category != islamicCat) {
        for (int j = 0; j < ISLAMIC_COUNT; j++) {
          if (ISLAMIC_DB[j].category == islamicCat) { islamicIdx = j; break; }
        }
      }
      islamicDraw();
      return;
    }
  }

  if (pressIn(8, SH - 30, 76, 26)) {
    islamicIdx = islamicNext(islamicIdx, -1, islamicCat);
    beep(1200, 20); islamicDraw();
  } else if (pressIn(88, SH - 30, 76, 26)) {
    islamicIdx = islamicNext(islamicIdx, 1, islamicCat);
    beep(1400, 20); islamicDraw();
  } else if (pressIn(168, SH - 30, 76, 26)) {
    int tries = 0, n = islamicIdx;
    do { n = random(ISLAMIC_COUNT); tries++; }
    while (islamicCat != 0 && ISLAMIC_DB[n].category != islamicCat && tries < 50);
    islamicIdx = n;
    beep(1700, 20); islamicDraw();
  } else if (pressIn(248, SH - 30, 64, 26)) {
    islamicLang = !islamicLang;
    beep(1500, 20); islamicDraw();
  }
}

// ============================ KNOWLEDGE QUIZ ================================
// A simple offline Yes/No quiz: General Knowledge, Islamic facts and Bangladesh facts.
struct KnowledgeQ { const char* cat; const char* q; bool yes; };
const KnowledgeQ KQ[] = {
  {"GENERAL", "The Earth goes around the Sun.", true},
  {"GENERAL", "Water freezes at 0 C at sea level.", true},
  {"GENERAL", "The Pacific Ocean is larger than the Atlantic.", true},
  {"GENERAL", "The Moon is a star.", false},
  {"ISLAMIC", "Muslims pray five obligatory prayers each day.", true},
  {"ISLAMIC", "Ramadan is the ninth month of the Islamic calendar.", true},
  {"ISLAMIC", "The Quran is the holy book of Islam.", true},
  {"ISLAMIC", "Fajr is one of the five daily obligatory prayers.", true},
  {"BANGLADESH", "Dhaka is the capital of Bangladesh.", true},
  {"BANGLADESH", "The national currency of Bangladesh is the taka.", true},
  {"BANGLADESH", "The Sundarbans is shared by Bangladesh and India.", true},
  {"BANGLADESH", "Cox's Bazar is known for its long natural sea beach.", true},
  {"BANGLADESH", "Bangladesh is located in South Asia.", true},
  {"GENERAL", "The Sun is a planet.", false},
  {"ISLAMIC", "Zakat is one of the Five Pillars of Islam.", true},
  {"BANGLADESH", "The national language of Bangladesh is Bangla.", true}
};
const int KQ_COUNT = sizeof(KQ) / sizeof(KQ[0]);
int kqIndex = 0, kqScore = 0, kqAnswered = 0;
bool kqAnsweredThis = false, kqLastCorrect = false;
uint32_t kqMsgUntil = 0;

void knowledgeDraw() {
  tft.fillScreen(TH.bg);
  drawBar("Knowledge", "NEXT", true);
  const KnowledgeQ &q = KQ[kqIndex];
  uint16_t catCol = !strcmp(q.cat, "ISLAMIC") ? TH.good :
                    (!strcmp(q.cat, "BANGLADESH") ? TH.accent2 : TH.accent);

  // Header cards
  tft.fillRoundRect(10, 38, 300, 30, 10, TH.panel);
  txt(q.cat, 24, 53, 2, catCol, TH.panel, ML_DATUM);
  txt(String("Q ") + String(kqIndex + 1) + "/" + String(KQ_COUNT),
      286, 53, 1, TH.dim, TH.panel, MR_DATUM);
  txt(String("Score ") + String(kqScore), 160, 77, 1, TH.dim, TH.bg, MC_DATUM);

  // Question card
  tft.fillRoundRect(10, 86, 300, 82, 12, TH.panel);
  tft.drawRoundRect(10, 86, 300, 82, 12, catCol);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TH.text, TH.panel);
  tft.setTextSize(1);
  String text = q.q;
  int y = 98;
  int pos = 0;
  while (pos < (int)text.length() && y < 155) {
    int cut = pos, last = -1;
    while (cut < (int)text.length()) {
      if (text[cut] == ' ') last = cut;
      String part = text.substring(pos, cut + 1);
      if (tft.textWidth(part, 1) > 278) break;
      cut++;
    }
    if (cut >= (int)text.length()) cut = text.length();
    else if (last > pos) cut = last;
    String line = text.substring(pos, cut);
    tft.drawString(line, 18, y, 1);
    y += 13;
    pos = cut;
    while (pos < (int)text.length() && text[pos] == ' ') pos++;
  }

  // Answer state
  if (!kqAnsweredThis) {
    button(12, 176, 140, 34, "YES", TH.good, TH.onacc, 2);
    button(168, 176, 140, 34, "NO", TH.bad, C565(255,255,255), 2);
    txt("Tap YES or NO", 160, 218, 1, TH.dim, TH.bg, MC_DATUM);
  } else {
    uint16_t resultCol = kqLastCorrect ? TH.good : TH.bad;
    const char* result = kqLastCorrect ? "RIGHT!" : "WRONG!";
    tft.fillRoundRect(12, 176, 296, 34, 10, resultCol);
    tft.drawRoundRect(12, 176, 296, 34, 10, resultCol);
    txt(result, 160, 193, 2, TH.onacc, resultCol, MC_DATUM);
    txt(kqLastCorrect ? "Correct answer" : "Try the next one",
        160, 218, 1, resultCol, TH.bg, MC_DATUM);
  }
}
void knowledgeInit() { knowledgeDraw(); }
void knowledgeAnswer(bool answer) {
  if (kqAnsweredThis) return;
  kqLastCorrect = (answer == KQ[kqIndex].yes);
  if (kqLastCorrect) kqScore++;
  kqAnswered++; kqAnsweredThis = true; kqMsgUntil = millis() + 1200;
  beep(kqLastCorrect ? 1800 : 700, 35);
  knowledgeDraw();
}
void knowledgeNext() {
  kqIndex = (kqIndex + 1) % KQ_COUNT;
  kqAnsweredThis = false; kqLastCorrect = false;
  knowledgeDraw();
}
void knowledgeLoop() {
  if (!tc.press) return;
  if (pressIn(SW - 70, 0, 66, BAR + 2)) { beep(1500, 20); knowledgeNext(); return; }
  if (pressIn(12, 170, 140, 42)) { knowledgeAnswer(true); return; }
  if (pressIn(168, 170, 140, 42)) { knowledgeAnswer(false); return; }
  if (kqAnsweredThis && (int32_t)(millis() - kqMsgUntil) > 0) knowledgeNext();
}

// ============================ GAME COMMON ===================================
const char* GAME_TITLE[10] = {"X & O", "Snake", "Memory", "Whack", "Reflex", "2048", "Bricks", "Simon", "Tetris", "Pong"};
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

// ---- 0: TIC-TAC-TOE ----
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
    if (p == 1 && random(100) < 15) continue;
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

// ---- 1: SNAKE ----
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

// ---- 2: MEMORY ----
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

// ---- 3: WHACK-A-MOLE ----
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
    int h;
    do { h = random(9); } while (h == wkLastHole);
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

// ---- 4: REFLEX ----
int rxState;
uint32_t rxAt, rxT0;
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

// ---- 5: 2048 ----
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

// ---- 6: BREAKOUT ----
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
  if (brDrawX > -40) tft.fillCircle(brDrawX, brDrawY, 4, TH.bg);
  if (brState == 0) { brX = brPX; brY = BR_PY - 5; }
  else {
    brX += brVX; brY += brVY;
    if (brX < 4) { brX = 4; brVX = -brVX; }
    if (brX > SW - 5) { brX = SW - 5; brVX = -brVX; }
    if (brY < BAR + 4) { brY = BAR + 4; brVY = -brVY; }
    if (brVY > 0 && brY + 4 >= BR_PY && brY + 4 <= BR_PY + 10 && brX >= brPX - BR_PW / 2 - 4 && brX <= brPX + BR_PW / 2 + 4) {
      float rel = constrain((brX - brPX) / (BR_PW / 2.0f), -1.0f, 1.0f);
      float sp = sqrtf(brVX * brVX + brVY * brVY);
      float ang = rel * 1.05f;
      brVX = sp * sinf(ang); brVY = -sp * cosf(ang);
      brY = BR_PY - 5; beep(500, 20);
    }
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
  if ((int)brPX != brOldPX) {
    tft.fillRect(brOldPX - BR_PW / 2 - 1, BR_PY, BR_PW + 2, 8, TH.bg);
    brOldPX = (int)brPX;
  }
  brPaddle();
  brDrawX = (int)brX; brDrawY = (int)brY;
  tft.fillCircle(brDrawX, brDrawY, 4, TH.text);
}

// ---- 7: SIMON ----
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

// ---- 8: TETRIS ----
#define TET_COLS 10
#define TET_ROWS 16
#define TET_SIZE 13
#define TET_X0   ((SW - TET_COLS * TET_SIZE) / 2)
#define TET_Y0   (BAR + 4)

const uint8_t TET_SHAPES[7][4][4] = {
  {{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}},
  {{0,0,0,0},{0,1,1,0},{0,1,1,0},{0,0,0,0}},
  {{0,0,0,0},{0,1,0,0},{1,1,1,0},{0,0,0,0}},
  {{0,0,0,0},{0,1,1,0},{1,1,0,0},{0,0,0,0}},
  {{0,0,0,0},{1,1,0,0},{0,1,1,0},{0,0,0,0}},
  {{0,0,0,0},{1,0,0,0},{1,1,1,0},{0,0,0,0}},
  {{0,0,0,0},{0,0,1,0},{1,1,1,0},{0,0,0,0}}
};
const uint16_t TET_COL[7] = {
  C565(0,220,220), C565(240,220,60), C565(180,80,220),
  C565(60,210,90), C565(240,70,70), C565(70,110,240), C565(240,160,50)
};

int tetGrid[TET_ROWS][TET_COLS];
int tetPiece[4][4];
int tetPx, tetPy, tetPc;
int tetScore, tetLines, tetLevel;
int tetState;
uint32_t tetFall;

void tetDrawCell(int x, int y, uint16_t c, uint16_t bg) {
  tft.fillRect(TET_X0 + x * TET_SIZE, TET_Y0 + y * TET_SIZE,
               TET_SIZE - 1, TET_SIZE - 1, c);
  tft.drawRect(TET_X0 + x * TET_SIZE, TET_Y0 + y * TET_SIZE,
               TET_SIZE - 1, TET_SIZE - 1, bg);
}
void tetDrawBoard() {
  tft.fillRoundRect(TET_X0 - 2, TET_Y0 - 2,
                    TET_COLS * TET_SIZE + 4, TET_ROWS * TET_SIZE + 4,
                    4, TH.panel);
  for (int y = 0; y < TET_ROWS; y++)
    for (int x = 0; x < TET_COLS; x++) {
      if (tetGrid[y][x]) tetDrawCell(x, y, TET_COL[tetGrid[y][x] - 1], TH.bg);
      else               tetDrawCell(x, y, TH.bg, TH.bg);
    }
}
void tetDrawPiece() {
  for (int y = 0; y < 4; y++)
    for (int x = 0; x < 4; x++)
      if (tetPiece[y][x]) {
        int gx = tetPx + x, gy = tetPy + y;
        if (gy >= 0 && gy < TET_ROWS && gx >= 0 && gx < TET_COLS)
          tetDrawCell(gx, gy, TET_COL[tetPc], TH.bg);
      }
}
void tetErasePiece() {
  for (int y = 0; y < 4; y++)
    for (int x = 0; x < 4; x++)
      if (tetPiece[y][x]) {
        int gx = tetPx + x, gy = tetPy + y;
        if (gy >= 0 && gy < TET_ROWS && gx >= 0 && gx < TET_COLS) {
          if (tetGrid[gy][gx]) tetDrawCell(gx, gy, TET_COL[tetGrid[gy][gx]-1], TH.bg);
          else                 tetDrawCell(gx, gy, TH.bg, TH.bg);
        }
      }
}
void tetSpawn() {
  tetPc = random(7);
  for (int y = 0; y < 4; y++)
    for (int x = 0; x < 4; x++)
      tetPiece[y][x] = TET_SHAPES[tetPc][y][x];
  tetPx = TET_COLS / 2 - 2;
  tetPy = -1;
  for (int y = 0; y < 4; y++)
    for (int x = 0; x < 4; x++)
      if (tetPiece[y][x]) {
        int gy = tetPy + y, gx = tetPx + x;
        if (gy >= 0 && gy < TET_ROWS && (gx < 0 || gx >= TET_COLS || tetGrid[gy][gx])) {
          tetState = 1; return;
        }
      }
  tetDrawPiece();
}
bool tetCollideMove(int nx, int ny) {
  for (int y = 0; y < 4; y++)
    for (int x = 0; x < 4; x++) {
      if (!tetPiece[y][x]) continue;
      int gx = nx + x, gy = ny + y;
      if (gx < 0 || gx >= TET_COLS || gy >= TET_ROWS) return true;
      if (gy >= 0 && tetGrid[gy][gx]) return true;
    }
  return false;
}
bool tetCollideRotate() {
  int tmp[4][4];
  for (int y = 0; y < 4; y++)
    for (int x = 0; x < 4; x++)
      tmp[x][3 - y] = tetPiece[y][x];
  for (int y = 0; y < 4; y++)
    for (int x = 0; x < 4; x++) {
      if (!tmp[y][x]) continue;
      int gx = tetPx + x, gy = tetPy + y;
      if (gx < 0 || gx >= TET_COLS || gy >= TET_ROWS) return true;
      if (gy >= 0 && tetGrid[gy][gx]) return true;
    }
  return false;
}
void tetLock() {
  for (int y = 0; y < 4; y++)
    for (int x = 0; x < 4; x++)
      if (tetPiece[y][x]) {
        int gx = tetPx + x, gy = tetPy + y;
        if (gy >= 0 && gy < TET_ROWS && gx >= 0 && gx < TET_COLS)
          tetGrid[gy][gx] = tetPc + 1;
      }
  int cleared = 0;
  for (int y = TET_ROWS - 1; y >= 0; y--) {
    bool full = true;
    for (int x = 0; x < TET_COLS; x++) if (!tetGrid[y][x]) { full = false; break; }
    if (full) {
      cleared++;
      for (int yy = y; yy > 0; yy--)
        for (int x = 0; x < TET_COLS; x++) tetGrid[yy][x] = tetGrid[yy-1][x];
      for (int x = 0; x < TET_COLS; x++) tetGrid[0][x] = 0;
      y++;
    }
  }
  if (cleared) {
    tetLines += cleared;
    tetScore += (cleared == 1 ? 100 : cleared == 2 ? 300 : cleared == 3 ? 600 : 1000);
    tetLevel = 1 + tetLines / 10;
    gameScoreText(String(tetScore));
    beep(1800, 60);
  }
  tetDrawBoard();
  tetSpawn();
}
void tetMove(int dx) {
  tetErasePiece();
  if (!tetCollideMove(tetPx + dx, tetPy)) tetPx += dx;
  tetDrawPiece();
}
void tetRotate() {
  tetErasePiece();
  if (!tetCollideRotate()) {
    int tmp[4][4];
    for (int y = 0; y < 4; y++)
      for (int x = 0; x < 4; x++)
        tmp[x][3 - y] = tetPiece[y][x];
    for (int y = 0; y < 4; y++)
      for (int x = 0; x < 4; x++)
        tetPiece[y][x] = tmp[y][x];
  }
  tetDrawPiece();
}
void tetInit() {
  clearContent();
  memset(tetGrid, 0, sizeof(tetGrid));
  tetScore = 0; tetLines = 0; tetLevel = 1; tetState = 0;
  tetDrawBoard();
  gameScoreText("0");
  tetSpawn();
  tetFall = millis();
}
void tetLoop() {
  if (tetState == 1) {
    if (tapAfterOver()) tetInit();
    return;
  }
  uint32_t now = millis();
  if (tc.press) {
    if (tc.x < 80) tetMove(-1);
    else if (tc.x > SW - 80) tetMove(1);
    else tetRotate();
  }
  int fallMs = max(150, 700 - tetLevel * 50);
  if (now - tetFall >= (uint32_t)fallMs) {
    tetFall = now;
    tetErasePiece();
    if (!tetCollideMove(tetPx, tetPy + 1)) { tetPy++; tetDrawPiece(); }
    else { tetDrawPiece(); tetLock(); if (tetState == 1) {
      submitScore(8, tetScore); flushHi();
      overlay("Game Over", String("Score ") + tetScore, TH.bad);
    } }
  }
}

// ---- 9: PONG ----
float pgBallX, pgBallY, pgVX, pgVY;
int pgPlayerY, pgCpuY, pgScore, pgCpuScore, pgState;
int pgOldPlayerY, pgOldCpuY, pgOldBallX, pgOldBallY;
uint32_t pgLast = 0;
const int PG_PAD_W = 7, PG_PAD_H = 42, PG_BALL_R = 5;

void pgStatic() {
  tft.fillScreen(TH.bg);
  drawBar("Pong", "NEW", false);
  for (int y = BAR + 4; y < SH; y += 10) tft.fillRect(SW / 2 - 1, y, 2, 5, TH.dim);
  txt("YOU", 45, BAR + 18, 1, TH.accent, TH.bg, MC_DATUM);
  txt("CPU", SW - 45, BAR + 18, 1, TH.accent2, TH.bg, MC_DATUM);
  txt(String(pgScore), SW / 2 - 25, BAR + 18, 4, TH.accent, TH.bg, MC_DATUM);
  txt(String(pgCpuScore), SW / 2 + 25, BAR + 18, 4, TH.accent2, TH.bg, MC_DATUM);
}
void pgDrawPaddle(int x, int y, uint16_t c) { tft.fillRoundRect(x, y, PG_PAD_W, PG_PAD_H, 3, c); }
void pgDrawBall(int x, int y) { tft.fillCircle(x, y, PG_BALL_R, TH.text); }
void pgEraseBall(int x, int y) {
  tft.fillCircle(x, y, PG_BALL_R + 1, TH.bg);
  if (abs(x - SW / 2) < PG_BALL_R + 3) {
    int yy = y - 5;
    for (int dy = -10; dy <= 10; dy += 10) if (yy + dy > BAR + 3 && yy + dy < SH) tft.fillRect(SW/2-1, yy+dy, 2, 5, TH.dim);
  }
}
void pgErasePaddle(int x, int y) { tft.fillRect(x - 1, y - 1, PG_PAD_W + 2, PG_PAD_H + 2, TH.bg); }
void pgDrawDynamic() {
  if (pgOldPlayerY != pgPlayerY) { pgErasePaddle(12, pgOldPlayerY); pgDrawPaddle(12, pgPlayerY, TH.accent); }
  if (pgOldCpuY != pgCpuY) { pgErasePaddle(SW - 19, pgOldCpuY); pgDrawPaddle(SW - 19, pgCpuY, TH.accent2); }
  if (pgOldBallX >= 0) pgEraseBall(pgOldBallX, pgOldBallY);
  pgDrawBall((int)pgBallX, (int)pgBallY);
  pgOldPlayerY = pgPlayerY; pgOldCpuY = pgCpuY;
  pgOldBallX = (int)pgBallX; pgOldBallY = (int)pgBallY;
}
void pgDrawScene() {
  pgStatic();
  pgOldPlayerY = pgPlayerY; pgOldCpuY = pgCpuY; pgOldBallX = -1; pgOldBallY = -1;
  pgDrawPaddle(12, pgPlayerY, TH.accent);
  pgDrawPaddle(SW - 19, pgCpuY, TH.accent2);
  pgDrawBall((int)pgBallX, (int)pgBallY);
  if (pgState == 0) txt("Touch the screen to start", SW / 2, SH - 14, 1, TH.dim, TH.bg, MC_DATUM);
}
void pgResetBall(int dir) {
  pgBallX = SW / 2.0f;
  pgBallY = BAR + 65 + random(10, 110);
  pgVX = dir * 3.0f;
  pgVY = (random(-100, 101)) / 45.0f;
  if (fabsf(pgVY) < 1.0f) pgVY = pgVY < 0 ? -1.0f : 1.0f;
}
void pgInit() {
  pgScore = 0; pgCpuScore = 0; pgState = 0;
  pgPlayerY = 100; pgCpuY = 100;
  pgResetBall(random(0, 2) ? 1 : -1);
  pgLast = millis();
  pgDrawScene();
}
void pgLoop() {
  uint32_t now = millis();
  if (pgState == 2) { if (tc.press && tc.y > BAR) pgInit(); return; }
  if (tc.down && tc.y > BAR) {
    int ny = constrain(tc.y - PG_PAD_H / 2, BAR + 3, SH - PG_PAD_H - 2);
    if (abs(ny - pgPlayerY) > 1) pgPlayerY = ny;
    if (pgState == 0) pgState = 1;
  }
  if (pgState == 0) return;
  if (now - pgLast < 16) return;
  float dt = (now - pgLast) / 16.0f;
  pgLast = now;
  pgBallX += pgVX * dt;
  pgBallY += pgVY * dt;
  if (pgBallY <= BAR + PG_BALL_R) { pgBallY = BAR + PG_BALL_R; pgVY = fabsf(pgVY); }
  if (pgBallY >= SH - PG_BALL_R) { pgBallY = SH - PG_BALL_R; pgVY = -fabsf(pgVY); }
  float cpuCenter = pgCpuY + PG_PAD_H / 2.0f;
  int cpuStep = (fabsf(pgVX) > 4.0f) ? 3 : 2;
  if (cpuCenter < pgBallY - 3) pgCpuY += cpuStep;
  else if (cpuCenter > pgBallY + 3) pgCpuY -= cpuStep;
  pgCpuY = constrain(pgCpuY, BAR + 3, SH - PG_PAD_H - 2);

  if (pgVX < 0 && pgBallX - PG_BALL_R <= 19 && pgBallX > 8 && pgBallY >= pgPlayerY - PG_BALL_R && pgBallY <= pgPlayerY + PG_PAD_H + PG_BALL_R) {
    pgBallX = 19 + PG_BALL_R; pgVX = fabsf(pgVX) + 0.08f;
    pgVY += (pgBallY - (pgPlayerY + PG_PAD_H / 2.0f)) * 0.055f;
    pgVY = constrain(pgVY, -6.0f, 6.0f); beep(1300, 10);
  }
  if (pgVX > 0 && pgBallX + PG_BALL_R >= SW - 19 && pgBallX < SW - 8 && pgBallY >= pgCpuY - PG_BALL_R && pgBallY <= pgCpuY + PG_PAD_H + PG_BALL_R) {
    pgBallX = SW - 19 - PG_BALL_R; pgVX = -fabsf(pgVX) - 0.08f;
    pgVY += (pgBallY - (pgCpuY + PG_PAD_H / 2.0f)) * 0.055f;
    pgVY = constrain(pgVY, -6.0f, 6.0f); beep(1500, 10);
  }
  if (pgBallX < -10) {
    pgCpuScore++; pgResetBall(1);
    if (pgCpuScore >= 5) { pgState = 2; submitScore(9, pgScore); flushHi(); overlay("Pong Over", String("You ") + pgScore + " - " + pgCpuScore, TH.bad); return; }
    pgStatic(); pgDrawDynamic(); return;
  }
  if (pgBallX > SW + 10) {
    pgScore++; pgResetBall(-1);
    if (pgScore >= 5) { pgState = 2; submitScore(9, pgScore); flushHi(); overlay("You Win", String("Score ") + pgScore, TH.good); return; }
    pgStatic(); pgDrawDynamic(); return;
  }
  pgDrawDynamic();
}

// ---- GAME DISPATCHER ----
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
    case 7: simInit(); break;
    case 8: tetInit(); break;
    default: pgInit(); break;
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
    case 7: simLoop(); break;
    case 8: tetLoop(); break;
    default: pgLoop(); break;
  }
}

// ============================ GAMES MENU ====================================
const char* GAME_NAME[10] = {"X & O", "Snake", "Memory", "Whack", "Reflex", "2048", "Bricks", "Simon", "Tetris", "Pong"};
int gtx(int i) { return 4 + (i % 5) * 63; }
int gty(int i) { return 32 + (i / 5) * 102; }

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
    case 7:
      tft.fillRoundRect(cx - 20, cy - 20, 19, 19, 5, SIMB[0]); tft.fillRoundRect(cx + 1, cy - 20, 19, 19, 5, SIMB[1]);
      tft.fillRoundRect(cx - 20, cy + 1, 19, 19, 5, SIMB[2]); tft.fillRoundRect(cx + 1, cy + 1, 19, 19, 5, SIMB[3]);
      break;
    case 8:
      tft.fillRoundRect(cx - 20, cy - 20, 12, 12, 2, C565(0,220,220));
      tft.fillRoundRect(cx - 8, cy - 20, 12, 12, 2, C565(240,220,60));
      tft.fillRoundRect(cx - 20, cy - 8, 12, 12, 2, C565(180,80,220));
      tft.fillRoundRect(cx - 8, cy - 8, 12, 12, 2, C565(60,210,90));
      tft.fillRoundRect(cx + 4, cy - 8, 12, 12, 2, C565(240,70,70));
      tft.fillRoundRect(cx - 20, cy + 4, 12, 12, 2, C565(70,110,240));
      tft.fillRoundRect(cx - 8, cy + 4, 12, 12, 2, C565(240,160,50));
      break;
    default:
      tft.fillRoundRect(cx - 25, cy - 20, 6, 40, 3, TH.accent);
      tft.fillRoundRect(cx + 19, cy - 20, 6, 40, 3, TH.accent2);
      tft.fillCircle(cx, cy, 5, TH.text);
      tft.drawFastHLine(cx - 11, cy, 11, TH.dim);
      break;
  }
}
void gamesInit() {
  tft.fillScreen(TH.bg);
  drawBar("Games", nullptr, false);
  for (int i = 0; i < 10; i++) {
    int x = gtx(i), y = gty(i);
    tft.fillRoundRect(x, y, 58, 94, 10, TH.panel);
    gameIcon(i, x + 29, y + 36, TH.panel);
    txt(GAME_NAME[i], x + 29, y + 68, 2, TH.text, TH.panel, MC_DATUM);
    txt(hiText(i), x + 29, y + 84, 1, TH.dim, TH.panel, MC_DATUM);
  }
}
void gamesLoop() {
  if (!tc.press) return;
  for (int i = 0; i < 10; i++) {
    if (pressIn(gtx(i), gty(i), 58, 94)) { beep(1500, 20); gameStart(i); return; }
  }
}

// ============================ PHONE CONTROL =================================
WebServer server(80);
const char* SCREEN_NAMES[11] = {"Home", "Clock", "Timer", "Weather", "Games", "Settings", "Game", "Wi-Fi Setup", "Islamic", "Alarms", "Knowledge"};

String jsonEscape(const String &in) {
  String out;
  out.reserve(in.length() + 8);
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    if (c == '\\') out += "\\\\";
    else if (c == '"') out += "\\\"";
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else out += c;
  }
  return out;
}

void wsRefreshTimerUI() {
  if (screen != SCR_TIMER) return;
  tmLastA = -1; tmLastB = -1;
  tmDrawMain(); tmDrawSub(); tmDrawButtons();
}

void handleWebRoot() {
  String html = R"rawliteral(
<!doctype html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0b1020">
<title>CYD HUB Pro</title>
<link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;600;800&display=swap" rel="stylesheet">
<style>
:root{--bg:#070b14;--card:#101827;--card2:#151f31;--line:#26344a;--text:#f5f7fb;--muted:#91a0b7;--accent:#29c7ff;--accent2:#b070ff;--good:#36df8a;--bad:#ff5d6c;--gold:#f7c948}
*{box-sizing:border-box}body{margin:0;background:radial-gradient(1200px 600px at 20% -10%,#101c3a 0%,#070b14 60%),#070b14;color:var(--text);font-family:Inter,system-ui,sans-serif;min-height:100vh}
.wrap{max-width:1100px;margin:auto;padding:16px}
.hero{background:linear-gradient(135deg,rgba(41,199,255,.15),rgba(176,112,255,.15));border:1px solid var(--line);border-radius:22px;padding:20px;margin-bottom:16px}
.hero-top{display:flex;align-items:center;justify-content:space-between;gap:12px;flex-wrap:wrap}
.brand{font-size:26px;font-weight:800;background:linear-gradient(90deg,var(--accent),var(--accent2));-webkit-background-clip:text;background-clip:text;color:transparent}
.sub{color:var(--muted);font-size:12px;margin-top:4px}
.pill{padding:10px 14px;border:1px solid var(--line);background:rgba(13,21,35,.85);border-radius:999px;font-size:12px;font-weight:600}
.hero-nav{display:flex;flex-wrap:wrap;gap:8px;margin-top:14px}
.hero-nav button{padding:10px 14px;font-size:13px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(340px,1fr));gap:14px}
.card{background:linear-gradient(180deg,rgba(21,31,49,.95),rgba(16,24,39,.95));border:1px solid var(--line);border-radius:20px;padding:16px;box-shadow:0 18px 40px rgba(0,0,0,.35);position:relative;overflow:hidden}
.card::before{content:"";position:absolute;top:0;left:0;right:0;height:2px;background:linear-gradient(90deg,var(--accent),var(--accent2))}
.card h2{font-weight:800;font-size:15px;margin:0 0 4px;display:flex;align-items:center;gap:8px;color:var(--text)}
.card .subh{font-size:11px;color:var(--muted);margin-bottom:12px;font-weight:500}
.icon{width:22px;height:22px;border-radius:6px;background:linear-gradient(135deg,var(--accent),var(--accent2));display:inline-flex;align-items:center;justify-content:center;font-size:12px}
.muted{color:var(--muted);font-size:12px}
.row{display:flex;flex-wrap:wrap;gap:8px;margin:8px 0}
button,select,input{font:inherit}
button{border:1px solid var(--line);background:var(--card2);color:var(--text);border-radius:12px;padding:10px 14px;cursor:pointer;transition:.15s;font-weight:600}
button:hover{border-color:var(--accent);transform:translateY(-1px)}
button.primary{background:linear-gradient(135deg,var(--accent),#1e8fd0);color:#04101a;border-color:transparent;font-weight:800}
button.primary2{background:linear-gradient(135deg,var(--accent2),#7d44d6);color:#fff;border-color:transparent;font-weight:800}
button.good{background:linear-gradient(135deg,#36df8a,#159f61);color:#04120a;border-color:transparent;font-weight:800}
button.danger{background:linear-gradient(135deg,#ff5d6c,#c2323f);color:#fff;border-color:transparent;font-weight:800}
button.state{min-width:150px}
.on{border-color:#2e9d68!important;background:rgba(18,59,42,.9)!important;color:#7affb1!important}
.off{border-color:#6e3945!important;background:rgba(38,21,27,.9)!important;color:#ff9aa6!important}
.input,select{width:100%;padding:10px 12px;background:#0b1320;border:1px solid var(--line);border-radius:12px;color:var(--text);outline:none}
.input:focus,select:focus{border-color:var(--accent)}
.two{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.status{padding:12px 14px;background:rgba(11,19,32,.85);border:1px solid var(--line);border-radius:12px;font-size:12px;color:var(--muted);margin-top:10px;line-height:1.5}
.network{display:flex;align-items:center;justify-content:space-between;gap:10px;padding:12px;border:1px solid var(--line);border-radius:14px;margin-top:8px;background:rgba(12,20,33,.85)}
.network b{font-size:13px;display:block}
.netmeta{font-size:11px;color:var(--muted);margin-top:3px}
.range{width:100%;accent-color:var(--accent)}
.games button{flex:1 1 105px;font-size:12px;padding:8px 10px}
.badge{display:inline-block;font-size:10px;padding:3px 8px;border-radius:6px;background:rgba(41,199,255,.15);color:var(--accent);font-weight:700;margin-left:6px}
.badge.gold{background:rgba(247,201,72,.15);color:var(--gold)}
.badge.green{background:rgba(54,223,138,.15);color:var(--good)}
.badge.pink{background:rgba(255,90,200,.15);color:#ff5ac8}
.islam-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:6px;margin:8px 0}
.islam-cat{padding:10px;border-radius:12px;border:1px solid var(--line);background:var(--card2);color:var(--text);font-weight:700;font-size:12px;cursor:pointer;text-align:center}
.islam-cat.ayat{background:rgba(41,199,255,.15);color:var(--accent);border-color:rgba(41,199,255,.4)}
.islam-cat.hadith{background:rgba(54,223,138,.15);color:var(--good);border-color:rgba(54,223,138,.4)}
.islam-cat.tip{background:rgba(255,90,200,.15);color:#ff5ac8;border-color:rgba(255,90,200,.4)}
.toast{position:fixed;left:50%;bottom:24px;transform:translate(-50%,20px);background:linear-gradient(180deg,#101827,#0b1320);border:1px solid var(--line);padding:12px 18px;border-radius:14px;opacity:0;pointer-events:none;transition:.25s;z-index:5;font-weight:600}
.toast.show{opacity:1;transform:translate(-50%,0)}
.section-title{font-size:11px;font-weight:800;letter-spacing:.8px;color:var(--muted);text-transform:uppercase;margin:20px 0 10px;padding-left:4px}
@media(max-width:520px){.wrap{padding:11px}.brand{font-size:20px}.two{grid-template-columns:1fr}}
</style></head><body><div class="wrap">

<div class="hero">
  <div class="hero-top">
    <div>
      <div class="brand">CYD HUB Pro Control Center</div>
      <div class="sub">Wi-Fi | Display | Weather | Islamic | Alarm | Games | Timer</div>
    </div>
    <div id="connection" class="pill">Connecting...</div>
  </div>
  <div class="hero-nav">
    <button onclick="go('home')">Home</button>
    <button onclick="go('clock')">Clock</button>
    <button onclick="go('timer')">Timer</button>
    <button onclick="go('alarm')">Alarm</button>
    <button onclick="go('weather')">Weather</button>
    <button onclick="go('islamic')">Islamic</button>
    <button onclick="go('games')">Games</button>
    <button onclick="go('settings')">Settings</button><button onclick="go('knowledge')">Knowledge</button>
    <button class="primary" onclick="go('setup')">Wi-Fi Setup</button>
  </div>
</div>

<div class="section-title">Device Status</div>
<div class="card">
  <h2><span class="icon">i</span> Live Device Status</h2>
  <div class="subh">Real-time information from your CYD display</div>
  <div id="device" class="status">Loading device status...</div>
</div>

<div class="section-title">Wi-Fi & Network</div>
<div class="grid">
  <section class="card">
    <h2><span class="icon">W</span> Wi-Fi Scanner & Connection</h2>
    <div class="subh">Scan nearby networks and connect the CYD</div>
    <div class="row"><button class="primary" onclick="scanWiFi()">Scan Networks</button></div>
    <div id="wifiList"><div class="status">Tap Scan to see nearby Wi-Fi.</div></div>
    <div class="status">
      <b>Control AP:</b> <span id="apInfo">-</span><br>
      <b>Home Wi-Fi:</b> <span id="staInfo">-</span><br>
      <b>Signal:</b> <span id="rssi">-</span>
    </div>
  </section>

  <section class="card">
    <h2><span class="icon">L</span> Live Settings <span class="badge" id="themeBadge">-</span></h2>
    <div class="subh">Quick toggles for common preferences</div>
    <div class="row">
      <button id="h24Btn" class="state" onclick="act('toggle','h24')">24-hour: --</button>
      <button id="soundBtn" class="state" onclick="act('toggle','sound')">Sound: --</button>
      <button id="fahrBtn" class="state" onclick="act('toggle','fahr')">Temp: --</button>
    </div>
    <div class="status" id="stateText">Loading...</div>
  </section>
</div>

<div class="section-title">Display & Appearance</div>
<div class="grid">
  <section class="card">
    <h2><span class="icon">D</span> Display Settings</h2>
    <div class="subh">Theme, brightness and clock style</div>
    <div class="row">
      <button onclick="act('theme','prev')">Prev Theme</button>
      <button class="primary2" onclick="act('theme','next')">Next Theme</button>
    </div>
    <div class="muted" style="margin-top:10px">Brightness: <span id="brightVal">-</span></div>
    <input class="range" type="range" min="10" max="255" id="bright" oninput="setBright(this.value)">
    <div class="two" style="margin-top:10px">
      <div><label class="muted">Clock Style</label>
        <select id="clockStyle" onchange="setClockStyle(this.value)"><option value="0">0: Digital</option><option value="1">1: Analog</option><option value="2">2: Big Digital</option><option value="3">3: Minimal</option><option value="4">4: Ring</option><option value="5">5: Neon</option><option value="6">6: Dashboard</option><option value="7">7: Split</option><option value="8">8: Clean</option><option value="9">9: Seconds</option><option value="10">10: Gear Train</option><option value="11">11: Pendulum</option><option value="12">12: Roman</option><option value="13">13: Moon Phase</option><option value="14">14: Compass</option><option value="15">15: Industrial</option><option value="16">16: Concentric</option><option value="17">17: Binary LED</option><option value="18">18: Sun Dial</option><option value="19">19: Balance</option></select>
      </div>
      <div><label class="muted">Temperature</label>
        <select id="tempUnit" onchange="setTemp(this.value)">
          <option value="C">Celsius</option><option value="F">Fahrenheit</option>
        </select>
      </div>
    </div>
  </section>
</div>

<div class="section-title">Alarm System</div>
<div class="card">
  <h2><span class="icon">A</span> Alarms <span class="badge" id="alarmBadge">Ready</span></h2>
  <div class="subh">Set up to 3 daily alarms. Times are local to the device.</div>
  <div id="alarmList"></div><div class="status">Each alarm can use a repeating day schedule or a specific one-time date. Use the date picker when "specific date" is selected.</div>
  <div class="row">
    <button class="danger" onclick="act('alarm','stop')">Stop Ringing</button>
    <button class="primary2" onclick="act('alarm','snooze')">Snooze 5m</button>
  </div>
</div>

<div class="section-title">Islamic Content</div>
<div class="card">
  <h2><span class="icon">I</span> Daily Islamic <span class="badge gold">Ayat | Hadith | Tip</span></h2>
  <div class="subh">Quranic Ayat, Hadith, and Poramorso. English / Bangla toggle on device.</div>
  <div class="islam-grid">
    <div class="islam-cat ayat" onclick="act('islamic','cat_ayat')">Quran Ayat</div>
    <div class="islam-cat hadith" onclick="act('islamic','cat_hadith')">Hadith</div>
    <div class="islam-cat tip" onclick="act('islamic','cat_tip')">Poramorso</div>
  </div>
  <div class="row">
    <button class="primary2" onclick="act('islamic','next')">Next Item</button>
    <button onclick="act('islamic','prev')">Previous</button>
    <button onclick="act('islamic','random')">Random</button>
    <button class="primary" onclick="act('islamic','cat_all')">Show All</button>
    <button class="good" onclick="go('islamic')">Show on CYD</button>
  </div>
  <div class="status" id="islamicInfo">Tap a category to filter.</div>
</div>

<div class="section-title">Time & Weather</div>
<div class="grid">
  <section class="card">
    <h2><span class="icon">T</span> Time Control</h2>
    <div class="subh">Works with NTP when online, and manual time from this Control AP when offline.</div>
    <div style="margin-bottom:10px">
      <label class="muted">Quick country / timezone preset</label>
      <select id="countryPreset" onchange="applyCountryPreset(this.value)">
        <option value="">Choose preset...</option>
        <option value="dhaka">Bangladesh - Dhaka (UTC+6)</option>
        <option value="kolkata">India - Kolkata (UTC+5:30)</option>
        <option value="karachi">Pakistan - Karachi (UTC+5)</option>
        <option value="kathmandu">Nepal - Kathmandu (UTC+5:45)</option>
        <option value="dubai">UAE - Dubai (UTC+4)</option>
        <option value="riyadh">Saudi Arabia - Riyadh (UTC+3)</option>
        <option value="singapore">Singapore (UTC+8)</option>
        <option value="tokyo">Japan - Tokyo (UTC+9)</option>
        <option value="shanghai">China - Shanghai (UTC+8)</option>
      </select>
    </div>
    <div class="two">
      <div><label class="muted">Format</label>
        <select id="timeFormat" onchange="setTimeFormat(this.value)">
          <option value="12">12-hour</option><option value="24">24-hour</option>
        </select>
      </div>
      <div><label class="muted">Manual Time</label>
        <input id="manualTime" class="input" type="datetime-local">
      </div>
    </div>
    <div class="row">
      <button class="primary" onclick="setManualTime()">Set Time</button>
      <button onclick="syncNtp()">Sync NTP</button>
    </div>
    <div class="status" id="timeStatus">NTP sync uses the saved UTC offset.</div>
  </section>

  <section class="card">
    <h2><span class="icon">Wx</span> Easy Weather Location</h2>
    <div class="subh">Pick a place, or type a city and let Open-Meteo find its coordinates. Wi-Fi is only needed to fetch live weather.</div>
    <label class="muted">Quick location</label>
    <select id="weatherPreset" onchange="applyWeatherPreset(this.value)" style="margin-top:6px">
      <option value="">Choose a city...</option>
      <option value="dhaka">Dhaka, Bangladesh</option><option value="rajshahi">Rajshahi, Bangladesh</option>
      <option value="chittagong">Chattogram, Bangladesh</option><option value="sylhet">Sylhet, Bangladesh</option>
      <option value="kolkata">Kolkata, India</option><option value="karachi">Karachi, Pakistan</option>
      <option value="kathmandu">Kathmandu, Nepal</option><option value="dubai">Dubai, UAE</option>
      <option value="riyadh">Riyadh, Saudi Arabia</option><option value="singapore">Singapore</option>
      <option value="tokyo">Tokyo, Japan</option><option value="shanghai">Shanghai, China</option><option value="doha">Doha, Qatar</option><option value="istanbul">Istanbul, Türkiye</option><option value="london">London, UK</option><option value="newyork">New York, USA</option>
    </select>
    <div class="two" style="margin-top:8px"><input id="citySearch" class="input" placeholder="Type city name"><button class="primary" onclick="findCity()">Find City</button></div>
    <input id="city" class="input" style="margin-top:8px" placeholder="Selected city">
    <div class="two" style="margin-top:8px"><input id="lat" class="input" type="number" step="0.0001" placeholder="Latitude"><input id="lon" class="input" type="number" step="0.0001" placeholder="Longitude"></div>
    <div class="two" style="margin-top:8px"><input id="tzH" class="input" type="number" step="0.25" placeholder="UTC Hours"><input id="dst" class="input" type="number" value="0" placeholder="DST"></div>
    <div class="row"><button class="primary" onclick="saveLocation()">Save Location</button><button onclick="act('refresh','weather')">Refresh Weather</button></div>
    <div class="status" id="geoStatus">Select a preset or find a city.</div>
  </section>
</div>

<div class="section-title">Timer & Games</div>
<div class="grid">
  <section class="card">
    <h2><span class="icon">Tr</span> Timer + Stopwatch</h2>
    <div class="subh">Both are inside the same CYD time tool. Choose the mode here.</div>
    <div class="row"><button class="primary" onclick="act('timer','countdown')">Countdown</button><button class="primary2" onclick="act('timer','stopwatch')">Stopwatch</button></div>
    <div class="row"><button onclick="act('timer','-1m')">-1 min</button><button onclick="act('timer','+1m')">+1 min</button><button onclick="act('timer','-10s')">-10 sec</button><button onclick="act('timer','+10s')">+10 sec</button></div>
    <div class="row"><button class="good" onclick="act('timer','start')">Start / Pause</button><button onclick="act('timer','reset')">Reset</button></div>
  </section>

  <section class="card">
    <h2><span class="icon">G</span> Games Library</h2>
    <div class="subh">10 built-in games. Tap to launch on the CYD.</div>
    <div id="games" class="row games"></div>
  </section>
</div>

<div class="section-title">Device Actions</div>
<div class="card">
  <h2><span class="icon">A</span> System Actions</h2>
  <div class="subh">Refresh, disconnect and utility actions</div>
  <div class="row">
    <button class="primary" onclick="act('refresh','screen')">Refresh Display</button>
    <button onclick="act('refresh','weather')">Refresh Weather</button>
    <button onclick="act('time','sync')">Sync NTP</button>
    <button class="danger" onclick="act('wifi','disconnect')">Disconnect Wi-Fi</button>
  </div>
</div>

<div id="toast" class="toast"></div>

<script>
const games=['X & O','Snake','Memory','Whack','Reflex','2048','Bricks','Simon','Tetris','Pong'];
const ge=document.getElementById('games');
games.forEach((n,i)=>{const b=document.createElement('button');b.textContent=n;b.onclick=()=>act('game',i);ge.appendChild(b);});
function toast(m){const e=document.getElementById('toast');e.textContent=m;e.classList.add('show');setTimeout(()=>e.classList.remove('show'),2200);}
function go(s){fetch('/goto?screen='+encodeURIComponent(s)).then(r=>r.text()).then(()=>{toast('Screen changed');refresh();}).catch(()=>toast('Connection error'));}
function act(k,v){fetch('/act?k='+encodeURIComponent(k)+'&v='+encodeURIComponent(v)).then(r=>r.text()).then(x=>{toast(x||'Done');refresh();}).catch(()=>toast('Connection error'));}
function setBright(v){document.getElementById('brightVal').textContent=v;fetch('/act?k=bright&v='+v).catch(()=>toast('Connection error'));}
function setClockStyle(v){act('clock',v);}
function setTemp(v){act('temp',v);}
function setTimeFormat(v){act('format',v);}
function syncNtp(){act('time','sync');}
const countryPresets={
 dhaka:{city:'Dhaka',lat:23.8103,lon:90.4125,tz:6},
 kolkata:{city:'Kolkata',lat:22.5726,lon:88.3639,tz:5.5},
 karachi:{city:'Karachi',lat:24.8607,lon:67.0011,tz:5},
 kathmandu:{city:'Kathmandu',lat:27.7172,lon:85.3240,tz:5.75},
 dubai:{city:'Dubai',lat:25.2048,lon:55.2708,tz:4},
 riyadh:{city:'Riyadh',lat:24.7136,lon:46.6753,tz:3},
 singapore:{city:'Singapore',lat:1.3521,lon:103.8198,tz:8},
 tokyo:{city:'Tokyo',lat:35.6762,lon:139.6503,tz:9},
 shanghai:{city:'Shanghai',lat:31.2304,lon:121.4737,tz:8}
};
const weatherPresets={
 dhaka:{city:'Dhaka',lat:23.8103,lon:90.4125,tz:6},rajshahi:{city:'Rajshahi',lat:24.3745,lon:88.6042,tz:6},
 chittagong:{city:'Chattogram',lat:22.3569,lon:91.7832,tz:6},sylhet:{city:'Sylhet',lat:24.8949,lon:91.8687,tz:6},
 kolkata:{city:'Kolkata',lat:22.5726,lon:88.3639,tz:5.5},karachi:{city:'Karachi',lat:24.8607,lon:67.0011,tz:5},
 kathmandu:{city:'Kathmandu',lat:27.7172,lon:85.3240,tz:5.75},dubai:{city:'Dubai',lat:25.2048,lon:55.2708,tz:4},
 riyadh:{city:'Riyadh',lat:24.7136,lon:46.6753,tz:3},singapore:{city:'Singapore',lat:1.3521,lon:103.8198,tz:8},
 tokyo:{city:'Tokyo',lat:35.6762,lon:139.6503,tz:9},shanghai:{city:'Shanghai',lat:31.2304,lon:121.4737,tz:8},
 newyork:{city:'New York',lat:40.7128,lon:-74.0060,tz:-4},london:{city:'London',lat:51.5074,lon:-0.1278,tz:0},
 doha:{city:'Doha',lat:25.2854,lon:51.5310,tz:3},istanbul:{city:'Istanbul',lat:41.0082,lon:28.9784,tz:3}
};
function applyWeatherPreset(k){const p=weatherPresets[k];if(!p)return;document.getElementById('city').value=p.city;document.getElementById('lat').value=p.lat;document.getElementById('lon').value=p.lon;document.getElementById('tzH').value=p.tz;document.getElementById('geoStatus').textContent=p.city+' selected. Tap Save Location.';}
function findCity(){const q=document.getElementById('citySearch').value.trim();if(!q){toast('Type a city first');return;}document.getElementById('geoStatus').textContent='Searching...';fetch('/geo?q='+encodeURIComponent(q)).then(r=>r.json()).then(d=>{if(!d.ok){document.getElementById('geoStatus').textContent=d.message||'City not found';return;}document.getElementById('city').value=d.city;document.getElementById('lat').value=d.lat;document.getElementById('lon').value=d.lon;document.getElementById('tzH').value=d.tz;document.getElementById('geoStatus').textContent=d.city+' found. Tap Save Location.';}).catch(()=>document.getElementById('geoStatus').textContent='Search failed. Check Wi-Fi.');}

function applyCountryPreset(k){
 const p=countryPresets[k]; if(!p)return;
 document.getElementById('city').value=p.city;
 document.getElementById('lat').value=p.lat;
 document.getElementById('lon').value=p.lon;
 document.getElementById('tzH').value=p.tz;
 saveLocation();
}
function saveLocation(){
 const city=document.getElementById('city').value.trim(), lat=Number(document.getElementById('lat').value), lon=Number(document.getElementById('lon').value), tz=Number(document.getElementById('tzH').value), dst=Number(document.getElementById('dst').value||0);
 if(!city || !Number.isFinite(lat)||!Number.isFinite(lon)||!Number.isFinite(tz)||lat<-90||lat>90||lon<-180||lon>180||tz<-14||tz>14){toast('Check city, latitude, longitude and timezone');return;}
 const q='city='+encodeURIComponent(city)+'&lat='+lat.toFixed(6)+'&lon='+lon.toFixed(6)+'&tz='+tz.toFixed(2)+'&dst='+Math.round(dst*60);
 fetch('/act?k=location&'+q).then(r=>r.text()).then(x=>{toast(x);document.getElementById('geoStatus').textContent=x+' • Weather will refresh.';refresh();}).catch(()=>toast('Location save failed'));
}
function setManualTime(){
 const v=document.getElementById('manualTime').value;
 if(!v){toast('Select date and time first');return;}
 fetch('/act?k=timeSet&v='+encodeURIComponent(v)).then(r=>r.text()).then(x=>{toast(x);refresh();});
}
function setState(id,on,onText,offText){const e=document.getElementById(id);e.textContent=on?onText:offText;e.classList.toggle('on',on);e.classList.toggle('off',!on);}
function alarmDaysLabel(d){return d===127?'Every day':d===62?'Weekdays':d===65?'Weekends':'Custom';}
function alarmDateText(day){ if(!day) return 'Choose date'; const d=new Date(day*86400000); return d.toISOString().slice(0,10); }
let alarmUiReady=false;
let lastAlarmJson='';
function buildAlarmUi(){
 const box=document.getElementById('alarmList');
 if(!box||alarmUiReady)return;
 box.innerHTML='';
 for(let i=0;i<3;i++){
  const row=document.createElement('div'); row.className='network'; row.id='alarmRow'+i;
  const left=document.createElement('div');
  const title=document.createElement('b'); title.textContent='Alarm '+(i+1);
  const meta=document.createElement('div'); meta.className='netmeta'; meta.id='alarmDays'+i;
  const inp=document.createElement('input');
  inp.type='time'; inp.className='input'; inp.id='alarmTime'+i;
  inp.style.width='120px'; inp.style.marginTop='6px';
  inp.onchange=()=>saveAlarmTime(i);
  left.appendChild(title); left.appendChild(meta); left.appendChild(inp);
  const date=document.createElement('input'); date.type='date'; date.className='input'; date.id='alarmDate'+i; date.style.width='150px'; date.style.marginTop='6px'; date.onchange=()=>saveAlarmDate(i); left.appendChild(date);
  const mode=document.createElement('button'); mode.id='alarmMode'+i; mode.style.marginTop='6px'; mode.onclick=()=>toggleAlarmMode(i); left.appendChild(mode);
  const right=document.createElement('div');
  const b=document.createElement('button'); b.id='alarmToggle'+i;
  b.onclick=()=>fetch('/act?k=alarm&v=toggle&i='+i).then(()=>refresh());
  right.appendChild(b);
  row.appendChild(left); row.appendChild(right); box.appendChild(row);
 }
 alarmUiReady=true;
}
function saveAlarmTime(i){
 const inp=document.getElementById('alarmTime'+i);
 const p=(inp.value||'').split(':'); if(p.length!==2)return;
 const days=Number(inp.dataset.days||127), enabled=inp.dataset.enabled==='1';
 fetch('/act?k=alarm&v=set&i='+i+'&h='+encodeURIComponent(p[0])+'&m='+encodeURIComponent(p[1])+'&d='+days+'&e='+(enabled?1:0))
  .then(r=>r.text()).then(x=>{toast(x);refresh();}).catch(()=>toast('Alarm save failed'));
}
function toggleAlarmMode(i){
 const inp=document.getElementById('alarmTime'+i); const date=document.getElementById('alarmDate'+i);
 const dm=inp.dataset.dateMode==='1'; const nd=dm?'0':'1';
 const h=(inp.value||'00:00').split(':'); const dd=dm?127:Number(inp.dataset.days||127);
 const dateDay=nd==='1' ? Math.floor(Date.UTC(...(date.value||new Date().toISOString().slice(0,10)).split('-').map(Number))/86400000) : 0;
 fetch('/act?k=alarm&v=set&i='+i+'&h='+h[0]+'&m='+h[1]+'&d='+dd+'&e='+(inp.dataset.enabled==='1'?1:0)+'&dm='+nd+'&dd='+dateDay).then(r=>r.text()).then(x=>{toast(x);refresh();});
}
function saveAlarmDate(i){
 const inp=document.getElementById('alarmTime'+i), date=document.getElementById('alarmDate'+i);
 if(inp.dataset.dateMode!=='1'||!date.value)return;
 const h=(inp.value||'00:00').split(':'); const dd=Math.floor(Date.UTC(...date.value.split('-').map(Number))/86400000);
 fetch('/act?k=alarm&v=set&i='+i+'&h='+h[0]+'&m='+h[1]+'&d=127&e='+(inp.dataset.enabled==='1'?1:0)+'&dm=1&dd='+dd).then(r=>r.text()).then(x=>{toast(x);refresh();});
}
function renderAlarms(list){
 if(!list)return;
 buildAlarmUi();
 const sig=JSON.stringify(list);
 if(sig===lastAlarmJson)return;
 list.forEach((a,i)=>{
  const inp=document.getElementById('alarmTime'+i);
  const meta=document.getElementById('alarmDays'+i);
  const b=document.getElementById('alarmToggle'+i);
  meta.textContent=a.dm ? ('ONE-TIME • '+alarmDateText(a.dd)) : alarmDaysLabel(a.d);
  inp.dataset.days=a.d; inp.dataset.enabled=a.e?'1':'0'; inp.dataset.dateMode=a.dm?'1':'0'; inp.dataset.dateDay=a.dd||0;
  // Never overwrite a time field while the user is editing/opening its picker.
  if(document.activeElement!==inp){
    const v=String(a.h).padStart(2,'0')+':'+String(a.m).padStart(2,'0');
    if(inp.value!==v) inp.value=v;
  }
  const date=document.getElementById('alarmDate'+i); const mode=document.getElementById('alarmMode'+i);
  date.value=a.dm ? alarmDateText(a.dd) : ''; date.style.display=a.dm?'block':'none';
  mode.textContent=a.dm?'Use day schedule':'Use specific date';
  b.textContent=a.e?'ON':'OFF'; b.className=a.e?'good':'';
 });
 lastAlarmJson=sig;
}
function safeValue(id,value){
 const e=document.getElementById(id);
 if(e && document.activeElement!==e) e.value=value;
}
function refresh(){fetch('/status').then(r=>r.json()).then(d=>{
 document.getElementById('connection').textContent=d.sta?'Home Wi-Fi Connected':'Control AP Active';
 document.getElementById('connection').style.color=d.sta?'#36df8a':'#29c7ff';
 document.getElementById('device').innerHTML='<b>Screen:</b> '+d.screen+' &nbsp; <b>Time:</b> '+d.time+'<br><b>Location:</b> '+escapeHtml(d.city)+' &nbsp; <b>TZ:</b> UTC'+escapeHtml(d.tz)+'<br><b>Brightness:</b> '+d.bright+' &nbsp; <b>Clock:</b> '+escapeHtml(d.clockStyle);
 document.getElementById('apInfo').textContent=d.ap+' / '+d.apip;
 document.getElementById('staInfo').textContent=d.sta?(d.ssid+' / '+d.staip):'Not connected';
 document.getElementById('rssi').textContent=d.sta?(d.rssi+' dBm'):'-';
 document.getElementById('bright').value=d.bright;document.getElementById('brightVal').textContent=d.bright;
 document.getElementById('clockStyle').value=d.clockIndex;document.getElementById('tempUnit').value=d.fahr?'F':'C';document.getElementById('timeFormat').value=d.h24?'24':'12';
 safeValue('city',d.city);safeValue('lat',d.lat);safeValue('lon',d.lon);safeValue('tzH',d.tzHours);
 setState('h24Btn',d.h24,'24-hour: ON','24-hour: OFF');setState('soundBtn',d.sound,'Sound: ON','Sound: OFF');setState('fahrBtn',d.fahr,'Fahrenheit: ON','Celsius: ON');
 document.getElementById('themeBadge').textContent='Theme: '+d.theme;
 document.getElementById('stateText').textContent='Clock: '+d.clockStyle+' | Wi-Fi: '+(d.sta?'ON':'OFF')+' | Weather: '+(!d.sta?'OFFLINE':(d.weather?'ACTIVE':'WAITING'))+' | Islamic: '+(d.islamicCat||'All');
 document.getElementById('islamicInfo').textContent='Category: '+(d.islamicCat||'All')+' | Current: '+escapeHtml(d.islamicRef||'-');
 if(d.alarms) renderAlarms(d.alarms);
 document.getElementById('alarmBadge').textContent=d.alarmRinging?'RINGING!':'Ready';
 document.getElementById('alarmBadge').style.color=d.alarmRinging?'#ff5d6c':'';
}).catch(()=>{document.getElementById('connection').textContent='Offline';});}
function scanWiFi(){const box=document.getElementById('wifiList');box.innerHTML='<div class="status">Scanning...</div>';fetch('/wifi/scan').then(r=>r.json()).then(d=>{
 if(!d.networks||!d.networks.length){box.innerHTML='<div class="status">No networks found.</div>';return;}
 box.innerHTML='';d.networks.forEach(n=>{const row=document.createElement('div');row.className='network';
 const left=document.createElement('div');left.innerHTML='<b>'+escapeHtml(n.ssid||'(Hidden)')+'</b><div class="netmeta">'+n.rssi+' dBm - '+(n.secure?'Secured':'Open')+'</div>';
 const actions=document.createElement('div');const b=document.createElement('button');b.textContent='Connect';b.className='primary';b.onclick=()=>connectWiFi(n.ssid,n.secure);actions.appendChild(b);row.appendChild(left);row.appendChild(actions);box.appendChild(row);});
 toast(d.networks.length+' networks found');}).catch(()=>{box.innerHTML='<div class="status">Scan failed.</div>';});}
function connectWiFi(ssid,secure){const pass=secure?prompt('Password for '+ssid):'';if(pass===null)return;
 const body='ssid='+encodeURIComponent(ssid)+'&pass='+encodeURIComponent(pass);fetch('/wifi/connect',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body}).then(r=>r.text()).then(x=>{toast(x);setTimeout(refresh,1500);}).catch(()=>toast('Connection failed'));}
function escapeHtml(s){return String(s).replace(/[&<>'"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;',"'":'&#39;','"':'&quot;'}[c]));}
refresh();setInterval(refresh,2500);
</script></body></html>
)rawliteral";
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
  else if (s == "setup") target = SCR_SETUP;
  else if (s == "islamic") target = SCR_ISLAMIC;
  else if (s == "alarm") target = SCR_ALARM;
  else if (s == "knowledge") target = SCR_KNOWLEDGE;
  if (target >= 0) { flushHi(); goScreen(target); }
  server.send(200, "text/plain", target >= 0 ? "Navigation updated" : "Unknown screen");
}

long daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097L + (long)doe - 719468L;
}
bool setManualLocalTime(const String &v) {
  if (v.length() < 16) return false;
  int y = v.substring(0,4).toInt(), mo = v.substring(5,7).toInt();
  int d = v.substring(8,10).toInt(), h = v.substring(11,13).toInt();
  int mi = v.substring(14,16).toInt(), s = v.length() >= 19 ? v.substring(17,19).toInt() : 0;
  if (y < 2020 || mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || s > 59) return false;
  time_t epoch = (time_t)(daysFromCivil(y, mo, d) * 86400L + h * 3600L + mi * 60L + s - savedTzOffset());
  struct timeval tv = { epoch, 0 };
  settimeofday(&tv, nullptr);
  return true;
}
void applyTimeSettings() {
  configTime(savedTzOffset(), savedDstOffset(), "pool.ntp.org", "time.google.com", "time.cloudflare.com");
  ntpStarted = true;
}

const char* islamicCatName(uint8_t c) {
  switch (c) { case 1: return "Ayat"; case 2: return "Hadith"; case 3: return "Tip"; default: return "All"; }
}

void handleWebAct() {
  String k = server.arg("k");
  String v = server.arg("v");
  String msg = "Command completed";

  if (k == "game") {
    int idx = v.toInt();
    if (idx >= 0 && idx < 10) { gameStart(idx); msg = "Game started"; }
  } else if (k == "timer") {
    if (v == "countdown") { if(swRun){swMs += millis()-swLast; swRun=false;} tmTab=0; cdRun=false; if(screen==SCR_TIMER) timerInit(); msg="Countdown mode"; wsRefreshTimerUI(); return; }
    if (v == "stopwatch") { cdRun=false; tmTab=1; if(screen==SCR_TIMER) timerInit(); msg="Stopwatch mode"; wsRefreshTimerUI(); return; }
    if (v == "start" && tmTab==1) { if(swRun){swMs += millis()-swLast; swRun=false;} else {swLast=millis(); swRun=true;} msg=swRun?"Stopwatch started":"Stopwatch paused"; wsRefreshTimerUI(); return; }
    if (v == "reset" && tmTab==1) { swRun=false; swMs=0; msg="Stopwatch reset"; wsRefreshTimerUI(); return; }
    int delta = 0;
    if (v == "-1m") delta = -60;
    else if (v == "+1m") delta = 60;
    else if (v == "-10s") delta = -10;
    else if (v == "+10s") delta = 10;
    if (delta && tmTab==0 && !cdRun && !cdDone) {
      int t = (int)(cdTotal / 1000) + delta;
      t = constrain(t, 10, 5990);
      cdTotal = (uint32_t)t * 1000UL; cdLeft = cdTotal; msg = "Timer updated";
    } else if (v == "start") {
      if (cdDone) { cdDone = false; cdLeft = cdTotal; }
      else if (cdRun) cdRun = false;
      else { if (cdLeft == 0) cdLeft = cdTotal; cdRun = true; cdLast = millis(); }
      msg = cdRun ? "Timer started" : "Timer paused";
    } else if (v == "reset") {
      cdRun = false; cdDone = false; cdLeft = cdTotal; msg = "Timer reset";
    }
    wsRefreshTimerUI();
  } else if (k == "theme") {
    if (v == "prev") cfg.theme = (cfg.theme + NTHEMES - 1) % NTHEMES;
    else cfg.theme = (cfg.theme + 1) % NTHEMES;
    saveCfg();
    goScreen(screen); msg = "Theme updated";
  } else if (k == "toggle") {
    if (v == "h24") cfg.h24 = !cfg.h24;
    else if (v == "sound") cfg.sound = !cfg.sound;
    else if (v == "fahr") cfg.fahr = !cfg.fahr;
    saveCfg(); goScreen(screen);
    msg = "Preference updated";
  } else if (k == "bright") {
    int b = constrain(v.toInt(), 10, 255);
    cfg.bright = b; setBacklight(b); saveCfg();
    if (screen == SCR_SETTINGS) setRowBright();
    msg = "Brightness updated";
  } else if (k == "clock") {
    int st = constrain(v.toInt(), 0, 19);
    cfg.clockStyle = st; saveCfg();
    if (screen == SCR_CLOCK) clockInit(); else if (screen == SCR_SETTINGS) setRowClockStyle();
    msg = "Clock style set to " + String(CLOCK_STYLE_NAMES[st]);
  } else if (k == "temp") {
    cfg.fahr = (v == "F"); saveCfg();
    if (screen == SCR_SETTINGS) setRowTemp();
    else if (screen == SCR_HOME) homeInit();
    msg = cfg.fahr ? "Fahrenheit enabled" : "Celsius enabled";
  } else if (k == "format") {
    cfg.h24 = (v == "24"); saveCfg();
    if (screen == SCR_SETTINGS) setRowH24();
    else if (screen == SCR_CLOCK || screen == SCR_HOME) goScreen(screen);
    msg = cfg.h24 ? "24-hour format enabled" : "12-hour format enabled";
  } else if (k == "location") {
    String city = server.arg("city"); city.trim();
    float lat = server.arg("lat").toFloat(), lon = server.arg("lon").toFloat();
    float tzH = server.arg("tz").toFloat();
    if (!city.length() || lat < -90 || lat > 90 || lon < -180 || lon > 180 || tzH < -14 || tzH > 14) {
      msg = "Invalid location or timezone";
    } else {
      long dstMinutes = (long)server.arg("dst").toInt();
      dstMinutes = constrain(dstMinutes, -120, 120);
      saveLocationConfig(city, lat, lon, (long)lroundf(tzH * 3600.0f), dstMinutes * 60L);
      applyTimeSettings(); wx.ok = false; wxTried = false;
      if (screen == SCR_WEATHER) wxRefresh();
      msg = "Location and timezone saved";
    }
  } else if (k == "time") {
    if (v == "sync") {
      ntpStarted = false; applyTimeSettings(); msg = "NTP sync requested";
    }
  } else if (k == "timeSet") {
    if (setManualLocalTime(v)) { alarmLastMinute = -1; alarmLastDay = -1; alarmSuppress = false; msg = "Device time updated"; }
    else msg = "Invalid date/time";
  } else if (k == "refresh") {
    if (v == "screen") { goScreen(screen); msg = "Display refreshed"; }
    else if (v == "weather") { wx.ok = false; wxTried = false; if (WiFi.status() == WL_CONNECTED) wxRefresh(); msg = "Weather refreshed"; }
  } else if (k == "islamic") {
    if (v == "next") {
      islamicIdx = islamicNext(islamicIdx, 1, islamicCat);
      msg = "Next Islamic item";
    } else if (v == "prev") {
      islamicIdx = islamicNext(islamicIdx, -1, islamicCat);
      msg = "Previous Islamic item";
    } else if (v == "random") {
      int tries = 0, n = islamicIdx;
      do { n = random(ISLAMIC_COUNT); tries++; }
      while (islamicCat != 0 && ISLAMIC_DB[n].category != islamicCat && tries < 50);
      islamicIdx = n;
      msg = "Random Islamic item";
    } else if (v == "cat_ayat") {
      islamicCat = 1;
      for (int j = 0; j < ISLAMIC_COUNT; j++) if (ISLAMIC_DB[j].category == 1) { islamicIdx = j; break; }
      msg = "Filter: Quran Ayat";
    } else if (v == "cat_hadith") {
      islamicCat = 2;
      for (int j = 0; j < ISLAMIC_COUNT; j++) if (ISLAMIC_DB[j].category == 2) { islamicIdx = j; break; }
      msg = "Filter: Hadith";
    } else if (v == "cat_tip") {
      islamicCat = 3;
      for (int j = 0; j < ISLAMIC_COUNT; j++) if (ISLAMIC_DB[j].category == 3) { islamicIdx = j; break; }
      msg = "Filter: Poramorso (Tips)";
    } else if (v == "cat_all") {
      islamicCat = 0;
      msg = "Showing all categories";
    }
    if (screen == SCR_ISLAMIC) islamicDraw();
  } else if (k == "alarm") {
    if (v == "stop") {
      if (alarmRinging) { dismissAlarm(); msg = "Alarm stopped"; }
      else msg = "No alarm ringing";
    } else if (v == "snooze") {
      if (alarmRinging) { snoozeAlarm(); msg = "Snoozed 5 minutes"; }
      else msg = "No alarm ringing";
    } else if (v == "toggle") {
      int i = server.arg("i").toInt();
      if (i >= 0 && i < 3) {
        alarms[i].enabled = !alarms[i].enabled; alarmLastFireMinute = -1; alarmLastFireDay = -1; saveAlarms();
        if (screen == SCR_ALARM) alarmDrawRow(i);
        msg = String("Alarm ") + (i + 1) + (alarms[i].enabled ? " enabled" : " disabled");
      }
    } else if (v == "set") {
      int i = server.arg("i").toInt();
      int h = server.arg("h").toInt();
      int m = server.arg("m").toInt();
      int d = server.arg("d").toInt();
      bool e = server.arg("e").toInt() != 0;
      bool dm = server.hasArg("dm") ? (server.arg("dm").toInt() != 0) : alarms[i].dateMode;
      long dd = server.hasArg("dd") ? server.arg("dd").toInt() : alarms[i].dateDay;
      if (i >= 0 && i < 3 && h >= 0 && h < 24 && m >= 0 && m < 60 && d >= 0 && d <= 127) {
        alarms[i].hour = h; alarms[i].minute = m;
        alarms[i].days = d; alarms[i].enabled = e; alarms[i].dateMode = dm; alarms[i].dateDay = dd;
        saveAlarms();
        if (screen == SCR_ALARM) alarmDrawRow(i);
        msg = String("Alarm ") + (i + 1) + " updated";
      } else msg = "Invalid alarm";
    }
  } else if (k == "wifi" && v == "disconnect") {
    WiFi.disconnect(); ntpStarted = false; msg = "Wi-Fi disconnected";
  }
  server.send(200, "text/plain", msg);
}

void handleWebStatus() {
  String t = "--:--";
  if (timeValid()) {
    if (cfg.h24) {
      char b[12]; snprintf(b, sizeof(b), "%02d:%02d:%02d", nowTm.tm_hour, nowTm.tm_min, nowTm.tm_sec); t = String(b);
    } else {
      char b[16]; snprintf(b, sizeof(b), "%d:%02d:%02d %s", hour12(nowTm.tm_hour), nowTm.tm_min, nowTm.tm_sec, nowTm.tm_hour >= 12 ? "PM" : "AM"); t = String(b);
    }
  }
  String islamicRef = ISLAMIC_DB[islamicIdx].ref;
  if (!islamicRef.length()) islamicRef = String(ISLAMIC_DB[islamicIdx].translation).substring(0, 40) + "...";

  String json = "{";
  json += "\"screen\":\"" + jsonEscape(String(SCREEN_NAMES[screen])) + "\",";
  json += "\"time\":\"" + jsonEscape(t) + "\",";
  json += "\"city\":\"" + jsonEscape(savedCity()) + "\",";
  json += "\"lat\":" + String(savedLat(), 4) + ",";
  json += "\"lon\":" + String(savedLon(), 4) + ",";
  json += "\"tz\":\"" + jsonEscape(tzLabel()) + "\",";
  json += "\"tzHours\":" + String(savedTzOffset() / 3600.0f, 2) + ",";
  json += "\"bright\":" + String(cfg.bright) + ",";
  json += "\"h24\":" + String(cfg.h24 ? "true" : "false") + ",";
  json += "\"sound\":" + String(cfg.sound ? "true" : "false") + ",";
  json += "\"fahr\":" + String(cfg.fahr ? "true" : "false") + ",";
  json += "\"clockIndex\":" + String(cfg.clockStyle) + ",";
  json += "\"clockStyle\":\"" + jsonEscape(String(CLOCK_STYLE_NAMES[cfg.clockStyle])) + "\",";
  json += "\"theme\":\"" + jsonEscape(String(TH.name)) + "\",";
  json += "\"weather\":" + String(wx.ok ? "true" : "false") + ",";
  json += "\"islamicCat\":\"" + jsonEscape(String(islamicCatName(islamicCat))) + "\",";
  json += "\"islamicRef\":\"" + jsonEscape(islamicRef) + "\",";
  json += "\"sta\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
  json += "\"ssid\":\"" + jsonEscape(WiFi.status() == WL_CONNECTED ? WiFi.SSID() : savedWifiSSID()) + "\",";
  json += "\"staip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"rssi\":" + String(WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0) + ",";
  json += "\"ap\":\"" + jsonEscape(String(CONTROL_AP_SSID)) + "\",";
  json += "\"apip\":\"" + WiFi.softAPIP().toString() + "\",";
  json += "\"alarmRinging\":" + String(alarmRinging ? "true" : "false") + ",";
  json += "\"alarms\":[";
  for (int i = 0; i < 3; i++) {
    if (i) json += ",";
    json += "{\"h\":" + String(alarms[i].hour) +
            ",\"m\":" + String(alarms[i].minute) +
            ",\"e\":" + String(alarms[i].enabled ? "true" : "false") +
            ",\"d\":" + String(alarms[i].days) +
            ",\"dm\":" + String(alarms[i].dateMode ? "true" : "false") +
            ",\"dd\":" + String(alarms[i].dateDay) + "}";
  }
  json += "],";
  json += "\"message\":\"" + jsonEscape(wifiMessage) + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleWiFiScan() {
  int n = WiFi.scanComplete();
  if (n != WIFI_SCAN_FAILED) WiFi.scanDelete();
  n = WiFi.scanNetworks(false, true);
  String json = "{\"networks\":[";
  if (n > 0) {
    int idx[40];
    int count = n > 40 ? 40 : n;
    for (int i = 0; i < count; i++) idx[i] = i;
    for (int a = 0; a < count - 1; a++) {
      for (int b = a + 1; b < count; b++) {
        if (WiFi.RSSI(idx[b]) > WiFi.RSSI(idx[a])) {
          int tmp = idx[a]; idx[a] = idx[b]; idx[b] = tmp;
        }
      }
    }
    for (int p = 0; p < count; p++) {
      int i = idx[p];
      if (p) json += ",";
      String ssid = jsonEscape(WiFi.SSID(i));
      json += "{\"ssid\":\"" + ssid + "\",\"rssi\":" + String(WiFi.RSSI(i)) +
              ",\"secure\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false") + "}";
    }
  }
  json += "]}";
  WiFi.scanDelete();
  server.send(200, "application/json", json);
}

void handleWiFiConnect() {
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  ssid.trim();
  if (!ssid.length()) {
    server.send(400, "text/plain", "SSID is required");
    return;
  }
  saveWifiCredentials(ssid, pass);
  WiFi.disconnect(false, false);
  delay(100);
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  lastWifiTry = millis();
  wifiConnectingFromWeb = true;
  ntpStarted = false;
  setWifiMessage("Connecting to " + ssid);
  server.send(200, "text/plain", "Wi-Fi credentials saved. Connecting to " + ssid + "...");
}

void handleWebGeo() {
  String q = server.arg("q"); q.trim();
  if (!q.length()) { server.send(400,"application/json","{\"ok\":false,\"message\":\"City required\"}"); return; }
  if (WiFi.status() != WL_CONNECTED) { server.send(200,"application/json","{\"ok\":false,\"message\":\"Connect Wi-Fi first for city search\"}"); return; }
  WiFiClientSecure client; client.setInsecure(); HTTPClient http; http.setTimeout(8000);
  String url = String("https://geocoding-api.open-meteo.com/v1/search?name=") + q + "&count=1&language=en&format=json";
  if (!http.begin(client,url)) { server.send(200,"application/json","{\"ok\":false,\"message\":\"Geocoding unavailable\"}"); return; }
  int code=http.GET(); if(code!=HTTP_CODE_OK){http.end();server.send(200,"application/json","{\"ok\":false,\"message\":\"City search failed\"}");return;}
  String body=http.getString(); http.end(); JsonDocument doc; if(deserializeJson(doc,body)){server.send(200,"application/json","{\"ok\":false,\"message\":\"Invalid geocoding response\"}");return;}
  JsonObject r=doc["results"][0]; if(r.isNull()){server.send(200,"application/json","{\"ok\":false,\"message\":\"City not found\"}");return;}
  String name=String((const char*)(r["name"]|"")); String country=String((const char*)(r["country"]|""));
  float lat=r["latitude"]|0.0f, lon=r["longitude"]|0.0f; String tzs=String((const char*)(r["timezone"]|""));
  long tz=0;
  if(tzs=="Asia/Dhaka")tz=21600; else if(tzs=="Asia/Kolkata")tz=19800; else if(tzs=="Asia/Kathmandu")tz=20700;
  else if(tzs=="Asia/Karachi")tz=18000; else if(tzs=="Asia/Dubai")tz=14400; else if(tzs=="Asia/Riyadh")tz=10800;
  else if(tzs=="Asia/Singapore")tz=28800; else if(tzs=="Asia/Tokyo")tz=32400; else if(tzs=="Asia/Shanghai")tz=28800;
  else if(tzs=="Europe/London")tz=0; else if(tzs=="Europe/Istanbul")tz=10800; else if(tzs=="America/New_York")tz=-18000;
  else if(tzs=="Asia/Qatar")tz=10800;
  String tzH=String(tz/3600.0f,2); String json="{\"ok\":true,\"city\":\""+jsonEscape(name+", "+country)+"\",\"lat\":"+String(lat,5)+",\"lon\":"+String(lon,5)+",\"tz\":"+tzH+"}"; server.send(200,"application/json",json);
}

void setupWebServer() {
  server.on("/", HTTP_GET, handleWebRoot);
  server.on("/goto", HTTP_GET, handleWebGoto);
  server.on("/act", HTTP_GET, handleWebAct);
  server.on("/status", HTTP_GET, handleWebStatus);
  server.on("/wifi/scan", HTTP_GET, handleWiFiScan);
  server.on("/wifi/connect", HTTP_POST, handleWiFiConnect);
  server.on("/geo", HTTP_GET, handleWebGeo);
  server.begin();
}

// ============================ SCREEN ROUTER =================================
void goScreen(int s) {
  if (screen == SCR_GAME && s != SCR_GAME) flushHi();
  if (screen == SCR_CLOCK && s != SCR_CLOCK) clockSpriteEnd();

  screen = s;
  switch (s) {
    case SCR_HOME:     homeInit();     break;
    case SCR_CLOCK:    clockInit();    break;
    case SCR_TIMER:    timerInit();    break;
    case SCR_WEATHER:  weatherInit();  break;
    case SCR_GAMES:    gamesInit();    break;
    case SCR_SETTINGS: settingsInit(); break;
    case SCR_SETUP:    setupInit();    break;
    case SCR_ISLAMIC:  islamicInit();  break;
    case SCR_ALARM:    alarmInit();    break;
    case SCR_KNOWLEDGE: knowledgeInit(); break;
    default: break;
  }
}

// ============================ SETUP / LOOP ==================================
void splash() {
  tft.fillScreen(TH.bg);
  txtp("CYD HUB Pro", SW / 2, 96, 4, TH.accent, TH.bg, MC_DATUM, 2, 0);
  txt("Clock  Timer  Alarm  Islamic  Games", SW / 2, 140, 2, TH.dim, TH.bg, MC_DATUM);
  tft.drawRoundRect(60, 170, 200, 10, 5, TH.dim);
  for (int i = 0; i <= 192; i += 6) { tft.fillRoundRect(64, 172, i, 6, 3, TH.accent); delay(6); }
}
void setup() {
  Serial.begin(115200);
  Serial.println("CYD HUB: alarm engine starting");
  loadCfg();
  tft.init();
  tft.setRotation(3);
  tft.invertDisplay(INVERT_COLORS);
  tft.fillScreen(TH.bg);
  hwPwmInit();
  touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  ts.begin(touchSPI);
  ts.setRotation(3);
  randomSeed(esp_random());

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(CONTROL_AP_SSID, CONTROL_AP_PASS);
  beginSavedWiFi();
  setupWebServer();
  splash();
  goScreen(SCR_SETUP);
}
void loop() {
  updateTouch();
  beepTick();
  wifiTick();
  timerBackground();
  alarmTick();
  weatherTick();
  server.handleClient();

  if (!alarmRinging && screen != SCR_HOME && tc.press && tc.x < 52 && tc.y < BAR + 2) {
    beep(1000, 20);
    goScreen(screen == SCR_GAME ? SCR_GAMES : SCR_HOME);
    return;
  }
  switch (screen) {
    case SCR_HOME:     homeLoop();     break;
    case SCR_CLOCK:    clockLoop();    break;
    case SCR_TIMER:    timerLoop();    break;
    case SCR_WEATHER:  weatherLoop();  break;
    case SCR_GAMES:    gamesLoop();    break;
    case SCR_SETTINGS: settingsLoop(); break;
    case SCR_SETUP:    setupLoop();    break;
    case SCR_GAME:     gameLoop();     break;
    case SCR_ISLAMIC:  islamicLoop();  break;
    case SCR_ALARM:    alarmLoop();    break;
    case SCR_KNOWLEDGE: knowledgeLoop(); break;
  }
  delay(1);
}

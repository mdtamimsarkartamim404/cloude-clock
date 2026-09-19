/*
 * ==========================================================
 *  CYD Smart Sky Clock  (ESP32-2432S028R, 2.8" ST7789)
 *  Clock + Weather + Catch Game + Tic-Tac-Toe
 *
 *  Libraries (Library Manager):
 *    - TFT_eSPI            (User_Setup.h রিপ্লেস করতে হবে)
 *    - XPT2046_Touchscreen (Paul Stoffregen)
 *    - ArduinoJson         (version 7.x)
 *  Board: ESP32 Dev Module
 * ==========================================================
 */
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>

// ================== এখানে নিজের তথ্য দিন ==================
const char* WIFI_SSID  = "Ho";
const char* WIFI_PASS  = "@#";
const char* OWM_KEY    = "";   // openweathermap.org থেকে ফ্রি key
const char* OWM_CITY   = "Rajshahi,BD";
const char* CITY_LABEL = "RAJSHAHI";
const long  GMT_OFFSET = 6 * 3600;                     // বাংলাদেশ UTC+6
// ==========================================================

// ---- Touch pins (CYD) ----
#define XPT_CS   33
#define XPT_IRQ  36
#define XPT_CLK  25
#define XPT_MISO 39
#define XPT_MOSI 32
// টাচ ক্যালিব্রেশন (দরকার হলে বদলান)
#define TS_MINX 200
#define TS_MAXX 3700
#define TS_MINY 240
#define TS_MAXY 3800

TFT_eSPI tft = TFT_eSPI();
SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(XPT_CS, XPT_IRQ);
Preferences prefs;

#define RGB(r,g,b) ((uint16_t)((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|((b)>>3)))

// ---------------- Theme ----------------
bool night = false;
uint16_t cBG, cCard, cDim, cText = TFT_WHITE;
const uint16_t cAccent = RGB(0, 220, 255);
const uint16_t cWarm   = RGB(255, 196, 60);
const uint16_t cGood   = RGB(80, 230, 140);
const uint16_t cBad    = RGB(255, 90, 90);
const uint16_t cRain   = RGB(90, 170, 255);

enum Page { P_CLOCK = 0, P_WEATHER, P_CATCH, P_XO };
int page = P_CLOCK;

// ---------------- Weather ----------------
struct Weather {
  bool valid = false;
  float temp = 0, feels = 0, wind = 0;
  int hum = 0, press = 0;
  String main = "", desc = "";
  bool nightIcon = false;
  char updated[8] = "--:--";
} W;
uint32_t lastWeatherTry = 0;
bool weatherOk = false;

// ---------------- Helpers ----------------
bool timeValid(struct tm &ti) {
  return getLocalTime(&ti, 0) && ti.tm_year > 120;
}
bool isNightNow() {
  struct tm ti;
  if (!timeValid(ti)) return false;
  return (ti.tm_hour < 6 || ti.tm_hour >= 18);
}
void updateTheme() {
  night = isNightNow();
  if (night) { cBG = RGB(8, 12, 34);  cCard = RGB(22, 32, 68);  cDim = RGB(140, 155, 195); }
  else       { cBG = RGB(24, 78, 150); cCard = RGB(38, 105, 180); cDim = RGB(190, 215, 245); }
}

// ---------------- Icons ----------------
void drawCloud(int cx, int cy, uint16_t col) {
  tft.fillCircle(cx - 10, cy + 3, 9, col);
  tft.fillCircle(cx + 2,  cy - 3, 12, col);
  tft.fillCircle(cx + 13, cy + 4, 8, col);
  tft.fillRoundRect(cx - 18, cy + 3, 38, 10, 5, col);
}
void drawSun(int cx, int cy, uint16_t bg) {
  for (int a = 0; a < 360; a += 45) {
    float r = a * 0.0174533f;
    tft.drawLine(cx + cosf(r) * 15, cy + sinf(r) * 15, cx + cosf(r) * 22, cy + sinf(r) * 22, cWarm);
    tft.drawLine(cx + cosf(r) * 15 + 1, cy + sinf(r) * 15, cx + cosf(r) * 22 + 1, cy + sinf(r) * 22, cWarm);
  }
  tft.fillCircle(cx, cy, 12, cWarm);
}
void drawMoon(int cx, int cy, uint16_t bg) {
  tft.fillCircle(cx, cy, 14, RGB(240, 240, 210));
  tft.fillCircle(cx + 7, cy - 4, 12, bg);
}
void drawWeatherIcon(int cx, int cy, const String &m, bool nightIcon, uint16_t bg) {
  uint16_t cloudCol = RGB(215, 225, 240);
  if (m == "Clear") {
    if (nightIcon) drawMoon(cx, cy, bg); else drawSun(cx, cy, bg);
  } else if (m == "Clouds") {
    drawCloud(cx, cy, cloudCol);
  } else if (m == "Rain" || m == "Drizzle") {
    drawCloud(cx, cy - 7, RGB(170, 185, 210));
    for (int i = -1; i <= 1; i++) {
      tft.drawLine(cx + i * 10, cy + 10, cx + i * 10 - 3, cy + 18, cRain);
      tft.drawLine(cx + i * 10 + 1, cy + 10, cx + i * 10 - 2, cy + 18, cRain);
    }
  } else if (m == "Thunderstorm") {
    drawCloud(cx, cy - 7, RGB(120, 130, 155));
    tft.fillTriangle(cx + 2, cy + 6, cx - 6, cy + 18, cx + 2, cy + 15, cWarm);
    tft.fillTriangle(cx + 2, cy + 15, cx + 10, cy + 6, cx, cy + 22, cWarm);
  } else if (m == "Snow") {
    drawCloud(cx, cy - 7, cloudCol);
    for (int i = -1; i <= 1; i++) tft.fillCircle(cx + i * 11, cy + 14, 2, TFT_WHITE);
  } else {  // Mist, Haze, Smoke...
    for (int i = 0; i < 4; i++) tft.fillRoundRect(cx - 20 + (i % 2) * 6, cy - 12 + i * 8, 34, 3, 1, cloudCol);
  }
}

// ---------------- Nav bar ----------------
void drawNav() {
  const char* L[4] = {"CLOCK", "WEATHER", "CATCH", "X-O"};
  tft.fillRect(0, 204, 320, 36, cBG);
  tft.setTextDatum(MC_DATUM);
  for (int i = 0; i < 4; i++) {
    int x = i * 80 + 4, y = 208, w = 72, h = 28;
    if (i == page) {
      tft.fillRoundRect(x, y, w, h, 8, cAccent);
      tft.setTextColor(RGB(5, 15, 40), cAccent);
    } else {
      tft.fillRoundRect(x, y, w, h, 8, cCard);
      tft.setTextColor(cDim, cCard);
    }
    tft.drawString(L[i], x + w / 2, y + h / 2, 2);
  }
}

void drawWifi(int x, int y) {
  int bars = 0;
  if (WiFi.status() == WL_CONNECTED) {
    int r = WiFi.RSSI();
    bars = r > -55 ? 4 : r > -65 ? 3 : r > -75 ? 2 : 1;
  }
  tft.fillRect(x, y - 14, 26, 16, cBG);
  for (int i = 0; i < 4; i++) {
    int h = 4 + i * 3;
    tft.fillRect(x + i * 6, y - h, 4, h, i < bars ? cAccent : cCard);
  }
}

// =====================================================
//                       CLOCK PAGE
// =====================================================
int lastMin = -1;

void drawWeatherCard() {
  tft.fillRoundRect(10, 128, 300, 70, 12, cCard);
  if (!W.valid) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(cDim, cCard);
    tft.drawString(WiFi.status() == WL_CONNECTED ? "Loading weather..." : "No WiFi", 160, 163, 4);
    return;
  }
  drawWeatherIcon(50, 163, W.main, W.nightIcon, cCard);
  char t[8]; sprintf(t, "%d", (int)roundf(W.temp));
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, cCard);
  int tw = tft.drawString(t, 96, 139, 6);
  tft.drawCircle(96 + tw + 6, 148, 3, TFT_WHITE);
  tft.drawString("C", 96 + tw + 12, 141, 4);

  String d = W.desc; if (d.length()) d[0] = toupper(d[0]);
  if (d.length() > 13) d = d.substring(0, 13);
  char b[24];
  tft.setTextPadding(108);
  tft.setTextColor(cWarm, cCard);
  tft.drawString(d, 200, 138, 2);
  tft.setTextColor(cText, cCard);
  sprintf(b, "Feels %dC", (int)roundf(W.feels));
  tft.drawString(b, 200, 160, 2);
  sprintf(b, "Humidity %d%%", W.hum);
  tft.drawString(b, 200, 178, 2);
  tft.setTextPadding(0);
}

void drawTime(bool force) {
  struct tm ti;
  if (!timeValid(ti)) return;
  int h12 = ti.tm_hour % 12; if (h12 == 0) h12 = 12;
  char hh[4], mm[4], ss[4];
  sprintf(hh, "%02d", h12); sprintf(mm, "%02d", ti.tm_min); sprintf(ss, "%02d", ti.tm_sec);

  int wH = tft.textWidth("88", 7), wC = tft.textWidth(":", 7), wS = tft.textWidth("88", 4);
  int total = 2 * wH + wC + 8 + wS;
  int x0 = (320 - total) / 2, y0 = 32;

  tft.setTextDatum(TL_DATUM);
  if (force || ti.tm_min != lastMin) {
    lastMin = ti.tm_min;
    tft.setTextColor(TFT_WHITE, cBG);
    tft.drawString(hh, x0, y0, 7);
    tft.drawString(mm, x0 + wH + wC, y0, 7);
    tft.setTextColor(cWarm, cBG);
    tft.drawString(ti.tm_hour >= 12 ? "PM" : "AM", x0 + total - wS, y0 + 2, 2);
    char d[32]; strftime(d, sizeof(d), "%A, %d %b %Y", &ti);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(cDim, cBG);
    tft.setTextPadding(320);
    tft.drawString(d, 160, 92, 4);
    tft.setTextPadding(0);
    tft.setTextDatum(TL_DATUM);
  }
  // blinking colon
  tft.setTextColor((ti.tm_sec % 2) ? TFT_WHITE : cBG, cBG);
  tft.drawString(":", x0 + wH, y0, 7);
  // seconds
  tft.setTextColor(cAccent, cBG);
  tft.drawString(ss, x0 + total - wS, y0 + 48 - 26, 4);
}

void drawClockPage() {
  updateTheme();
  tft.fillScreen(cBG);
  // decoration
  if (night) {
    for (int i = 0; i < 26; i++) {
      int x = (i * 73 + 11) % 60; if (i % 2) x = 320 - x;
      int y = 30 + (i * 37) % 90;
      tft.fillCircle(x, y, (i % 5 == 0) ? 2 : 1, RGB(220, 225, 255));
    }
  } else {
    drawCloud(34, 70, RGB(210, 228, 250));
    drawCloud(288, 108, RGB(200, 222, 248));
  }
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(cDim, cBG);
  tft.drawString(CITY_LABEL, 10, 6, 2);
  drawWifi(286, 20);
  drawWeatherCard();
  lastMin = -1;
  drawTime(true);
  drawNav();
}

// =====================================================
//                      WEATHER PAGE
// =====================================================
void drawWeatherPage() {
  updateTheme();
  tft.fillScreen(cBG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, cBG);
  tft.drawString(CITY_LABEL, 10, 6, 2);
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(cDim, cBG);
  char u[24]; sprintf(u, "Updated %s", W.updated);
  tft.drawString(u, 310, 6, 2);

  tft.fillRoundRect(10, 34, 140, 164, 12, cCard);
  if (!W.valid) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(cDim, cCard);
    tft.drawString("No data", 80, 116, 4);
    drawNav();
    return;
  }
  drawWeatherIcon(80, 70, W.main, W.nightIcon, cCard);
  char t[8]; sprintf(t, "%d", (int)roundf(W.temp));
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_WHITE, cCard);
  int tw = tft.textWidth(t, 6);
  tft.drawString(t, 66, 106, 6);
  tft.drawCircle(66 + tw / 2 + 8, 114, 3, TFT_WHITE);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("C", 66 + tw / 2 + 14, 108, 4);
  String d = W.desc; if (d.length()) d[0] = toupper(d[0]);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(cWarm, cCard);
  if (d.length() > 16) d = d.substring(0, 16);
  tft.drawString(d, 80, 166, 2);

  const char* lab[4] = {"Feels like", "Humidity", "Wind", "Pressure"};
  char v[4][16];
  sprintf(v[0], "%d C", (int)roundf(W.feels));
  sprintf(v[1], "%d %%", W.hum);
  sprintf(v[2], "%.1f m/s", W.wind);
  sprintf(v[3], "%d hPa", W.press);
  for (int i = 0; i < 4; i++) {
    int y = 34 + i * 41;
    tft.fillRoundRect(160, y, 150, 37, 10, cCard);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(cDim, cCard);
    tft.drawString(lab[i], 170, y + 4, 2);
    tft.setTextColor(TFT_WHITE, cCard);
    tft.drawString(v[i], 170, y + 17, 2);
  }
  drawNav();
}

// =====================================================
//                       CATCH GAME
// =====================================================
struct Item { int x; float y; float vy; uint8_t k; };
const int NITEM = 5, PLAY_TOP = 32, PLAY_BOT = 200, IR = 22;
Item it[NITEM];
int cScore = 0, cBest = 0, cShownScore = -1, cShownTime = -1, cBarW = -1;
uint8_t cState = 0;   // 0 idle, 1 playing, 2 over
uint32_t cEnd = 0, cLast = 0, cStart = 0;
const int GAME_MS = 25000;

bool rainyTheme() { return W.main == "Rain" || W.main == "Drizzle" || W.main == "Thunderstorm"; }

void spawnItem(int i, bool spread) {
  it[i].x = random(IR + 4, 320 - IR - 4);
  it[i].y = PLAY_TOP + IR + (spread ? random(0, 100) : 0);
  float boost = (millis() - cStart) / 25000.0f;              // ক্রমশ দ্রুত
  it[i].vy = random(15, 33) / 10.0f + boost * 1.5f;
  int r = random(100);
  it[i].k = r < 68 ? 0 : (r < 84 ? 1 : 2);
}
void eraseItem(int i) {
  tft.fillRect(it[i].x - IR, (int)it[i].y - IR, IR * 2 + 1, IR * 2 + 1, cBG);
}
void drawItem(int i) {
  int x = it[i].x, y = (int)it[i].y;
  if (it[i].k == 0) {
    if (rainyTheme()) {
      tft.fillCircle(x, y + 4, 9, cRain);
      tft.fillTriangle(x, y - 14, x - 8, y + 2, x + 8, y + 2, cRain);
      tft.fillCircle(x - 3, y + 2, 2, TFT_WHITE);
    } else {
      drawSun(x, y, cBG);
    }
  } else if (it[i].k == 1) {
    tft.fillTriangle(x, y - 16, x - 14, y + 9, x + 14, y + 9, cWarm);
    tft.fillTriangle(x, y + 16, x - 14, y - 6, x + 14, y - 6, cWarm);
  } else {
    drawCloud(x, y, RGB(115, 120, 140));
    tft.drawLine(x - 6, y - 4, x + 6, y + 8, cBad);
    tft.drawLine(x + 6, y - 4, x - 6, y + 8, cBad);
  }
}
void drawCatchHeader(bool force) {
  int left = max(0, (int)((cEnd - millis()) / 1000) + 1);
  if (cState != 1) left = GAME_MS / 1000;
  if (force || cShownScore != cScore || cShownTime != left) {
    cShownScore = cScore; cShownTime = left;
    char b[24];
    tft.setTextPadding(96);
    tft.setTextDatum(TL_DATUM); tft.setTextColor(TFT_WHITE, cBG);
    sprintf(b, "SCORE %d", cScore); tft.drawString(b, 8, 6, 2);
    tft.setTextDatum(TC_DATUM); tft.setTextColor(cWarm, cBG);
    sprintf(b, "BEST %d", cBest); tft.drawString(b, 160, 6, 2);
    tft.setTextDatum(TR_DATUM); tft.setTextColor(cAccent, cBG);
    sprintf(b, "TIME %d", left); tft.drawString(b, 312, 6, 2);
    tft.setTextPadding(0);
  }
}
void drawCatchBar() {
  int w = (cState == 1) ? (int)(320.0f * (cEnd - millis()) / GAME_MS) : 320;
  w = constrain(w, 0, 320);
  if (w != cBarW) {
    cBarW = w;
    tft.fillRect(0, 28, 320, 3, cCard);
    tft.fillRect(0, 28, w, 3, cAccent);
  }
}
void drawOverlay(const char* title, const char* l1, const char* l2) {
  tft.fillRoundRect(50, 62, 220, 108, 14, cCard);
  tft.drawRoundRect(50, 62, 220, 108, 14, cAccent);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(cWarm, cCard);  tft.drawString(title, 160, 88, 4);
  tft.setTextColor(TFT_WHITE, cCard); tft.drawString(l1, 160, 122, 2);
  tft.setTextColor(cDim, cCard);   tft.drawString(l2, 160, 146, 2);
}
void drawCatchPage() {
  updateTheme();
  tft.fillScreen(cBG);
  cState = 0; cScore = 0; cBarW = -1;
  prefs.begin("cyd", true); cBest = prefs.getInt("best", 0); prefs.end();
  drawCatchHeader(true);
  drawCatchBar();
  drawOverlay("CATCH!", rainyTheme() ? "Tap the raindrops" : "Tap the suns",
              "Star +5  |  Dark cloud -3");
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(cGood, cCard);
  tft.drawString("Tap to start", 160, 160, 2);
  drawNav();
}
void catchStart() {
  cState = 1; cScore = 0; cStart = millis(); cEnd = cStart + GAME_MS; cLast = 0; cBarW = -1;
  tft.fillRect(0, PLAY_TOP, 320, PLAY_BOT - PLAY_TOP, cBG);
  for (int i = 0; i < NITEM; i++) spawnItem(i, true);
  drawCatchHeader(true);
}
void catchEnd() {
  cState = 2;
  bool rec = cScore > cBest;
  if (rec) { cBest = cScore; prefs.begin("cyd", false); prefs.putInt("best", cBest); prefs.end(); }
  tft.fillRect(0, PLAY_TOP, 320, PLAY_BOT - PLAY_TOP, cBG);
  char b[32]; sprintf(b, "Score: %d", cScore);
  drawOverlay("GAME OVER", b, rec ? "New record!" : "Nice try!");
  tft.setTextDatum(MC_DATUM); tft.setTextColor(cGood, cCard);
  tft.drawString("Tap to play again", 160, 160, 2);
  drawCatchHeader(true);
  drawCatchBar();
}
void catchTap(int x, int y) {
  if (cState != 1) { catchStart(); return; }
  for (int i = 0; i < NITEM; i++) {
    int dx = x - it[i].x, dy = y - (int)it[i].y;
    if (dx * dx + dy * dy < 32 * 32) {
      int pts = it[i].k == 0 ? 1 : (it[i].k == 1 ? 5 : -3);
      cScore = max(0, cScore + pts);
      eraseItem(i);
      uint16_t col = pts > 0 ? cGood : cBad;
      tft.drawCircle(it[i].x, (int)it[i].y, 14, col);
      tft.drawCircle(it[i].x, (int)it[i].y, 20, col);
      char b[8]; sprintf(b, pts > 0 ? "+%d" : "%d", pts);
      tft.setTextDatum(MC_DATUM); tft.setTextColor(col, cBG);
      tft.drawString(b, it[i].x, (int)it[i].y, 4);
      delay(70);
      eraseItem(i);
      spawnItem(i, false);
      drawCatchHeader(false);
      break;
    }
  }
}
void catchLoop() {
  if (cState != 1) return;
  if (millis() >= cEnd) { catchEnd(); return; }
  if (millis() - cLast < 30) return;
  cLast = millis();
  for (int i = 0; i < NITEM; i++) {
    eraseItem(i);
    it[i].y += it[i].vy;
    if (it[i].y > PLAY_BOT - IR) spawnItem(i, false);
    drawItem(i);
  }
  drawCatchHeader(false);
  drawCatchBar();
}

// =====================================================
//                     TIC-TAC-TOE
// =====================================================
const int BX = 14, BY = 34, CS = 54;
const uint8_t L8[8][3] = {{0,1,2},{3,4,5},{6,7,8},{0,3,6},{1,4,7},{2,5,8},{0,4,8},{2,4,6}};
int xb[9], xTurn = 1, xWin = 0, xLineIdx = -1, xScore[3] = {0, 0, 0};
uint32_t aiAt = 0;

void drawXoPanel() {
  tft.fillRoundRect(190, 34, 122, 162, 12, cCard);
  char b[24];
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(cAccent, cCard); sprintf(b, "You (X)   %d", xScore[0]); tft.drawString(b, 200, 44, 2);
  tft.setTextColor(cWarm, cCard);   sprintf(b, "CYD (O)   %d", xScore[1]); tft.drawString(b, 200, 66, 2);
  tft.setTextColor(cDim, cCard);    sprintf(b, "Draws     %d", xScore[2]); tft.drawString(b, 200, 88, 2);
  tft.drawFastHLine(200, 112, 102, cDim);
  const char* s; uint16_t col = TFT_WHITE;
  if (xWin == 1) { s = "You win!"; col = cGood; }
  else if (xWin == 2) { s = "CYD wins"; col = cBad; }
  else if (xWin == 3) { s = "Draw!"; col = cWarm; }
  else if (xTurn == 1) s = "Your turn";
  else s = "Thinking";
  tft.setTextDatum(TC_DATUM); tft.setTextColor(col, cCard);
  tft.drawString(s, 251, 128, 4);
  if (xWin) { tft.setTextColor(cDim, cCard); tft.drawString("Tap to restart", 251, 170, 2); }
}
void drawMark(int i) {
  int cx = BX + (i % 3) * CS + CS / 2, cy = BY + (i / 3) * CS + CS / 2;
  if (xb[i] == 1) {
    for (int o = -1; o <= 1; o++) {
      tft.drawLine(cx - 15 + o, cy - 15, cx + 15 + o, cy + 15, cAccent);
      tft.drawLine(cx - 15 + o, cy + 15, cx + 15 + o, cy - 15, cAccent);
    }
  } else if (xb[i] == 2) {
    tft.fillCircle(cx, cy, 17, cWarm);
    tft.fillCircle(cx, cy, 11, cCard);
  }
}
void drawXoBoard() {
  tft.fillRoundRect(BX - 4, BY - 4, CS * 3 + 8, CS * 3 + 8, 12, cCard);
  for (int i = 1; i < 3; i++) {
    tft.drawFastVLine(BX + i * CS, BY + 6, CS * 3 - 12, cDim);
    tft.drawFastHLine(BX + 6, BY + i * CS, CS * 3 - 12, cDim);
  }
  for (int i = 0; i < 9; i++) if (xb[i]) drawMark(i);
}
int checkWin() {
  for (int l = 0; l < 8; l++) {
    int a = xb[L8[l][0]];
    if (a && a == xb[L8[l][1]] && a == xb[L8[l][2]]) { xLineIdx = l; return a; }
  }
  for (int i = 0; i < 9; i++) if (!xb[i]) return 0;
  return 3;
}
void xoReset() {
  for (int i = 0; i < 9; i++) xb[i] = 0;
  xTurn = 1; xWin = 0; xLineIdx = -1;
  drawXoBoard(); drawXoPanel();
}
void xoPlace(int i, int who) {
  xb[i] = who; drawMark(i);
  int w = checkWin();
  if (w) {
    xWin = w; xScore[w == 1 ? 0 : (w == 2 ? 1 : 2)]++;
    if (w != 3) {
      int a = L8[xLineIdx][0], c = L8[xLineIdx][2];
      int x1 = BX + (a % 3) * CS + CS / 2, y1 = BY + (a / 3) * CS + CS / 2;
      int x2 = BX + (c % 3) * CS + CS / 2, y2 = BY + (c / 3) * CS + CS / 2;
      for (int o = -1; o <= 1; o++) tft.drawLine(x1 + o, y1, x2 + o, y2, cGood);
      for (int o = -1; o <= 1; o++) tft.drawLine(x1, y1 + o, x2, y2 + o, cGood);
    }
  } else {
    xTurn = 3 - who;
    if (xTurn == 2) aiAt = millis() + 550;
  }
  drawXoPanel();
}
int findWinMove(int p) {
  for (int l = 0; l < 8; l++) {
    int cnt = 0, empty = -1;
    for (int k = 0; k < 3; k++) {
      int v = xb[L8[l][k]];
      if (v == p) cnt++; else if (v == 0) empty = L8[l][k];
    }
    if (cnt == 2 && empty >= 0) return empty;
  }
  return -1;
}
int pickMove() {
  if (random(100) < 15) {                       // মাঝে মাঝে ভুল করবে
    int e[9], n = 0; for (int i = 0; i < 9; i++) if (!xb[i]) e[n++] = i;
    return e[random(n)];
  }
  int m = findWinMove(2); if (m >= 0) return m;
  m = findWinMove(1);     if (m >= 0) return m;
  if (!xb[4]) return 4;
  int c[4] = {0, 2, 6, 8}, cn[4], n = 0;
  for (int i = 0; i < 4; i++) if (!xb[c[i]]) cn[n++] = c[i];
  if (n) return cn[random(n)];
  int s[4] = {1, 3, 5, 7}, sn[4]; n = 0;
  for (int i = 0; i < 4; i++) if (!xb[s[i]]) sn[n++] = s[i];
  return sn[random(n)];
}
void drawXoPage() {
  updateTheme();
  tft.fillScreen(cBG);
  tft.setTextDatum(TL_DATUM); tft.setTextColor(TFT_WHITE, cBG);
  tft.drawString("TIC-TAC-TOE", 10, 6, 2);
  xoReset();
  drawNav();
}
void xoTap(int x, int y) {
  if (xWin) { xoReset(); return; }
  if (xTurn != 1) return;
  if (x < BX || y < BY || x >= BX + CS * 3 || y >= BY + CS * 3) return;
  int idx = ((y - BY) / CS) * 3 + (x - BX) / CS;
  if (!xb[idx]) xoPlace(idx, 1);
}
void xoLoop() {
  if (!xWin && xTurn == 2 && millis() >= aiAt) xoPlace(pickMove(), 2);
}

// =====================================================
//                 WiFi / NTP / Weather fetch
// =====================================================
void splash(const char* msg, int prog) {
  tft.fillScreen(RGB(8, 12, 34));
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(cAccent, RGB(8, 12, 34));
  tft.drawString("CYD Sky Clock", 160, 90, 4);
  tft.setTextColor(RGB(150, 165, 200), RGB(8, 12, 34));
  tft.drawString(msg, 160, 130, 2);
  tft.drawRoundRect(60, 160, 200, 10, 5, RGB(150, 165, 200));
  tft.fillRoundRect(62, 162, constrain(prog, 0, 100) * 196 / 100, 6, 3, cAccent);
}

bool fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return false;
  HTTPClient http;
  String url = String("http://api.openweathermap.org/data/2.5/weather?q=") + OWM_CITY +
               "&appid=" + OWM_KEY + "&units=metric";
  http.begin(url);
  http.setTimeout(8000);
  int code = http.GET();
  bool ok = false;
  if (code == 200) {
    JsonDocument doc;
    if (!deserializeJson(doc, http.getString())) {
      W.temp  = doc["main"]["temp"] | 0.0f;
      W.feels = doc["main"]["feels_like"] | 0.0f;
      W.hum   = doc["main"]["humidity"] | 0;
      W.press = doc["main"]["pressure"] | 0;
      W.wind  = doc["wind"]["speed"] | 0.0f;
      W.main  = doc["weather"][0]["main"].as<String>();
      W.desc  = doc["weather"][0]["description"].as<String>();
      String ic = doc["weather"][0]["icon"].as<String>();
      W.nightIcon = ic.endsWith("n");
      struct tm ti;
      if (timeValid(ti)) sprintf(W.updated, "%02d:%02d", ti.tm_hour, ti.tm_min);
      W.valid = true; ok = true;
    }
  }
  http.end();
  return ok;
}

// =====================================================
//                       PAGES / SETUP / LOOP
// =====================================================
void drawPage() {
  switch (page) {
    case P_CLOCK:   drawClockPage();   break;
    case P_WEATHER: drawWeatherPage(); break;
    case P_CATCH:   drawCatchPage();   break;
    case P_XO:      drawXoPage();      break;
  }
}

bool readTouch(int &x, int &y) {
  if (!ts.tirqTouched() || !ts.touched()) return false;
  TS_Point p = ts.getPoint();
  x = constrain(map(p.x, TS_MINX, TS_MAXX, 0, 320), 0, 319);
  y = constrain(map(p.y, TS_MINY, TS_MAXY, 0, 240), 0, 239);
  return true;
}

void setup() {
  Serial.begin(115200);
  pinMode(21, OUTPUT); digitalWrite(21, HIGH);

  tft.init();
  tft.setRotation(1);            // উল্টা দেখালে 3 দিন
  touchSPI.begin(XPT_CLK, XPT_MISO, XPT_MOSI, XPT_CS);
  ts.begin(touchSPI);
  ts.setRotation(1);             // টাচ উল্টা হলে 3 দিন
  randomSeed(esp_random());

  updateTheme();
  splash("Connecting WiFi...", 10);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  for (int i = 0; i < 30 && WiFi.status() != WL_CONNECTED; i++) {
    delay(500);
    splash("Connecting WiFi...", 10 + i * 2);
  }
  if (WiFi.status() == WL_CONNECTED) {
    splash("Syncing time...", 70);
    configTime(GMT_OFFSET, 0, "pool.ntp.org", "time.google.com");
    struct tm ti;
    for (int i = 0; i < 16 && !timeValid(ti); i++) delay(500);
    splash("Getting weather...", 85);
    weatherOk = fetchWeather();
  } else {
    splash("WiFi failed - offline mode", 100);
    delay(1200);
  }
  lastWeatherTry = millis();
  drawPage();
}

void loop() {
  // ---- Touch ----
  static uint32_t lastSeen = 0;
  int tx, ty;
  bool down = readTouch(tx, ty), tap = false;
  if (down) { if (millis() - lastSeen > 120) tap = true; lastSeen = millis(); }
  if (tap) {
    if (ty >= 204) {
      int idx = constrain(tx / 80, 0, 3);
      if (idx != page) { page = idx; drawPage(); }
    } else if (page == P_CATCH) catchTap(tx, ty);
    else if (page == P_XO)      xoTap(tx, ty);
  }

  // ---- Clock tick ----
  static int lastSec = -1;
  struct tm ti;
  if (timeValid(ti) && ti.tm_sec != lastSec) {
    lastSec = ti.tm_sec;
    if (page == P_CLOCK) drawTime(false);
    bool nn = (ti.tm_hour < 6 || ti.tm_hour >= 18);
    if (nn != night && (page == P_CLOCK || page == P_WEATHER)) drawPage();  // দিন-রাত থিম বদল
    if (page == P_CLOCK && ti.tm_sec % 20 == 0) drawWifi(286, 20);
  }

  // ---- Weather refresh (10 মিনিট, ব্যর্থ হলে 1 মিনিট) ----
  uint32_t interval = weatherOk ? 600000UL : 60000UL;
  if (millis() - lastWeatherTry > interval && page != P_CATCH && page != P_XO) {
    lastWeatherTry = millis();
    weatherOk = fetchWeather();
    if (page == P_CLOCK) drawWeatherCard();
    else if (page == P_WEATHER) drawWeatherPage();
  }

  // ---- Games ----
  if (page == P_CATCH) catchLoop();
  if (page == P_XO)    xoLoop();

  delay(5);
}

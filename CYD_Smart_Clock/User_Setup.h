// =====================================================
//  TFT_eSPI  User_Setup.h  ->  ESP32-2432S028 (CYD) 2.8"  ST7789
//  এই ফাইলটা দিয়ে  Arduino/libraries/TFT_eSPI/User_Setup.h  রিপ্লেস করুন
// =====================================================
#define USER_SETUP_ID 202

#define ST7789_DRIVER

#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// ST7789 CYD-তে সাধারণত এটা লাগে। কালার উল্টা (negative) দেখালে এই লাইন কমেন্ট করুন
#define TFT_INVERSION_ON
// লাল-নীল অদলবদল হলে TFT_BGR <-> TFT_RGB করুন
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
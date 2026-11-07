/*
  Dal demo — Arabic text on the ELEGOO ESP32 2.8" screen (LovyanGFX).
  Tap the screen to change page.
*/
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <Dal.h>
#include <fonts/DalNoto18.h>
#include <fonts/DalNoto26.h>

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341 _panel;
  lgfx::Bus_SPI       _bus;
  lgfx::Light_PWM     _light;
  lgfx::Touch_XPT2046 _touch;
public:
  LGFX() {
    { auto cfg = _bus.config();
      cfg.spi_host   = SPI2_HOST;
      cfg.spi_mode   = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read  = 16000000;
      cfg.spi_3wire  = false;
      cfg.use_lock   = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = 14; cfg.pin_mosi = 13; cfg.pin_miso = 12; cfg.pin_dc = 2;
      _bus.config(cfg); _panel.setBus(&_bus); }

    { auto cfg = _panel.config();
      cfg.pin_cs = 15; cfg.pin_rst = -1; cfg.pin_busy = -1;
      cfg.panel_width = 240; cfg.panel_height = 320;
      cfg.offset_x = 0; cfg.offset_y = 0; cfg.offset_rotation = 0;
      cfg.readable   = true;
      cfg.invert     = false;
      cfg.rgb_order  = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      _panel.config(cfg); }

    { auto cfg = _light.config();
      cfg.pin_bl = 21; cfg.invert = false; cfg.freq = 44100; cfg.pwm_channel = 7;
      _light.config(cfg); _panel.setLight(&_light); }

    { auto cfg = _touch.config();
      cfg.x_min = 200; cfg.x_max = 3900; cfg.y_min = 200; cfg.y_max = 3900;
      cfg.pin_int = 36; cfg.bus_shared = false; cfg.offset_rotation = 0;
      cfg.spi_host = -1;
      cfg.freq = 1000000;
      cfg.pin_sclk = 25; cfg.pin_mosi = 32; cfg.pin_miso = 39; cfg.pin_cs = 33;
      _touch.config(cfg); _panel.setTouch(&_touch); }

    setPanel(&_panel);
  }
};

LGFX tft;
LGFX_Sprite spr(&tft);
LovyanGFX* g = &tft;

static constexpr uint32_t BG = 0x0B0E14u, INK = 0xF2EBDDu, TITLE = 0x9FB4D9u, DIM = 0x8A93A6u;

DalText dal;                        // keep it global: it holds a working buffer (~14 KB)

const char* PAGES[][3] = {
  { "هايكو",
    "صمتُ الغرفةِ\nيُتَرجِمُ ضوءَ النافذةِ\nإلى ظلٍّ طويل",
    "قصيدة تكتبها الشاشة بنفسها، بحروف متصلة وحركات" },
  { "نص مختلط",
    "رحلة KAC788 من DXB إلى الكويت على ارتفاع 3500 قدم، السرعة 420 كم/س (تهبط الآن)",
    "الأرقام والكلمات الإنجليزية تبقى من اليسار لليمين داخل السطر العربي" },
  { "فقرة مضبوطة",
    "في الترجمة لا ننقل الكلمات وحدها، بل ننقل المسافة بينها، والصمت الذي يسبقها، والإيقاع الذي يجعلها تُقرأ كأنها كُتبت أول مرة بهذه اللغة.",
    "تقسيم الأسطر وضبط الفقرة على العرض كله" },
  { "لام ألف وأشكال الحروف",
    "لا لأ لإ لآ\nبـ ـبـ ـب ب\nهـ ـهـ ـه ه\nعـ ـعـ ـع ع",
    "كل حرف بأشكاله الأربعة حسب موقعه" },
};
const int PAGE_N = sizeof(PAGES) / sizeof(PAGES[0]);
int page = 0;

void render() {                       // drawn straight to the screen in full colour (no sprite needed)
  tft.startWrite();
  g->fillScreen(BG);
  dal.setFont(&DalNoto18);
  dalDraw(g, dal, PAGES[page][0], 12, 6, 296, DAL_RIGHT, TITLE, BG);
  dal.setFont(page == 1 || page == 2 ? &DalNoto18 : &DalNoto26);   // long texts in the smaller size
  dalDraw(g, dal, PAGES[page][1], 12, 44, 296, page == 2 ? DAL_JUSTIFY : page == 0 ? DAL_CENTER : DAL_RIGHT, INK, BG);
  dal.setFont(&DalNoto18);
  int h = dal.measure(PAGES[page][2], 296);
  dalDraw(g, dal, PAGES[page][2], 12, 236 - h, 296, DAL_RIGHT, DIM, BG);
  tft.endWrite();
}

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);
  tft.setBrightness(255);
  render();
}

void loop() {
  static bool was = false;
  int32_t x, y;
  bool t = tft.getTouch(&x, &y);
  if (t && !was) { page = (page + 1) % PAGE_N; render(); }
  was = t;
  delay(20);
}

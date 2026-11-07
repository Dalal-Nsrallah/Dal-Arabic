/*
  Dal (دال) — Arabic text for small screens.

  Draws Arabic correctly on microcontroller displays: connected letters (all four joining
  forms), lam-alef ligatures, harakat, right-to-left layout with numbers and Latin words kept
  left-to-right inside it, word wrapping, and right / left / centre alignment.
  Glyphs are anti-aliased bitmaps generated from a real Arabic font (see tools/make_font.py).

  Works with any graphics library that has drawPixel(x, y, colour) and color565(r, g, b)
  (LovyanGFX, TFT_eSPI, Adafruit GFX, ...) through dalDraw(), or with your own pixel callback.

  Author: Dalal Nasrallah. License: MIT.
*/
#pragma once
#include <stdint.h>
#include <stddef.h>

#if defined(ARDUINO) && !defined(ESP32)
  #include <avr/pgmspace.h>
  #define DAL_PROGMEM PROGMEM
#else
  #define DAL_PROGMEM
#endif

struct DalGlyph {
  uint32_t key;      // (codepoint << 2) | form   form: 0 isolated, 1 initial, 2 medial, 3 final
  uint32_t offset;   // into the font's bitmap (4 bits per pixel, 2 pixels per byte)
  uint8_t  w, h;
  int8_t   xoff, yoff;   // bitmap position from the pen (left edge) and the baseline
  uint8_t  adv;          // advance width
  uint8_t  flags;        // bit 0: combining mark (positioned over the previous letter)
};

struct DalFont {
  const DalGlyph* glyphs;
  uint16_t count;
  const uint8_t* bits;
  uint8_t ascent, descent, lineHeight, spaceAdv;
};

enum DalAlign { DAL_RIGHT, DAL_LEFT, DAL_CENTER, DAL_JUSTIFY };

// Called for every pixel of every glyph; alpha 1..15 (coverage)
typedef void (*DalPixelFn)(void* ctx, int x, int y, uint8_t alpha);

class DalText {
public:
  static const int MAX_CHARS = 400;      // per call

  explicit DalText(const DalFont* font = nullptr) : _font(font) {}
  void setFont(const DalFont* font) { _font = font; }
  const DalFont* font() const { return _font; }
  void setLineGap(int px) { _gap = px; }

  // Lays out utf8 inside a box `width` pixels wide whose top-left corner is (x, y)
  // and draws it through fn. Returns the height used.
  int draw(const char* utf8, int x, int y, int width, DalAlign align, DalPixelFn fn, void* ctx);

  // Height the text would take in a box `width` wide (draws nothing)
  int measure(const char* utf8, int width) { return draw(utf8, 0, 0, width, DAL_RIGHT, nullptr, nullptr); }

  // Width of the text on a single line
  int width(const char* utf8);

private:
  struct Item {
    const DalGlyph* g;        // base glyph (nullptr for a space)
    const DalGlyph* mark[2];  // up to two harakat on it
    uint32_t cp;
    uint8_t cls;               // 0 space, 1 RTL, 2 LTR, 3 number, 4 neutral
    uint8_t level;
    int16_t adv;
  };
  const DalFont* _font;
  int _gap = 0;
  Item _items[MAX_CHARS];
  int _n = 0;
  uint32_t _cps[MAX_CHARS];
  int _ncp = 0;
  int16_t _order[MAX_CHARS];

  const DalGlyph* find(uint32_t cp, uint8_t form) const;
  void decode(const char* s);
  void shape();
  void resolve(bool rtlPara);
  int lineWidth(int from, int to) const;
  void drawLine(int from, int to, int x, int baseline, int width, DalAlign align, bool last,
                bool rtlPara, DalPixelFn fn, void* ctx);
  void drawGlyph(const DalGlyph* g, int penX, int baseline, DalPixelFn fn, void* ctx) const;
};

// ---- convenience for graphics libraries (LovyanGFX, TFT_eSPI, Adafruit GFX ...) ----
// Draws in colour fg over a known background colour bg (both 0xRRGGBB); edges are blended.
template <class GFX>
struct DalTarget { GFX* g; uint32_t fg, bg; };

template <class GFX>
static void dalPixel(void* ctx, int x, int y, uint8_t a) {
  DalTarget<GFX>* t = (DalTarget<GFX>*)ctx;
  uint32_t fr = (t->fg >> 16) & 255, fgc = (t->fg >> 8) & 255, fb = t->fg & 255;
  uint32_t br = (t->bg >> 16) & 255, bgc = (t->bg >> 8) & 255, bb = t->bg & 255;
  uint8_t r = (fr * a + br * (15 - a)) / 15, g = (fgc * a + bgc * (15 - a)) / 15, b = (fb * a + bb * (15 - a)) / 15;
  t->g->drawPixel(x, y, t->g->color565(r, g, b));
}

template <class GFX>
int dalDraw(GFX* gfx, DalText& text, const char* utf8, int x, int y, int width, DalAlign align,
             uint32_t fg, uint32_t bg) {
  DalTarget<GFX> t = { gfx, fg, bg };
  return text.draw(utf8, x, y, width, align, dalPixel<GFX>, &t);
}

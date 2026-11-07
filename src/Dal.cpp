// Dal (دال) — Arabic text for small screens. See Dal.h.
#include "Dal.h"
#include <string.h>

// ---------------------------------------------------------------------------------------
// Character classes
// ---------------------------------------------------------------------------------------
static bool isMark(uint32_t c) { return (c >= 0x064B && c <= 0x0652) || c == 0x0670; }
static bool isAboveMark(uint32_t c) { return isMark(c) && c != 0x0650 && c != 0x064D; }

static bool isArabicLetter(uint32_t c) {
  return (c >= 0x0621 && c <= 0x063A) || (c >= 0x0640 && c <= 0x064A) || c == 0x0671 || c == 0x067E ||
         c == 0x0686 || c == 0x0698 || c == 0x06A4 || c == 0x06A9 || c == 0x06AF || c == 0x06CC;
}

// Joining type: 'D' joins both sides, 'R' joins only the letter before it, 'U' joins nothing
static char joining(uint32_t c) {
  if (c == 0x200D) return 'D';                     // zero width joiner: joins on both sides
  if (!isArabicLetter(c)) return 'U';              // includes zero width non-joiner (U+200C)
  switch (c) {
    case 0x0621: return 'U';
    case 0x0622: case 0x0623: case 0x0624: case 0x0625: case 0x0627: case 0x0629:
    case 0x062F: case 0x0630: case 0x0631: case 0x0632: case 0x0648: case 0x0671: case 0x0698:
      return 'R';
  }
  return 'D';
}

static bool isDigit(uint32_t c) { return (c >= '0' && c <= '9') || (c >= 0x0660 && c <= 0x0669); }
static bool isLatin(uint32_t c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }

static uint32_t mirror(uint32_t c) {
  switch (c) {
    case '(': return ')'; case ')': return '(';
    case '[': return ']'; case ']': return '[';
    case '{': return '}'; case '}': return '{';
    case '<': return '>'; case '>': return '<';
    case 0x00AB: return 0x00BB; case 0x00BB: return 0x00AB;
  }
  return c;
}

enum { C_SPACE = 0, C_RTL = 1, C_LTR = 2, C_NUM = 3, C_NEUTRAL = 4, C_NEWLINE = 5 };

// ---------------------------------------------------------------------------------------
const DalGlyph* DalText::find(uint32_t cp, uint8_t form) const {
  uint32_t key = (cp << 2) | form;
  int lo = 0, hi = _font->count - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    uint32_t k = _font->glyphs[mid].key;
    if (k == key) return &_font->glyphs[mid];
    if (k < key) lo = mid + 1; else hi = mid - 1;
  }
  return nullptr;
}

void DalText::decode(const char* s) {
  _ncp = 0;
  const uint8_t* p = (const uint8_t*)s;
  while (*p && _ncp < MAX_CHARS) {
    uint32_t c = *p++;
    if (c >= 0xF0)      { c = (c & 0x07) << 18; for (int k = 12; k >= 0; k -= 6) if (*p) c |= (uint32_t)(*p++ & 0x3F) << k; }
    else if (c >= 0xE0) { c = (c & 0x0F) << 12; for (int k = 6; k >= 0; k -= 6)  if (*p) c |= (uint32_t)(*p++ & 0x3F) << k; }
    else if (c >= 0xC0) { c = (c & 0x1F) << 6;  if (*p) c |= (*p++ & 0x3F); }
    if (c == '\r' || c == 0x200F || c == 0x200E) continue;   // ZWNJ / ZWJ are kept: they steer joining
    if (c == '\t') c = ' ';
    _cps[_ncp++] = c;
  }
}

// Turns characters into glyphs: chooses each Arabic letter's joining form,
// builds lam-alef ligatures and attaches harakat to their letter.
void DalText::shape() {
  _n = 0;
  for (int i = 0; i < _ncp && _n < MAX_CHARS; i++) {
    uint32_t c = _cps[i];
    if (c == 0x200C || c == 0x200D) continue;          // invisible: only affects the neighbours' forms
    if (isMark(c)) {                                   // haraka: belongs to the previous letter
      if (_n > 0 && _items[_n - 1].g) {
        const DalGlyph* m = find(c, 0);
        Item& it = _items[_n - 1];
        if (m) {
          if (!it.mark[0]) it.mark[0] = m;
          else if (!it.mark[1]) {
            if (c == 0x0651) { it.mark[1] = it.mark[0]; it.mark[0] = m; }   // shadda sits lowest
            else it.mark[1] = m;
          }
        }
      }
      continue;
    }
    Item it; memset(&it, 0, sizeof(it));
    it.cp = c;
    if (c == '\n') { it.cls = C_NEWLINE; _items[_n++] = it; continue; }
    if (c == ' ' || c == 0x00A0) { it.cls = C_SPACE; it.adv = _font->spaceAdv; _items[_n++] = it; continue; }

    if (isArabicLetter(c)) {
      // neighbours, skipping harakat
      int pi = i - 1; while (pi >= 0 && isMark(_cps[pi])) pi--;
      int ni = i + 1; while (ni < _ncp && isMark(_cps[ni])) ni++;
      uint32_t prev = pi >= 0 ? _cps[pi] : 0, next = ni < _ncp ? _cps[ni] : 0;
      bool joinPrev = joining(prev) == 'D' && joining(c) != 'U';

      uint32_t lig = 0;                                // lam + alef
      if (c == 0x0644) {
        switch (next) { case 0x0622: lig = 0xFEF5; break; case 0x0623: lig = 0xFEF7; break;
                        case 0x0625: lig = 0xFEF9; break; case 0x0627: lig = 0xFEFB; break; }
      }
      if (lig) {
        it.g = find(joinPrev ? lig + 1 : lig, 0);
        if (!it.g) it.g = find(lig, 0);
        it.cls = C_RTL;
        it.adv = it.g ? it.g->adv : 0;
        _items[_n++] = it;
        // harakat written between lam and alef stay on the ligature; skip the alef itself
        for (int k = i + 1; k < ni; k++) {
          const DalGlyph* m = find(_cps[k], 0);
          Item& L = _items[_n - 1];
          if (m) { if (!L.mark[0]) L.mark[0] = m; else if (!L.mark[1]) L.mark[1] = m; }
        }
        i = ni;
        continue;
      }
      bool joinNext = joining(c) == 'D' && (joining(next) == 'D' || joining(next) == 'R');
      uint8_t form = joinPrev && joinNext ? 2 : joinPrev ? 3 : joinNext ? 1 : 0;
      it.g = find(c, form);
      if (!it.g) it.g = find(c, 0);
      it.cls = C_RTL;
    } else {
      it.g = find(c, 0);
      if (!it.g) it.g = find('?', 0);
      it.cls = isDigit(c) ? C_NUM : isLatin(c) ? C_LTR :
               (c == 0x060C || c == 0x061B || c == 0x061F || c == 0x066A) ? C_RTL : C_NEUTRAL;
      if (c >= 0x00C0 && c < 0x0600) it.cls = C_LTR;   // accented Latin, Greek, Cyrillic ...
    }
    it.adv = it.g ? it.g->adv : 0;
    _items[_n++] = it;
  }
}

// Simplified Unicode bidi: gives every item an embedding level for one paragraph.
// Arabic is level 1; Latin words and numbers inside it are level 2, so they read left-to-right.
void DalText::resolve(bool rtlPara) {
  // separators between digits belong to the number: 3.5  10:30  1,000  2026/11/07  3-7
  for (int i = 1; i + 1 < _n; i++) {
    uint32_t c = _items[i].cp;
    if (_items[i].cls == C_NEUTRAL && _items[i - 1].cls == C_NUM && _items[i + 1].cls == C_NUM &&
        (c == '.' || c == ',' || c == ':' || c == '/' || c == '-' || c == '+' || c == 0x066B || c == 0x066C)) _items[i].cls = C_NUM;
  }
  for (int i = 0; i + 1 < _n; i++)                       // %, +, - next to a number
    if (_items[i].cls == C_NEUTRAL && (_items[i].cp == '%' || _items[i].cp == 0x066A) &&
        i > 0 && _items[i - 1].cls == C_NUM) _items[i].cls = C_NUM;

  // direction of each strong item ('R' or 'L'); numbers follow the last strong letter
  char dir[MAX_CHARS];
  char lastStrong = rtlPara ? 'R' : 'L';
  for (int i = 0; i < _n; i++) {
    uint8_t k = _items[i].cls;
    if (k == C_RTL) { dir[i] = 'R'; lastStrong = 'R'; }
    else if (k == C_LTR) { dir[i] = 'L'; lastStrong = 'L'; }
    else if (k == C_NUM) {                                           // 'N' = number in Arabic context
      bool arabicDigit = _items[i].cp >= 0x0660 && _items[i].cp <= 0x066C;  // ٠..٩ stay Arabic numbers
      dir[i] = (lastStrong == 'L' && !arabicDigit) ? 'L' : 'N';
    }
    else dir[i] = 0;
  }
  // a percent sign after a number in Arabic text is a neutral, so it lands on the number's left: %15
  for (int i = 1; i < _n; i++)
    if ((_items[i].cp == '%' || _items[i].cp == 0x066A) && dir[i] == 'N') { dir[i] = 0; _items[i].cls = C_NEUTRAL; }

  // paired brackets (Unicode bidi rule N0): both brackets take the direction of the text inside
  // them, so "book(s)" keeps its brackets inside an Arabic sentence
  {
    char e = rtlPara ? 'R' : 'L';
    int16_t stack[63]; int sp = 0;
    int16_t po[MAX_CHARS / 2], pc[MAX_CHARS / 2]; int np = 0;
    for (int i = 0; i < _n && np < MAX_CHARS / 2; i++) {
      if (_items[i].cls != C_NEUTRAL) continue;
      uint32_t c = _items[i].cp;
      if (c == '(' || c == '[' || c == '{') { if (sp == 63) break; stack[sp++] = i; }
      else if (c == ')' || c == ']' || c == '}') {
        uint32_t open = c == ')' ? '(' : c == ']' ? '[' : '{';
        for (int k = sp - 1; k >= 0; k--)
          if (_items[stack[k]].cp == open) { po[np] = stack[k]; pc[np] = i; np++; sp = k; break; }
      }
    }
    for (int a = 1; a < np; a++)                        // in order of the opening bracket
      for (int b = a; b > 0 && po[b - 1] > po[b]; b--) {
        int16_t t = po[b]; po[b] = po[b - 1]; po[b - 1] = t; t = pc[b]; pc[b] = pc[b - 1]; pc[b - 1] = t;
      }
    for (int p = 0; p < np; p++) {
      bool same = false, other = false;
      for (int k = po[p] + 1; k < pc[p]; k++) {
        char d = dir[k] == 'N' ? 'R' : dir[k];
        if (d == e) same = true; else if (d) other = true;
      }
      char set = 0;
      if (same) set = e;
      else if (other) {
        char before = e;
        for (int k = po[p] - 1; k >= 0; k--) if (dir[k]) { before = dir[k] == 'N' ? 'R' : dir[k]; break; }
        set = (before != e) ? before : e;
      }
      if (set) dir[po[p]] = dir[pc[p]] = set;
    }
  }

  // spaces and punctuation take the direction of their surroundings
  for (int i = 0; i < _n; ) {
    if (dir[i]) { i++; continue; }
    int j = i; while (j < _n && !dir[j]) j++;
    char before = i > 0 ? dir[i - 1] : (rtlPara ? 'R' : 'L');
    char after = j < _n ? dir[j] : (rtlPara ? 'R' : 'L');
    if (before == 'N') before = 'R';
    if (after == 'N') after = 'R';
    char d = (before == after) ? before : (rtlPara ? 'R' : 'L');
    for (int k = i; k < j; k++) dir[k] = d;
    i = j;
  }
  for (int i = 0; i < _n; i++) {
    char d = dir[i];
    if (rtlPara) _items[i].level = (d == 'R') ? 1 : 2;
    else _items[i].level = (d == 'L') ? 0 : (d == 'N') ? 2 : 1;
  }
}

int DalText::lineWidth(int from, int to) const {
  int w = 0;
  for (int i = from; i < to; i++) w += _items[i].adv;
  return w;
}

void DalText::drawGlyph(const DalGlyph* g, int penX, int baseline, DalPixelFn fn, void* ctx) const {
  if (!g || !g->w || !fn) return;
  int ox = penX + g->xoff, oy = baseline + g->yoff;
  int rowBytes = (g->w + 1) / 2;
  const uint8_t* b = _font->bits + g->offset;
  for (int y = 0; y < g->h; y++)
    for (int x = 0; x < g->w; x++) {
      uint8_t v = b[y * rowBytes + x / 2];
      uint8_t a = (x & 1) ? (v & 15) : (v >> 4);
      if (a) fn(ctx, ox + x, oy + y, a);
    }
}

void DalText::drawLine(int from, int to, int x, int baseline, int width, DalAlign align, bool last,
                        bool rtlPara, DalPixelFn fn, void* ctx) {
  while (from < to && _items[from].cls == C_SPACE) from++;        // trim spaces at both ends
  while (to > from && _items[to - 1].cls == C_SPACE) to--;
  if (from >= to) return;

  // visual order: reverse every run at level >= L, from the highest level down to 1
  int n = to - from;
  for (int i = 0; i < n; i++) _order[i] = from + i;
  uint8_t maxL = 0;
  for (int i = from; i < to; i++) if (_items[i].level > maxL) maxL = _items[i].level;
  for (int L = maxL; L >= 1; L--) {
    for (int i = 0; i < n; ) {
      if (_items[_order[i]].level < L) { i++; continue; }
      int j = i; while (j < n && _items[_order[j]].level >= L) j++;
      for (int a = i, b = j - 1; a < b; a++, b--) { int16_t t = _order[a]; _order[a] = _order[b]; _order[b] = t; }
      i = j;
    }
  }

  int w = lineWidth(from, to);
  int spaces = 0;
  for (int i = from; i < to; i++) if (_items[i].cls == C_SPACE) spaces++;
  int extra = 0, extraRem = 0;
  int pen;
  if (align == DAL_JUSTIFY && !last && spaces > 0 && w < width) {
    extra = (width - w) / spaces; extraRem = (width - w) % spaces;
    pen = x;
  } else if (align == DAL_LEFT || (align == DAL_JUSTIFY && !rtlPara)) pen = x;
  else if (align == DAL_CENTER) pen = x + (width - w) / 2;
  else pen = x + width - w;                                         // right (and last justified Arabic line)

  for (int k = 0; k < n; k++) {
    Item& it = _items[_order[k]];
    if (it.cls == C_SPACE) {
      pen += it.adv;
      if (extra || extraRem) { pen += extra; if (extraRem) { pen++; extraRem--; } }
      continue;
    }
    const DalGlyph* g = it.g;
    if ((it.level & 1) && mirror(it.cp) != it.cp) { const DalGlyph* m = find(mirror(it.cp), 0); if (m) g = m; }
    drawGlyph(g, pen, baseline, fn, ctx);
    // harakat: centred on the letter's ink, just above its top (or below its bottom)
    if (g && (it.mark[0] || it.mark[1])) {
      int cx = pen + g->xoff + g->w / 2;
      int top = baseline + g->yoff, bottom = baseline + g->yoff + g->h;
      for (int m = 0; m < 2; m++) {
        const DalGlyph* mg = it.mark[m];
        if (!mg || !mg->w) continue;
        int mx = cx - mg->w / 2;
        if (isAboveMark(mg->key >> 2)) { int my = top - 1 - mg->h; drawGlyph(mg, mx - mg->xoff, my - mg->yoff, fn, ctx); top = my; }
        else { int my = bottom + 1; drawGlyph(mg, mx - mg->xoff, my - mg->yoff, fn, ctx); bottom = my + mg->h; }
      }
    }
    pen += it.adv;
  }
}

int DalText::draw(const char* utf8, int x, int y, int width, DalAlign align, DalPixelFn fn, void* ctx) {
  if (!_font || !utf8) return 0;
  decode(utf8);
  shape();
  int lineH = _font->lineHeight + _gap;
  int line = 0;

  int p = 0;
  while (p <= _n) {
    // one paragraph = up to the next newline
    int q = p; while (q < _n && _items[q].cls != C_NEWLINE) q++;
    bool rtl = true;                                   // direction from the first strong letter
    for (int i = p; i < q; i++) {
      if (_items[i].cls == C_RTL) { rtl = true; break; }
      if (_items[i].cls == C_LTR) { rtl = false; break; }
    }
    // resolve this paragraph only (temporarily treat it as the whole text)
    Item* base = _items + p; int saveN = _n;
    if (p > 0) memmove(_items, base, sizeof(Item) * (q - p));
    _n = q - p;
    resolve(rtl);

    // greedy line breaking on spaces
    int lineStart = 0, lineW = 0, i = 0;
    bool any = false;
    while (i < _n) {
      if (_items[i].cls == C_SPACE) { i++; continue; }
      int ws = i, ww = 0; while (i < _n && _items[i].cls != C_SPACE) { ww += _items[i].adv; i++; }
      int spaceBefore = any ? lineWidth(lineStart, ws) - lineW : 0;
      if (any && lineW + spaceBefore + ww > width) {
        int baseline = y + line * lineH + _font->ascent;
        drawLine(lineStart, ws, x, baseline, width, align, false, rtl, fn, ctx);
        line++; lineStart = ws; lineW = 0; any = false;
      }
      lineW = lineWidth(lineStart, i);
      any = true;
    }
    if (any || p == q) {
      int baseline = y + line * lineH + _font->ascent;
      drawLine(lineStart, _n, x, baseline, width, align, true, rtl, fn, ctx);
      line++;
    }
    // restore the full item list for the next paragraph
    _n = saveN;
    if (p > 0) { decode(utf8); shape(); }
    p = q + 1;
  }
  return line * lineH;
}

int DalText::width(const char* utf8) {
  if (!_font || !utf8) return 0;
  decode(utf8); shape();
  int w = 0;
  for (int i = 0; i < _n; i++) if (_items[i].cls != C_NEWLINE) w += _items[i].adv;
  return w;
}

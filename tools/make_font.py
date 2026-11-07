#!/usr/bin/env python3
"""
Dal font generator: turns a TrueType Arabic font (+ a Latin font) into a small bitmap font
for microcontrollers. Each Arabic letter is rendered in its four joining forms
(isolated / initial / medial / final) by the real shaping engine (HarfBuzz via Pillow+raqm),
using ZERO WIDTH JOINERs to force each form. Glyphs are stored as 4-bit anti-aliased bitmaps.

usage: make_font.py ARABIC.ttf LATIN.ttf SIZE NAME OUT.h [weight]
"""
import sys
from PIL import Image, ImageDraw, ImageFont

ar_path, la_path, size, name, out = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4], sys.argv[5]
weight = (sys.argv[6] if len(sys.argv) > 6 else 'Regular').encode()
ZWJ = '‍'

def load(path, sz):
    f = ImageFont.truetype(path, sz, layout_engine=ImageFont.Layout.RAQM)
    try:
        if weight in f.get_variation_names(): f.set_variation_by_name(weight)
    except Exception:
        pass
    return f

far = load(ar_path, size)
fla = load(la_path, max(8, round(size * 0.78)))

# joining types: D = dual, R = right only, U = none
RIGHT = set([0x0622, 0x0623, 0x0624, 0x0625, 0x0627, 0x0629, 0x062F, 0x0630, 0x0631, 0x0632, 0x0648, 0x0671, 0x0698])
NONE = set([0x0621])
letters = list(range(0x0621, 0x063B)) + list(range(0x0641, 0x064B)) + [0x0671, 0x067E, 0x0686, 0x0698, 0x06A4, 0x06A9, 0x06AF, 0x06CC]
MARKS = list(range(0x064B, 0x0653)) + [0x0670]
AR_SINGLE = [0x0640, 0x060C, 0x061B, 0x061F, 0x066A, 0x066B, 0x066C] + list(range(0x0660, 0x066A))
LAMALEF = {0xFEF5: 'لآ', 0xFEF7: 'لأ', 0xFEF9: 'لإ', 0xFEFB: 'لا'}
LATIN = list(range(0x21, 0x7F)) + [0x00AB, 0x00BB, 0x00B0, 0x2013, 0x2014, 0x2026]

PAD = size * 3

def render(font, text, rtl):
    W, H = size * 10, size * 6
    img = Image.new('L', (W, H), 0)
    d = ImageDraw.Draw(img)
    x0, base = PAD, PAD + size
    kw = dict(anchor='ls')
    if rtl: kw.update(direction='rtl', language='ar')
    d.text((x0, base), text, font=font, fill=255, **kw)
    adv = font.getlength(text, **({'direction': 'rtl'} if rtl else {}))
    return img, x0, base, adv

def render_after_alef(font, ch):
    """Isolated form as it appears inside a word (after a letter that does not join forward).
    Some fonts use a different isolated shape there than for a lone character, e.g. word-final heh."""
    W, H = size * 10, size * 6
    R, base = W - PAD, PAD + size
    pre = '\u0627'
    full = Image.new('L', (W, H), 0); ImageDraw.Draw(full).text((R, base), pre + ch, font=font, fill=255, anchor='rs', direction='rtl', language='ar')
    alone = Image.new('L', (W, H), 0); ImageDraw.Draw(alone).text((R, base), pre, font=font, fill=255, anchor='rs', direction='rtl', language='ar')
    a, b = full.load(), alone.load()
    for y in range(H):
        for x in range(W):
            if b[x, y]: a[x, y] = 0 if a[x, y] <= b[x, y] + 40 else a[x, y]
    adv_pre = font.getlength(pre, direction='rtl')
    adv = font.getlength(pre + ch, direction='rtl') - adv_pre
    pen_left = R - adv_pre - adv
    return full, pen_left, base, adv

def crop(img, x0, base):
    px = img.load()
    bb = img.point(lambda v: 255 if v > 12 else 0).getbbox()
    if not bb: return None
    return bb

def to4(img, bb):
    x1, y1, x2, y2 = bb
    w, h = x2 - x1, y2 - y1
    px = img.load()
    rows = []
    for y in range(y1, y2):
        row = []
        for x in range(x1, x2, 2):
            a = (px[x, y] + 8) // 17
            b = (px[x + 1, y] + 8) // 17 if x + 1 < x2 else 0
            row.append((a << 4) | b)
        rows.extend(row)
    return w, h, rows

glyphs = []   # (key, w, h, xoff, yoff, adv, flags, data)
def add(key, img, x0, base, adv, flags=0, center=None):
    bb = crop(img, x0, base)
    if bb is None:
        glyphs.append((key, 0, 0, 0, 0, round(adv), flags, [])); return
    w, h, data = to4(img, bb)
    ref = x0 if center is None else center
    glyphs.append((key, w, h, round(bb[0] - ref), bb[1] - base, round(adv), flags, data))

# Letters whose isolated shape inside a word differs from a lone character.
# In Noto Sans Arabic this is heh: alone it is drawn knotted (هـ-like), after و ز ر ا ... it is round (ه).
# Checked by eye at a large size; other fonts may need more letters here.
IN_WORD = {0x0647}

FORM_TEXT = [lambda c: c, lambda c: c + ZWJ, lambda c: ZWJ + c + ZWJ, lambda c: ZWJ + c]
for cp in letters:
    ch = chr(cp)
    forms = [0] if cp in NONE else [0, 3] if cp in RIGHT else [0, 1, 2, 3]
    for f in forms:
        img, x0, base, adv = render(far, FORM_TEXT[f](ch), True)
        if f == 0 and cp in IN_WORD:
            img, x0, base, adv = render_after_alef(far, ch)     # e.g. heh: a different shape inside words
        add((cp << 2) | f, img, x0, base, adv)
for cp in AR_SINGLE:
    img, x0, base, adv = render(far, chr(cp), True)
    add(cp << 2, img, x0, base, adv)
for cp, txt in LAMALEF.items():
    img, x0, base, adv = render(far, txt, True); add(cp << 2, img, x0, base, adv)
    img, x0, base, adv = render(far, ZWJ + txt, True); add((cp + 1) << 2, img, x0, base, adv)
# marks: render on a tatweel and keep only the difference, positioned from the tatweel's centre
timg, tx0, tbase, tadv = render(far, 'ـ', True)
for cp in MARKS:
    img, x0, base, adv = render(far, 'ـ' + chr(cp), True)
    diff = Image.eval(Image.merge('L', [img]), lambda v: v)
    a, b = img.load(), timg.load()
    for y in range(img.size[1]):
        for x in range(img.size[0]):
            if b[x, y] > 40: a[x, y] = 0 if a[x, y] <= b[x, y] + 40 else a[x, y]
    add(cp << 2, img, x0, base, 0, flags=1, center=x0 + tadv / 2)
for cp in LATIN:
    img, x0, base, adv = render(fla, chr(cp), False)
    add(cp << 2, img, x0, base, adv)
space_adv = round(far.getlength(' ', direction='rtl'))

glyphs.sort(key=lambda g: g[0])
bits, out_g, off = [], [], 0
for key, w, h, xo, yo, adv, fl, data in glyphs:
    out_g.append(f"  {{0x{key:06X}, {off}, {w}, {h}, {xo}, {yo}, {adv}, {fl}}},")
    bits.extend(data); off += len(data)
letters_only = [g for g in glyphs if not g[6] & 1 and g[2]]
asc = max(-g[4] for g in glyphs if g[2])
desc = max(g[4] + g[2] for g in glyphs if g[2])
body_asc = max(-g[4] for g in letters_only)
U = name.upper()
with open(out, 'w', encoding='utf-8') as f:
    f.write(f"// Generated by Dal tools/make_font.py from {ar_path.split('/')[-1]} + {la_path.split('/')[-1]}, size {size}\n")
    f.write("// 4-bit anti-aliased glyphs; Arabic letters in all joining forms. Do not edit by hand.\n#pragma once\n#include \"../Dal.h\"\n\n")
    f.write(f"static const uint8_t {U}_BITS[] DAL_PROGMEM = {{\n")
    for i in range(0, len(bits), 32): f.write('  ' + ','.join(str(b) for b in bits[i:i + 32]) + ',\n')
    f.write("};\n\n")
    f.write(f"static const DalGlyph {U}_GLYPHS[] DAL_PROGMEM = {{\n" + "\n".join(out_g) + "\n};\n\n")
    f.write(f"static const DalFont {name} = {{ {U}_GLYPHS, {len(glyphs)}, {U}_BITS, {asc}, {desc}, {asc + desc + 2}, {space_adv} }};\n")
print(f"{name}: {len(glyphs)} glyphs, {len(bits)} bytes, ascent {asc}, descent {desc}, body ascent {body_asc}")

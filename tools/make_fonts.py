#!/usr/bin/env python3
"""MixtapeDSi font pipeline: Unscii (public domain) -> DS 4bpp console tiles.
Emits assets/font_ui.h (8x8 UI font, 256 glyphs) and assets/font_lyr.h
(8x16 lyric font, 256 glyphs x2 tiles). Text palette banks are set at runtime:
bank0 white, bank1 emerald, bank2 gray, bank3 violet (idx0 transparent)."""
import os

WANT = list(range(0x20, 0x7F)) + [
    0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA,
    0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5,
    0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0,
    0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB,
    0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6,
    0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE0, 0xE1,
    0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC,
    0xED, 0xEE, 0xEF, 0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
    0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
    0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2026, 0x2190,
    0x2192, 0x2665,
]


def load_hex(path):
    d = {}
    for line in open(path):
        line = line.strip()
        if not line or ":" not in line:
            continue
        cp, bits = line.split(":")
        d[int(cp, 16)] = bits
    return d


f8 = load_hex("third_party/fonts/unscii-8.hex")
f16 = load_hex("third_party/fonts/unscii-16.hex")

# sanity: row lengths (a few control glyphs are 8x16; UI takes top half)
for cp, b in f8.items():
    assert len(b) in (16, 32), (hex(cp), len(b))
for cp, b in f16.items():
    assert len(b) in (32, 64), (hex(cp), len(b))

missing8 = [c for c in WANT if c not in f8]
missing16 = [c for c in WANT if c not in f16]
print("missing8:", [hex(c) for c in missing8])
print("missing16:", [hex(c) for c in missing16])

# fallback: '?' glyph
q8 = f8.get(0x3F, "0" * 16)
q16 = f16.get(0x3F, "0" * 32)


def rows8(cp):
    b = f8.get(cp, q8)
    out = []
    for r in range(8):
        byte = int(b[r * 2:r * 2 + 2], 16)
        out.append([(byte >> (7 - i)) & 1 for i in range(8)])
    return out


def rows16(cp):
    b = f16.get(cp, q16)
    if len(b) == 64:  # 16x16 wide: take left 8 cols
        out = []
        for r in range(16):
            w = int(b[r * 4:r * 4 + 4], 16)
            out.append([(w >> (15 - i)) & 1 for i in range(8)])
        return out
    out = []
    for r in range(16):
        byte = int(b[r * 2:r * 2 + 2], 16)
        out.append([(byte >> (7 - i)) & 1 for i in range(8)])
    return out


def pack4(rows):
    """8px rows of 0/1 -> 4bpp bytes (low nibble = left pixel)."""
    out = []
    for r in rows:
        for i in range(0, 8, 2):
            out.append(r[i] | (r[i + 1] << 4))
    return out


# UI font: 256 glyphs, codepoint order 0..255 (blank if absent and not wanted)
ui_tiles = []
for cp in range(256):
    if cp in f8 and (cp in WANT or cp < 0x100):
        ui_tiles += pack4(rows8(cp))
    else:
        ui_tiles += [0] * 32

# lyric font: 8x16, two stacked tiles per glyph; map codepoint->index via table
ORDER_LYR = (list(range(0x20, 0x7F)) + list(range(0xC0, 0x100)) +
             [c for c in WANT if c >= 0x100])
lyr_index = {}  # cp -> glyph slot
lyr_tiles = []
slot = 0
order = ORDER_LYR
for cp in order:
    lyr_index[cp] = slot
    slot += 1
    r = rows16(cp)
    lyr_tiles += pack4(r[:8])
    lyr_tiles += pack4(r[8:])
assert slot <= 200, slot


def emit_bytes(name, data, per_line=32):
    L = [f"static const unsigned char {name}[{len(data)}] = {{"]
    for i in range(0, len(data), per_line):
        L.append(",".join(map(str, data[i:i + per_line])) + ",")
    L.append("};")
    return "\n".join(L)


os.makedirs("assets", exist_ok=True)
h = ["/* MixtapeDSi UI font (Unscii-8, PD) 4bpp 256 glyphs, codepoint order */",
     emit_bytes("fontUiTiles", ui_tiles),
     "static const unsigned int fontUiTilesLen = %d;" % len(ui_tiles)]
open("assets/font_ui.h", "w").write("\n".join(h) + "\n")

h2 = ["/* MixtapeDSi lyric font (Unscii-16, PD) 4bpp 8x16, two tiles per glyph */",
      emit_bytes("fontLyrTiles", lyr_tiles),
      "static const unsigned int fontLyrTilesLen = %d;" % len(lyr_tiles),
      "/* codepoint -> glyph slot (slots are ASCII/Latin-1 ordered, then extras) */",
      "static const unsigned short lyrCpToSlot[%d] = {" % 0x100 + ",".join(
          str(lyr_index.get(c, lyr_index[0x3F])) for c in range(0x100)) + "};"]
open("assets/font_lyr.h", "w").write("\n".join(h2) + "\n")
print(f"ui tiles={len(ui_tiles)//32} lyr slots={slot} tiles={len(lyr_tiles)//32}")

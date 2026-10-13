#!/usr/bin/env python3
"""Generate DS 8bpp tiled backdrops as C headers (256x192, <=1024 tiles).
Dark charcoal gradient + emerald accents + panel frame. Palette idx0=black.
Writes assets/bg_top.h and assets/bg_bot.h."""
import os

W, H = 256, 192
# Palette bank offset: console text owns entries 0-127; backdrop lives at 128+.
POFF = 128

# --- palette ---
PAL = [(0, 0, 0)]  # idx 0
# gradient ramp idx 1..40: #0c0e12 -> #1e222b
for i in range(40):
    t = i / 39
    PAL.append((int(12 + t * 18), int(14 + t * 20), int(18 + t * 25)))
# panel fill idx 41..44
PAL += [(35, 38, 46), (30, 33, 41), (25, 28, 35), (20, 23, 30)]
# emerald accents idx 45..50 (dim -> bright)
PAL += [(18, 90, 58), (24, 120, 76), (32, 150, 96), (46, 204, 130), (120, 230, 170), (200, 245, 220)]
# dot pattern idx 51, border idx 52
PAL += [(16, 18, 24), (52, 58, 70)]
assert len(PAL) <= 64


def lum(c):
    return c[0] * 3 + c[1] * 6 + c[2]


def nearest(rgb):
    best, bi = None, 0
    for i, p in enumerate(PAL):
        d = (p[0] - rgb[0]) ** 2 + (p[1] - rgb[1]) ** 2 + (p[2] - rgb[2]) ** 2
        if best is None or d < best:
            best, bi = d, i
    return bi


def base_px(x, y):
    t = y / (H - 1)
    r = int(12 + t * 18)
    g = int(14 + t * 20)
    b = int(18 + t * 25)
    return (r, g, b)


def panel(img, x0, y0, x1, y1):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            corners = (x, y) in [(x0, y0), (x0, y1), (x1, y0), (x1, y1),
                                 (x0 + 1, y0), (x0, y0 + 1), (x1 - 1, y0), (x1, y0 + 1),
                                 (x0 + 1, y1), (x0, y1 - 1), (x1 - 1, y1), (x1, y1 - 1)]
            if corners:
                continue
            edge = x in (x0 + 2, x1 - 2) or y in (y0 + 2, y1 - 2)
            img[y][x] = PAL[52] if edge else PAL[41 + ((x + y) % 2)]


def accents(img, top):
    for x in range(8, W - 8):
        img[2][x] = PAL[47]
    for i in range(6):  # EQ-motif dots top-right
        h = [4, 7, 10, 6, 8, 5][i]
        for k in range(h):
            img[14 - k][232 + i * 3] = PAL[48]
            img[14 - k][233 + i * 3] = PAL[48]
    if top:  # record ring motif, right side
        cx, cy, rr = 208, 96, 34
        for y in range(cy - rr - 2, cy + rr + 3):
            for x in range(cx - rr - 2, cx + rr + 3):
                d = ((x - cx) ** 2 + (y - cy) ** 2) ** 0.5
                if abs(d - rr) < 1.2:
                    img[y][x] = PAL[46]
                elif abs(d - rr * 0.55) < 1.0:
                    img[y][x] = PAL[45]


def build(top):
    img = [[base_px(x, y) for x in range(W)] for y in range(H)]
    # dot grid
    for y in range(6, H - 4, 8):
        for x in range(6, W - 4, 8):
            img[y][x] = PAL[51]
    panel(img, 4, 4, W - 5, 26)       # header strip
    panel(img, 4, 30, W - 5, H - 9)   # content panel
    accents(img, top)
    # quantize to palette indices
    return [[nearest(px) for px in row] for row in img]


def emit(name, grid):
    tiles = []
    tmap = {}
    amap = []
    for ty in range(H // 8):
        for tx in range(W // 8):
            t = tuple(grid[y][x] for y in range(ty * 8, ty * 8 + 8) for x in range(tx * 8, tx * 8 + 8))
            if t not in tmap:
                tmap[t] = len(tiles)
                tiles.append(t)
            amap.append(tmap[t])
    assert len(tiles) <= 1024, len(tiles)
    pal16 = [(p[0] >> 3) | ((p[1] >> 3) << 5) | ((p[2] >> 3) << 10) for p in PAL]
    full = [0] * 256
    full[POFF:POFF + len(pal16)] = pal16
    L = [f"/* MixtapeDSi generated backdrop ({name}) - do not edit, see tools/make_bg.py */",
         f"static const unsigned short {name}Pal[256] = {{",
         ",".join(str(v) for v in full) + "};",
         f"static const unsigned int {name}PalLen = 512;",
         f"static const unsigned char {name}Tiles[{len(tiles) * 64}] = {{"]
    for t in tiles:
        L.append(",".join(str(v + POFF) for v in t) + ",")
    L.append("};")
    L.append(f"static const unsigned int {name}TilesLen = {len(tiles) * 64};")
    L.append(f"static const unsigned short {name}Map[{(W // 8) * (H // 8)}] = {{")
    L.append(",".join(str(v) for v in amap) + "};")
    L.append(f"static const unsigned int {name}MapLen = {(W // 8) * (H // 8) * 2};")
    return "\n".join(L) + "\n"


os.makedirs("assets", exist_ok=True)
open("assets/bg_top.h", "w").write(emit("bg_top", build(True)))
open("assets/bg_bot.h", "w").write(emit("bg_bot", build(False)))
print("wrote assets/bg_top.h assets/bg_bot.h")

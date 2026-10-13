#!/usr/bin/env python3
"""MixtapeDSi graphical UI tileset generator.
Emits assets/ui_top.h, assets/ui_bot.h, assets/ui_cover.h (placeholder),
assets/ui_spr.h (EQ bar frames), assets/ui_layout.h (ids/coords/zones).
Console text owns palette 0-127; UI art 128-191; runtime covers 192-255."""
import os, math

W, H = 256, 192
POFF = 128

PAL = [(0, 0, 0)]
for i in range(32):
    t = i / 31
    PAL.append((int(5 + t * 12), int(8 + t * 14), int(12 + t * 18)))
PAL += [(18, 25, 30), (24, 32, 38), (31, 41, 47), (39, 50, 57)]
PAL += [(10, 48, 39), (12, 75, 56), (18, 112, 78), (38, 186, 123),
        (76, 230, 166), (176, 255, 216)]
PAL += [(35, 24, 61), (61, 35, 103), (106, 57, 170), (166, 107, 255)]
PAL += [(95, 108, 119), (183, 200, 201), (241, 248, 245)]
PAL += [(8, 18, 22), (14, 29, 31), (23, 45, 44), (44, 69, 62)]
while len(PAL) < 64:
    PAL.append((0, 0, 0))
assert PAL[41] == (76, 230, 166)

P2, P3 = 35, 36
E3, E4, E6 = 41, 42, 44
V1, V2, V3 = 45, 46, 47
DOT, BORD, DIM, WHT, PUR = 48, 49, 50, 51, 52


def new_img():
    return [[None for _ in range(W)] for _ in range(H)]


def panel(img, x0, y0, x1, y1, fill=P2, edge=BORD):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            if x in (x0, x1) or y in (y0, y1):
                img[y][x] = edge
            else:
                img[y][x] = fill + ((x + y) % 3 == 0 and fill < edge)


def surface(img, x0, y0, x1, y1, fill):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            img[y][x] = fill + ((x + y) % 5 == 0 and fill < 35)


def atmosphere(img, phase=0):
    for y in range(H):
        for x in range(W):
            band = int((y / H) * 10)
            glow = max(0, 8 - abs(x - 180) // 18 - abs(y - 54) // 18)
            img[y][x] = min(31, 1 + band + glow)
    for r in range(58, 4, -3):
        col = 22 + max(0, (58 - r) // 10)
        for y in range(max(0, 52 - r), min(H, 52 + r + 1)):
            for x in range(max(0, 185 - r), min(W, 185 + r + 1)):
                if (x - 185) * (x - 185) + (y - 52) * (y - 52) <= r * r:
                    img[y][x] = min(31, col)


def orb(img, cx, cy, radius, col):
    for y in range(max(0, cy - radius), min(H, cy + radius + 1)):
        for x in range(max(0, cx - radius), min(W, cx + radius + 1)):
            d = (x - cx) * (x - cx) + (y - cy) * (y - cy)
            if d <= radius * radius:
                img[y][x] = col


def rule(img, x0, y, x1, col):
    for x in range(x0, x1 + 1):
        img[y][x] = col


def ring(img, cx, cy, r, col):
    for y in range(cy - r - 1, cy + r + 2):
        for x in range(cx - r - 1, cx + r + 2):
            if 0 <= x < W and 0 <= y < H and abs(((x - cx) ** 2 + (y - cy) ** 2) ** 0.5 - r) < 1.1:
                img[y][x] = col


def tri(img, x0, y0, w, h, col, flip=False):
    for y in range(h):
        t = y / max(1, h - 1)
        ww = int(w * t) if flip else int(w * (1 - t))
        for x in range(max(1, ww)):
            img[y0 + y][x0 + x] = col


def glyph_prev_next(img, cx, cy, col, is_next):
    s = 1 if is_next else -1
    tri(img, cx + (2 if is_next else -11), cy - 6, 9, 13, col, flip=is_next)
    for y in range(cy - 6, cy + 7):
        for x in range(cx + 5 * s - 1, cx + 5 * s + 1):
            img[y][x] = col


def glyph_shuffle(img, cx, cy, col):
    for i in range(11):
        img[cy - 5 + i][cx - 5 + i] = col
        img[cy - 5 + i][cx + 5 - i] = col
    for x in range(cx + 2, cx + 6):
        img[cy + 4][x] = col
        img[cy - 6][x] = col


def glyph_repeat(img, cx, cy, col):
    for a in range(360):
        x = int(cx + 6 * math.cos(math.radians(a)))
        y = int(cy + 6 * math.sin(math.radians(a)) * 0.7)
        if 20 < a < 320 and 0 <= x < W and 0 <= y < H:
            img[y][x] = col
    for x in range(cx - 2, cx + 3):
        img[cy - 6][x] = col


def heart(img, cx, cy, col, filled):
    outline = [(-5, -2), (-4, -3), (-3, -4), (-2, -4), (-1, -3), (0, -2),
               (5, -2), (4, -3), (3, -4), (2, -4), (1, -3),
               (-6, 0), (-5, 1), (-4, 2), (-3, 3), (-2, 4), (-1, 5), (0, 6),
               (6, 0), (5, 1), (4, 2), (3, 3), (2, 4), (1, 5)]
    for dx, dy in outline:
        img[cy + dy][cx + dx] = col
    if filled:
        for dy in range(-2, 5):
            for dx in range(-5, 6):
                if abs(dx) + abs(dy) * 2 < 9:
                    img[cy + dy][cx + dx] = col


def button_block(img, cx, cy, kind, active):
    """Draw 3x3-cell button centered at (cx,cy). kind: shuf/prev/play/pause/next/rep."""
    ringcol = E4 if active or kind in ("play", "pause") else DIM
    gcol = E4 if (active or kind in ("play", "pause")) else WHT
    if kind == "rep" and not active:
        gcol = WHT
    ring(img, cx, cy, 10, ringcol)
    if kind == "shuf":
        glyph_shuffle(img, cx, cy, gcol)
    elif kind == "prev":
        glyph_prev_next(img, cx, cy, gcol, False)
    elif kind == "play":
        tri(img, cx - 4, cy - 6, 10, 13, gcol)
    elif kind == "pause":
        for y in range(cy - 6, cy + 7):
            for x in (cx - 4, cx - 3, cx + 2, cx + 3):
                img[y][x] = gcol
    elif kind == "next":
        glyph_prev_next(img, cx, cy, gcol, True)
    elif kind == "rep":
        glyph_repeat(img, cx, cy, gcol)


def chevron(img, cx, cy, col, up):
    for i in range(7):
        d = abs(i - 3)
        y = cy - d if up else cy + d
        img[y][cx - 3 + i] = col
        img[y + (1 if up else -1)][cx - 3 + i] = col


def tab_icon(img, cx, cy, kind, col):
    if kind == 0:
        for y in range(cy - 5, cy + 6):
            img[y][cx + 3] = col
        for y in range(cy + 3, cy + 6):
            for x in range(cx - 2, cx + 4):
                img[y][x] = col
    elif kind == 1:
        tri(img, cx - 3, cy - 5, 8, 11, col)
    else:
        for r in range(3):
            for x in range(cx - 4, cx + 5):
                img[cy - 4 + r * 4][x] = col


def eq_motif(img, x, y0, heights, c1, c2):
    for i, h in enumerate(heights):
        for k in range(h):
            img[y0 - k][x + i * 3] = c1 if k < h - 2 else c2
            img[y0 - k][x + i * 3 + 1] = c1 if k < h - 2 else c2


CY = 132
BTNS = [("shuf", 36), ("prev", 76), ("play", 124), ("next", 172), ("rep", 212)]
CELL_X = {36: (3, 5), 76: (8, 10), 124: (14, 16), 172: (20, 22), 212: (25, 27)}
CELL_Y = (15, 17)


def build_bot():
    img = new_img()
    atmosphere(img)
    surface(img, 5, 5, W - 6, 31, 27)
    surface(img, 5, 36, W - 6, 151, 25)
    surface(img, 5, 158, W - 6, H - 6, 27)
    surface(img, 12, 49, 88, 123, 29)  # cover frame
    rule(img, 16, 48, 84, E3)
    rule(img, 16, 124, 84, V2)
    surface(img, 101, 52, 246, 89, 27)
    surface(img, 101, 96, 246, 122, 26)
    for x in range(111, 239, 8):
        img[108][x] = V1
        img[109][x] = V2
    for kind, cx in BTNS:
        button_block(img, cx, CY, kind, kind == "rep")
    heart(img, 232, 66, E4, False)
    eq_motif(img, 226, 20, [4, 7, 5], E3, V2)
    rule(img, 18, 145, 238, 35)
    tab_icon(img, 40, 176, 0, DIM)
    tab_icon(img, 128, 176, 1, E4)
    tab_icon(img, 216, 176, 2, DIM)
    return img


def build_top():
    img = new_img()
    atmosphere(img)
    surface(img, 5, 5, W - 6, 31, 27)
    surface(img, 5, 36, W - 6, 158, 25)
    surface(img, 5, 165, W - 6, H - 6, 27)
    orb(img, 128, 94, 43, 26)
    orb(img, 128, 94, 34, 27)
    rule(img, 30, 52, 225, 35)
    rule(img, 30, 143, 225, 35)
    chevron(img, 128, 52, E4, True)
    chevron(img, 128, 143, V2, False)
    for x in range(36, 220, 12):
        img[151][x] = E3
        img[152][x] = 36
    return img


def quantize(img):
    out = []
    for y in range(H):
        row = []
        for x in range(W):
            v = img[y][x]
            row.append(v if v is not None else 1 + int(y / (H - 1) * 39))
        out.append(row)
    return out


def cover_placeholder():
    px = []
    for y in range(64):
        row = []
        for x in range(64):
            t = y / 63
            r, g, b = int(40 + t * 60), int(20 + t * 30), int(90 + t * 80)
            if (x - 44) ** 2 + (y - 18) ** 2 < 100:
                r, g, b = (60, 220, 140)
            if y > 44 + int(12 * abs(((x / 64) * 3) % 1 - 0.5)):
                r, g, b = (10, 14, 20)
            row.append(192 + (min(r, 255) >> 6) * 16 + (min(g, 255) >> 6) * 4 + (min(b, 255) >> 6))
        px.append(row)
    return px


class Emitter:
    def __init__(self):
        self.tiles = []
        self.tmap = {}
        self.extra = {}

    def add(self, tup):
        if tup not in self.tmap:
            self.tmap[tup] = len(self.tiles)
            self.tiles.append(tup)
        return self.tmap[tup]

    def add_extra(self, name, tup):
        self.extra[name] = self.add(tup)


def cell_tup(grid, tx, ty):
    return tuple(grid[y][x] + POFF for y in range(ty * 8, ty * 8 + 8) for x in range(tx * 8, tx * 8 + 8))


def emit(name, grid, dynamic, extras_fn):
    """dynamic: {(tx,ty): key} cells get unique tiles, ids recorded.
       extras_fn(em, ...) builds named extra blocks. Returns (header, info)."""
    em = Emitter()
    amap = [0] * 768
    dyn_ids = {}
    for ty in range(24):
        for tx in range(32):
            key = dynamic.get((tx, ty))
            if key is not None:
                tid = len(em.tiles)
                em.tiles.append(cell_tup(grid, tx, ty))
                dyn_ids[key] = dyn_ids.get(key, []) + [(tx, ty, tid)]
                amap[ty * 32 + tx] = tid
    for ty in range(24):
        for tx in range(32):
            if (tx, ty) not in dynamic:
                amap[ty * 32 + tx] = em.add(cell_tup(grid, tx, ty))
    extras_fn(em, grid)
    assert len(em.tiles) <= 1024, (name, len(em.tiles))
    pal16 = [(p[0] >> 3) | ((p[1] >> 3) << 5) | ((p[2] >> 3) << 10) for p in PAL]
    full = [0] * 256
    full[POFF:POFF + len(pal16)] = pal16
    L = [f"/* MixtapeDSi UI ({name}) - generated, see tools/make_ui.py */",
         f"static const unsigned short {name}Pal[256] = {{" + ",".join(map(str, full)) + "};",
         f"static const unsigned int {name}PalLen = 512;",
         f"static const unsigned char {name}Tiles[{len(em.tiles) * 64}] = {{"]
    for t in em.tiles:
        L.append(",".join(map(str, t)) + ",")
    L += ["};", f"static const unsigned int {name}TilesLen = {len(em.tiles) * 64};",
          f"static const unsigned short {name}Map[768] = {{" + ",".join(map(str, amap)) + "};",
          f"static const unsigned int {name}MapLen = 1536;"]
    info = {"tiles": len(em.tiles), "dyn": dyn_ids, "extra": em.extra}
    return "\n".join(L) + "\n", info


def top_extras(em, grid):
    em.add_extra("eq_empty", tuple([P2 + POFF] * 64))
    em.add_extra("eq_full", tuple([E4 + POFF] * 64))
    for h in range(1, 8):
        t = []
        for y in range(8):
            for x in range(8):
                t.append((E4 if y >= 8 - h else P2) + POFF)
        em.add_extra(f"eq_p{h}", tuple(t))


def bot_extras(em, grid):
    # alternate button states drawn on scratch copies
    def block(kind, cx, active):
        tmp = [row[:] for row in grid_img_bot]
        button_block(tmp, cx, CY, kind, active)
        x0, x1 = CELL_X[cx]
        ids = []
        for ty in range(CELL_Y[0], CELL_Y[1] + 1):
            for tx in range(x0, x1 + 1):
                ids.append(em.add(cell_tup(quantize(tmp), tx, ty)))
        return ids
    em.extra["shuf_on"] = block("shuf", 36, True)
    em.extra["rep_off"] = block("rep", 212, False)
    em.extra["pause"] = block("pause", 124, True)
    em.extra["play"] = block("play", 124, True)
    # heart filled
    tmp = [row[:] for row in grid_img_bot]
    heart(tmp, 232, 66, E4, True)
    ids = []
    for ty in range(7, 10):
        for tx in range(28, 31):
            ids.append(em.add(cell_tup(quantize(tmp), tx, ty)))
    em.extra["heart_on"] = ids
    # progress + tab variants (single tiles, panel-row based)
    def seg(fillcol=None, dot=False, cap=None):
        t = []
        for y in range(8):
            for x in range(8):
                v = P2
                if fillcol is not None and 3 <= y <= 4:
                    v = fillcol
                if dot and abs(x - 3.5) < 2.5 and abs(y - 3.5) < 2.5:
                    v = WHT
                if cap == "l" and x < 2 and 3 <= y <= 4:
                    v = E4
                if cap == "r" and x > 5 and 3 <= y <= 4:
                    v = E4
                t.append(v + POFF)
        return tuple(t)
    em.add_extra("trk_empty", seg())
    em.add_extra("trk_fill", seg(E4))
    em.add_extra("trk_cap_l", seg(None, False, "l"))
    em.add_extra("trk_cap_r", seg(None, False, "r"))
    em.add_extra("trk_knob", seg(E4, True))
    em.add_extra("tab_dim", tuple([(BORD if y == 7 else P2) + POFF for y in range(8) for x in range(8)]))
    em.add_extra("tab_on", tuple([(E4 if y == 7 else P2) + POFF for y in range(8) for x in range(8)]))


grid_img_bot = quantize(build_bot())
grid_img_top = quantize(build_top())

# dynamic cells bottom: cover art 8x8, progress row, buttons, heart, tab underlines
dyn_bot = {}
for ty in range(7, 15):
    for tx in range(2, 10):
        dyn_bot[(tx, ty)] = "cover"
for tx in range(11, 31):
    dyn_bot[(tx, 12)] = "prog"
for cx in [36, 76, 124, 172, 212]:
    x0, x1 = CELL_X[cx]
    for ty in range(CELL_Y[0], CELL_Y[1] + 1):
        for tx in range(x0, x1 + 1):
            dyn_bot[(tx, ty)] = f"btn{cx}"
for ty in range(7, 10):
    for tx in range(28, 31):
        dyn_bot[(tx, ty)] = "heart"
for tx in range(2, 31):
    dyn_bot[(tx, 23)] = "tabline"

dyn_top = {}
for ty in range(1, 4):
    for tx in list(range(1, 5)) + list(range(27, 31)):
        dyn_top[(tx, ty)] = "eq"

bot_h, bot_info = emit("ui_bot", grid_img_bot, dyn_bot, bot_extras)
top_h, top_info = emit("ui_top", grid_img_top, dyn_top, top_extras)

os.makedirs("assets", exist_ok=True)
open("assets/ui_bot.h", "w").write(bot_h)
open("assets/ui_top.h", "w").write(top_h)

cov = cover_placeholder()
ct = []
for ty in range(8):
    for tx in range(8):
        ct.append(tuple(cov[y][x] for y in range(ty * 8, ty * 8 + 8) for x in range(tx * 8, tx * 8 + 8)))
cl = ["/* placeholder cover tiles (8x8 cells), palette bank 192+ (RGB444 uniform) */",
      "static const unsigned char coverPlaceTiles[4096] = {"]
for t in ct:
    cl.append(",".join(map(str, t)) + ",")
cl += ["};"]
open("assets/ui_cover.h", "w").write("\n".join(cl) + "\n")

# layout header
cov_ids = [tid for (_, _, tid) in bot_info["dyn"]["cover"]]


def uarr(name, ids):
    return "static const unsigned short " + name + "[" + str(len(ids)) + "] = {" + ",".join(map(str, ids)) + "};"


LN = []
LN.append("/* MixtapeDSi UI layout (generated). */")
LN.append("#ifndef MIXTAPEDSI_UI_LAYOUT_H")
LN.append("#define MIXTAPEDSI_UI_LAYOUT_H")
LN.append("#define UI_PROG_R 12")
LN.append("#define UI_PROG_C0 11")
LN.append("#define UI_PROG_C1 30")
LN.append("#define UI_B_SHUF_X 36")
LN.append("#define UI_B_PREV_X 76")
LN.append("#define UI_B_PLAY_X 124")
LN.append("#define UI_B_NEXT_X 172")
LN.append("#define UI_B_REP_X 212")
LN.append("#define UI_B_Y 132")
LN.append("#define UI_B_R 16")
LN.append("#define UI_HEART_X0 220")
LN.append("#define UI_HEART_X1 244")
LN.append("#define UI_HEART_Y0 54")
LN.append("#define UI_HEART_Y1 78")
LN.append("#define UI_TAB_Y0 162")
LN.append("#define UI_TAB_Y1 190")
LN.append("#define UI_TAB0_X0 16")
LN.append("#define UI_TAB0_X1 64")
LN.append("#define UI_TAB1_X0 104")
LN.append("#define UI_TAB1_X1 152")
LN.append("#define UI_TAB2_X0 192")
LN.append("#define UI_TAB2_X1 240")
LN.append("#define UI_EQ_TOP_Y 0")
LN.append("#define UI_EQ_LX0 12")
LN.append("#define UI_EQ_RX0 214")
LN.append("#define UI_COVER_C0 2")
LN.append("#define UI_COVER_R0 7")
LN.append("#define UI_TILE_TRK_EMPTY " + str(bot_info["extra"]["trk_empty"]))
LN.append("#define UI_TILE_TRK_FILL " + str(bot_info["extra"]["trk_fill"]))
LN.append("#define UI_TILE_TRK_CAP_L " + str(bot_info["extra"]["trk_cap_l"]))
LN.append("#define UI_TILE_TRK_CAP_R " + str(bot_info["extra"]["trk_cap_r"]))
LN.append("#define UI_TILE_TRK_KNOB " + str(bot_info["extra"]["trk_knob"]))
LN.append("#define UI_TILE_TAB_DIM " + str(bot_info["extra"]["tab_dim"]))
LN.append("#define UI_TILE_TAB_ON " + str(bot_info["extra"]["tab_on"]))
LN.append("#define UI_TILE_EQ_EMPTY " + str(top_info["extra"]["eq_empty"]))
LN.append("#define UI_TILE_EQ_FULL " + str(top_info["extra"]["eq_full"]))
for h in range(1, 8):
    LN.append("#define UI_TILE_EQ_P%d " % h + str(top_info["extra"][f"eq_p{h}"]))
LN.append(uarr("uiCoverCells", cov_ids))
LN.append(uarr("uiShufOn", bot_info["extra"]["shuf_on"]))
LN.append(uarr("uiRepOff", bot_info["extra"]["rep_off"]))
LN.append(uarr("uiPauseTiles", bot_info["extra"]["pause"]))
LN.append(uarr("uiPlayTiles", bot_info["extra"]["play"]))
LN.append(uarr("uiHeartOn", bot_info["extra"]["heart_on"]))
LN.append("#endif")
open("assets/ui_layout.h", "w").write("\n".join(LN) + "\n")

# EQ bar sprite frames: 8 heights in 8x32 box, 4bpp, + green/purple pals
def bar_frame(h):
    px = []
    for y in range(32):
        for x in range(8):
            px.append(1 if (1 <= x <= 6 and y >= 32 - h) else 0)
    return px
frames = [bar_frame(h) for h in (0, 4, 8, 12, 16, 20, 24, 28)]
def pal16(rgb):
    r, g, b = rgb
    v = (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10)
    return [0, v] + [0] * 14
spr = ["/* EQ bar sprite frames 8x32 bottom-anchored, 4bpp */",
       "static const unsigned short eqBarPalG[16] = {" + ",".join(map(str, pal16((46, 204, 130)))) + "};",
       "static const unsigned short eqBarPalP[16] = {" + ",".join(map(str, pal16((150, 80, 220)))) + "};",
       "static const unsigned char eqBarFrames[8][128] = {"]
for f in frames:
    spr.append("{" + ",".join(str(f[i * 2] | (f[i * 2 + 1] << 4)) for i in range(128)) + "},")
spr += ["};"]
open("assets/ui_spr.h", "w").write("\n".join(spr) + "\n")
print(f"ui tiles: bot={bot_info['tiles']} top={top_info['tiles']}")

#!/usr/bin/env python3
"""MixtapeDSi v1.0.7 UI tileset generator ("Midnight Glass" theme).

No longer tied to the old demo PNGs. Emits:
  assets/ui_top.h   (lyric stage)
  assets/ui_bot.h   (player deck)
  assets/ui_spr.h   (EQ stub, build compat)
  assets/ui_layout.h

Hardware contract:
  palette 0-127 = text layers, 128-191 = UI art (8bpp BG3).
  map entries are plain tile ids; tiles hold palette values 128-191.
  Runtime EQ tiles are appended after the generated tiles by bg_backdrop.c.
"""
import math
import os

W, H = 256, 192
POFF = 128

# ---- palette: Midnight Glass (RGB888) ----
C_BG = (7, 16, 28)            # 1  deep space
C_BG2 = (10, 22, 38)          # 2  gradient mid
C_BG3 = (13, 28, 48)          # 3  gradient low
C_CARD = (14, 29, 46)         # 4  glass card
C_ACCENT = (46, 230, 255)     # 5  aqua
C_VIOLET = (155, 123, 255)    # 6  violet
C_DIVIDER = (36, 64, 92)      # 7  border / divider
C_TEXT = (232, 244, 255)      # 8  near-white
C_MUTED = (143, 176, 204)     # 9  secondary text
C_DIM = (94, 124, 153)        # 10 tertiary
C_TRACK = (27, 51, 72)        # 11 empty rail
C_DARK = (4, 10, 18)          # 12 icon ink
C_TAB_BG = (10, 24, 38)       # 13 tab bar
C_TAB_ACTIVE = (15, 58, 76)   # 14 active segment
C_ACCENT_DIM = (14, 143, 184) # 15
C_CARD_HI = (20, 43, 66)      # 16 card highlight / raised glass
C_GLOW = (13, 40, 60)         # 17 soft lyric glow
C_DISC = (9, 20, 34)          # 18 vinyl body
C_WHITE = (255, 255, 255)     # 19 knob core
C_ACCENT_SOFT = (26, 74, 99)  # 20

PAL = [
    (0, 0, 0),
    C_BG, C_BG2, C_BG3, C_CARD, C_ACCENT, C_VIOLET, C_DIVIDER,
    C_TEXT, C_MUTED, C_DIM, C_TRACK, C_DARK, C_TAB_BG, C_TAB_ACTIVE,
    C_ACCENT_DIM, C_CARD_HI, C_GLOW, C_DISC, C_WHITE, C_ACCENT_SOFT,
]

# 24-step vertical gradient (one step per 8px tile row) so the backdrop
# reads as a smooth dark-glass wash instead of three hard bands.
NGRAD = 24
GRAD_START = len(PAL)
for _i in range(NGRAD):
    _t = _i / (NGRAD - 1)
    PAL.append(tuple(int(round(a + (b - a) * _t)) for a, b in zip(C_BG, C_BG3)))
GLOW2 = len(PAL)
PAL.append((17, 52, 76))
GLOW3 = len(PAL)
PAL.append((22, 68, 94))

while len(PAL) < 64:
    PAL.append((0, 0, 0))

(BG, BG2, BG3, CARD, ACCENT, VIOLET, BORDER, TEXT, MUTED, DIM, TRACK,
 DARK, TABBG, TABACT, ACCENT_DIM, CARD_HI, GLOW, DISC, WHITE, ACC_SOFT) = (
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20)

# ---- layout (pixels; rows are 8px) ----
TOP_DIV_Y = 32
TOP_FDIV_Y = 156
TOP_PROG_R, TOP_PROG_C0, TOP_PROG_C1 = 20, 3, 28
TOP_PROG_Y = 164
TOP_EQL = (2, 1, 2, 2)
TOP_EQR = (28, 1, 2, 2)
TOP_EQ_L = [5, 9, 7, 11]
TOP_EQ_R = [11, 7, 10, 5]

BOT_DIV_Y = 26
BOT_CARD = (8, 30, 247, 111)
ART_RC = (18, 40, 82, 104)
ART_CXY = (50, 72)
PILL_RC = (92, 86, 154, 98)
BOT_PROG_R, BOT_PROG_C0, BOT_PROG_C1 = 15, 1, 30
BOT_EQ = (22, 0, 3, 3)
BOT_EQ_SHAPE = [5, 10, 13, 7, 11, 6]

BTN_Y = 160
BTN_HERO = (128, 160)
BTN_SIDE = [(32, 160), (88, 160), (168, 160), (224, 160)]
BTN_CELLS = {
    "shuf": (3, 19, 2, 2),
    "prev": (10, 19, 2, 2),
    "play": (14, 18, 4, 4),
    "next": (20, 19, 2, 2),
    "rep": (27, 19, 2, 2),
}
HEART_C0, HEART_R0, HEART_W, HEART_H = 28, 1, 2, 2

TAB_Y0 = 176
TAB_CELLS = [(1, 22, 9, 2), (11, 22, 9, 2), (21, 22, 9, 2)]
TAB_TEXT_R = 22
TAB_TEXT_X = [2, 14, 23]

# list / panel views replace the deck card interior with a clean glass panel
PANEL_C0, PANEL_C1 = 1, 30
PANEL_R0, PANEL_R1 = 4, 13
LIST_Y0, LIST_ROW_H, LIST_ROWS = 32, 16, 5


def new_img(fill=BG):
    return [[fill for _ in range(W)] for _ in range(H)]


def grad_idx(y_px):
    """Palette index for the gradient band containing this pixel row."""
    return GRAD_START + min(NGRAD - 1, max(0, y_px) * NGRAD // H)


def px(img, x, y, c):
    if 0 <= x < W and 0 <= y < H:
        img[y][x] = c


def hline(img, x0, y, x1, c):
    for x in range(x0, x1 + 1):
        px(img, x, y, c)


def vline(img, x, y0, y1, c):
    for y in range(y0, y1 + 1):
        px(img, x, y, c)


def rect(img, x0, y0, x1, y1, fill=None, outline=None):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            edge = x == x0 or x == x1 or y == y0 or y == y1
            if edge and outline is not None:
                px(img, x, y, outline)
            elif not edge and fill is not None:
                px(img, x, y, fill)
            elif edge and fill is not None:
                px(img, x, y, fill)


def rounded_rect(img, x0, y0, x1, y1, r, fill=None, outline=None):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            cx = min(max(x, x0 + r), x1 - r)
            cy = min(max(y, y0 + r), y1 - r)
            d = math.hypot(x - cx, y - cy)
            edge = d > r - 0.8 or x == x0 or x == x1 or y == y0 or y == y1
            if edge and outline is not None:
                px(img, x, y, outline)
            elif not edge and fill is not None:
                px(img, x, y, fill)
            elif edge and fill is not None:
                px(img, x, y, fill)


def circle(img, cx, cy, r, fill=None, outline=None):
    for y in range(int(cy - r - 1), int(cy + r + 2)):
        for x in range(int(cx - r - 1), int(cx + r + 2)):
            d = math.hypot(x - cx, y - cy)
            if outline is not None and abs(d - r) < 0.7:
                px(img, x, y, outline)
            elif fill is not None and d <= r:
                px(img, x, y, fill)


def heart(img, cx, cy, color, filled):
    pat = ["..##.##..", ".#######.", "#########", "#########",
           ".#######.", "..#####..", "...###...", "....#...."]
    if not filled:
        pat = ["..##.##..", ".#..#..#.", "#...#...#", "#.......#",
               ".#.....#.", "..#...#..", "...#.#...", "....#...."]
    for r, row in enumerate(pat):
        for c, ch in enumerate(row):
            if ch == "#":
                px(img, cx - 4 + c, cy - 4 + r, color)


def g_shuffle(img, cx, cy, color):
    for i in range(7):
        px(img, cx - 4 + i, cy - 3 + i, color)
        px(img, cx - 4 + i, cy + 3 - i, color)
    for d in range(3):
        px(img, cx + 2 + d, cy - 3, color)
        px(img, cx + 4, cy - 3 + d, color)
        px(img, cx + 2 + d, cy + 3, color)
        px(img, cx + 4, cy + 3 - d, color)


def g_prev(img, cx, cy, color):
    for y in range(cy - 4, cy + 5):
        px(img, cx - 4, y, color)
    for y in range(cy - 3, cy + 4):
        h = 3 - abs(y - cy) // 1
        for dx in range(h + 1):
            px(img, cx - 1 - dx, y, color)
            px(img, cx + 3 - dx, y, color)


def g_next(img, cx, cy, color):
    for y in range(cy - 4, cy + 5):
        px(img, cx + 4, y, color)
    for y in range(cy - 3, cy + 4):
        h = 3 - abs(y - cy) // 1
        for dx in range(h + 1):
            px(img, cx + 1 + dx, y, color)
            px(img, cx - 3 + dx, y, color)


def g_repeat(img, cx, cy, color):
    for x in range(cx - 4, cx + 5):
        px(img, x, cy - 4, color)
        px(img, x, cy + 4, color)
    for y in range(cy - 3, cy + 4):
        px(img, cx - 5, y, color)
        px(img, cx + 5, y, color)
    px(img, cx + 3, cy - 6, color)
    px(img, cx + 4, cy - 5, color)
    px(img, cx + 5, cy - 4, color)
    px(img, cx + 4, cy - 3, color)
    px(img, cx + 3, cy - 2, color)


def side_button(img, cx, cy, kind, color):
    circle(img, cx, cy, 10, fill=CARD_HI, outline=BORDER)
    circle(img, cx, cy, 9, fill=CARD_HI)
    if kind == "shuf":
        g_shuffle(img, cx, cy, color)
    elif kind == "prev":
        g_prev(img, cx, cy, color)
    elif kind == "next":
        g_next(img, cx, cy, color)
    else:
        g_repeat(img, cx, cy, color)


def hero(img, cx, cy, playing, ring, fill, icon):
    circle(img, cx, cy, 20, fill=ring)
    circle(img, cx, cy, 18, fill=fill)
    if playing:
        for y in range(cy - 6, cy + 7):
            for x in (cx - 5, cx - 4, cx + 3, cx + 4):
                px(img, x, y, icon)
    else:
        for y in range(cy - 7, cy + 8):
            h = 7 - abs(y - cy)
            for x in range(cx - 4, cx - 4 + h + 1):
                px(img, x, y, icon)


def vinyl(img):
    x0, y0, x1, y1 = ART_RC
    rounded_rect(img, x0, y0, x1, y1, 8, fill=DARK, outline=BORDER)
    cx, cy = ART_CXY
    circle(img, cx, cy, 28, fill=DISC, outline=BORDER)
    circle(img, cx, cy, 21, outline=TRACK)
    circle(img, cx, cy, 15, outline=TRACK)
    circle(img, cx, cy, 9, fill=TABACT, outline=ACCENT_DIM)
    circle(img, cx, cy, 2, fill=BG)
    # sheen arc (top-left)
    for a in range(200, 251, 2):
        r = 23.5
        px(img, int(cx + r * math.cos(math.radians(a))),
           int(cy - r * math.sin(math.radians(a))), ACCENT)
    # small center glint
    px(img, cx - 3, cy - 3, ACCENT)
    px(img, cx - 2, cy - 4, ACCENT)


def gradient(img):
    for y in range(H):
        c = grad_idx(y)
        for x in range(W):
            img[y][x] = c


def build_top():
    img = new_img(BG)
    gradient(img)
    # three nested ellipses fake a soft radial glow behind the lyric band
    for rx, ry, c in [(112, 34, GLOW), (88, 27, GLOW2), (62, 20, GLOW3)]:
        for y in range(80, 145):
            for x in range(20, 236):
                if ((x - 128) / float(rx)) ** 2 + ((y - 112) / float(ry)) ** 2 < 1.0:
                    img[y][x] = c
    # header / footer rules
    hline(img, 12, TOP_DIV_Y, 243, BORDER)
    hline(img, 112, TOP_DIV_Y, 143, ACCENT)
    hline(img, 12, TOP_FDIV_Y, 243, BORDER)
    hline(img, 112, TOP_FDIV_Y, 143, VIOLET)
    # progress rail base (dynamic tiles cover row 20)
    hline(img, 24, TOP_PROG_Y, 231, TRACK)
    hline(img, 24, TOP_PROG_Y + 1, 231, TRACK)
    return img


def build_bot():
    img = new_img(BG)
    gradient(img)
    # header
    vline(img, 3, 8, 18, ACCENT)
    hline(img, 8, BOT_DIV_Y, 247, BORDER)
    hline(img, 8, BOT_DIV_Y, 48, ACCENT)
    # main glass card
    rounded_rect(img, *BOT_CARD, r=10, fill=CARD, outline=BORDER)
    hline(img, 26, BOT_CARD[1] + 1, 230, CARD_HI)
    vline(img, BOT_CARD[0] + 1, 36, 105, ACCENT_DIM)
    vinyl(img)
    # quality pill
    rounded_rect(img, *PILL_RC, r=5, fill=TABACT, outline=ACCENT_DIM)
    # transport wells (icon art arrives as dynamic tiles)
    for cx, cy in [BTN_SIDE[0], BTN_SIDE[1], BTN_SIDE[2], BTN_SIDE[3]]:
        circle(img, cx, cy, 11, outline=TRACK)
    circle(img, *BTN_HERO, 22, outline=TRACK)
    # segmented tab rail
    rounded_rect(img, 8, TAB_Y0, 248, 191, r=6, fill=TABBG, outline=BORDER)
    for i in range(3):
        c0, r0, cw, ch = TAB_CELLS[i]
        # inactive segment glass
        sx0, sy0 = c0 * 8 + 1, r0 * 8 + 1
        sx1, sy1 = (c0 + cw) * 8 - 2, (r0 + ch) * 8 - 2
        rounded_rect(img, sx0, sy0, sx1, sy1, r=5, fill=TABBG)
    return img


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
    return tuple(grid[y][x] + POFF
                 for y in range(ty * 8, ty * 8 + 8)
                 for x in range(tx * 8, tx * 8 + 8))


def emit(name, grid, dynamic, extras_fn):
    em = Emitter()
    amap = [0] * 768
    for ty in range(24):
        for tx in range(32):
            if (tx, ty) in dynamic:
                amap[ty * 32 + tx] = len(em.tiles)
                em.tiles.append(cell_tup(grid, tx, ty))
    for ty in range(24):
        for tx in range(32):
            if (tx, ty) not in dynamic:
                amap[ty * 32 + tx] = em.add(cell_tup(grid, tx, ty))
    extras_fn(em, grid)
    assert len(em.tiles) <= 640, (name, len(em.tiles))
    pal16 = [(p[0] >> 3) | ((p[1] >> 3) << 5) | ((p[2] >> 3) << 10) for p in PAL]
    full = [0] * 256
    full[POFF:POFF + len(pal16)] = pal16
    L = ["/* MixtapeDSi v1.0.7 UI (%s) - generated, see tools/make_ui.py */" % name,
         "static const unsigned short %sPal[256] = {%s};" % (name, ",".join(map(str, full))),
         "static const unsigned int %sPalLen = 512;" % name,
         "static const unsigned char %sTiles[%d] = {" % (name, len(em.tiles) * 64)]
    for t in em.tiles:
        L.append(",".join(map(str, t)) + ",")
    L += ["};",
          "static const unsigned int %sTilesLen = %d;" % (name, len(em.tiles) * 64),
          "static const unsigned short %sMap[768] = {%s};" % (name, ",".join(map(str, amap))),
          "static const unsigned int %sMapLen = 1536;" % name]
    return "\n".join(L) + "\n", {"tiles": len(em.tiles), "extra": em.extra}


def eq_block(width, height, bar_hs, base_py, x_off=0, peak=None):
    g = [[BG] * width for _ in range(height)]
    for i, h in enumerate(bar_hs):
        bx = x_off + i * 3
        for py in range(height):
            d = base_py - py
            if 0 <= d < h:
                c = peak if (peak is not None and d == h - 1) else ACCENT
                for dx in (0, 1):
                    if 0 <= bx + dx < width:
                        g[py][bx + dx] = c
    return g


def block_from_grid(em, g, w_cells, h_cells):
    ids = []
    for cy in range(h_cells):
        for cx in range(w_cells):
            ids.append(em.add(tuple(g[cy * 8 + py][cx * 8 + px] + POFF
                                    for py in range(8) for px in range(8))))
    return ids


def block_ids(em, grid, c0, r0, w, h):
    return [em.add(cell_tup(grid, c0 + c, r0 + r)) for r in range(h) for c in range(w)]


def top_extras(em, grid):
    for lvl in range(9):
        hl = [min(8, (h * lvl + 4) // 8) for h in TOP_EQ_L]
        hr = [min(8, (h * lvl + 4) // 8) for h in TOP_EQ_R]
        em.extra["eqL_%d" % lvl] = block_from_grid(
            em, eq_block(16, 16, hl, 14, 0, VIOLET), 2, 2)
        em.extra["eqR_%d" % lvl] = block_from_grid(
            em, eq_block(16, 16, hr, 14, 4, VIOLET), 2, 2)

    def tseg(kind):
        bgc = grad_idx(TOP_PROG_R * 8)
        t = []
        for y in range(8):
            for x in range(8):
                v = bgc
                if 3 <= y <= 4:
                    v = TRACK
                    if kind in ("fill", "knob"):
                        v = ACCENT
                if kind == "knob" and 2 <= x <= 4 and 1 <= y <= 6:
                    v = TEXT if (2 < y < 6 or x == 3) else ACCENT
                t.append(v + POFF)
        return tuple(t)
    em.add_extra("ttrk_empty", tseg("empty"))
    em.add_extra("ttrk_fill", tseg("fill"))
    em.add_extra("ttrk_knob", tseg("knob"))


def bot_extras(em, grid):
    # two-row seek rail: top tile then bottom tile, states consecutive
    def rail(kind, row):
        bgc = grad_idx((BOT_PROG_R + row) * 8)
        t = []
        for y in range(8):
            for x in range(8):
                gy = y if row == 0 else y + 8
                v = bgc
                if 4 <= gy <= 13:
                    v = TRACK
                    if kind in ("fill", "knob"):
                        v = ACCENT
                if kind == "knob":
                    # 12px capsule spanning both rail rows
                    if 2 <= gy <= 13 and 1 <= x <= 6:
                        v = ACCENT
                    if 3 <= gy <= 12 and 2 <= x <= 5:
                        v = WHITE
                t.append(v + POFF)
        return tuple(t)
    em.add_extra("trk_empty_top", rail("empty", 0))
    em.add_extra("trk_empty_bot", rail("empty", 1))
    em.add_extra("trk_fill_top", rail("fill", 0))
    em.add_extra("trk_fill_bot", rail("fill", 1))
    em.add_extra("trk_knob_top", rail("knob", 0))
    em.add_extra("trk_knob_bot", rail("knob", 1))

    base = [row[:] for row in grid]

    def paint(fn):
        tmp = [row[:] for row in base]
        fn(tmp)
        return tmp

    # ---- transport ----
    t = paint(lambda g: side_button(g, 32, BTN_Y, "shuf", MUTED))
    em.extra["shuf_off"] = block_ids(em, t, *BTN_CELLS["shuf"])
    t = paint(lambda g: side_button(g, 32, BTN_Y, "shuf", ACCENT))
    em.extra["shuf_on"] = block_ids(em, t, *BTN_CELLS["shuf"])
    t = paint(lambda g: side_button(g, 88, BTN_Y, "prev", TEXT))
    em.extra["prev_ids"] = block_ids(em, t, *BTN_CELLS["prev"])
    t = paint(lambda g: side_button(g, 168, BTN_Y, "next", TEXT))
    em.extra["next_ids"] = block_ids(em, t, *BTN_CELLS["next"])
    t = paint(lambda g: side_button(g, 224, BTN_Y, "rep", MUTED))
    em.extra["rep_off"] = block_ids(em, t, *BTN_CELLS["rep"])
    t = paint(lambda g: side_button(g, 224, BTN_Y, "rep", ACCENT))
    em.extra["rep_on"] = block_ids(em, t, *BTN_CELLS["rep"])
    t = paint(lambda g: hero(g, 128, BTN_Y, False, ACCENT, ACCENT, DARK))
    em.extra["play_ids"] = block_ids(em, t, *BTN_CELLS["play"])
    t = paint(lambda g: hero(g, 128, BTN_Y, True, ACCENT, ACCENT, DARK))
    em.extra["pause_ids"] = block_ids(em, t, *BTN_CELLS["play"])

    # ---- heart ----
    t = paint(lambda g: heart(g, 235, 16, MUTED, False))
    em.extra["heart_off"] = block_ids(em, t, HEART_C0, HEART_R0, HEART_W, HEART_H)
    t = paint(lambda g: heart(g, 235, 16, ACCENT, True))
    em.extra["heart_on"] = block_ids(em, t, HEART_C0, HEART_R0, HEART_W, HEART_H)

    # ---- segmented tabs: each segment as a complete 9x2 block ----
    for seg, (c0, r0, cw, ch) in enumerate(TAB_CELLS):
        for state in ("on", "off"):
            t = paint(lambda g, seg=seg, state=state: _tab_seg(g, seg, state == "on"))
            em.extra["tab%d_%s" % (seg, state)] = block_ids(em, t, c0, r0, cw, ch)

    # ---- panel tiles: flat card interior for Library/Queue/Home/Settings ----
    def panel_tile(left=False, right=False, bottom=False):
        t = []
        for y in range(8):
            for x in range(8):
                v = CARD
                if bottom and y == 7:
                    v = BORDER
                if left and x == 0:
                    v = BORDER
                if right and x == 7:
                    v = BORDER
                t.append(v + POFF)
        return tuple(t)
    em.add_extra("panel_body", panel_tile())
    em.add_extra("panel_left", panel_tile(left=True))
    em.add_extra("panel_right", panel_tile(right=True))
    em.add_extra("panel_bottom", panel_tile(bottom=True))
    em.add_extra("panel_bottom_left", panel_tile(left=True, bottom=True))
    em.add_extra("panel_bottom_right", panel_tile(right=True, bottom=True))

    em.add_extra("blank", tuple([(BG + POFF)] * 64))


def _tab_seg(grid, seg, active):
    c0, r0, cw, ch = TAB_CELLS[seg]
    x0, y0 = c0 * 8 + 1, r0 * 8 + 1
    x1, y1 = (c0 + cw) * 8 - 2, (r0 + ch) * 8 - 2
    if active:
        rounded_rect(grid, x0, y0, x1, y1, r=5, fill=TABACT, outline=ACCENT_DIM)
        hline(grid, x0 + 6, y0 + 1, x1 - 6, ACCENT)
    else:
        rounded_rect(grid, x0, y0, x1, y1, r=5, fill=TABBG)
    return grid


grid_top = build_top()
grid_bot = build_bot()

dyn_top = {}
for _ty in range(TOP_EQL[1], TOP_EQL[1] + TOP_EQL[3]):
    for _tx in range(TOP_EQL[0], TOP_EQL[0] + TOP_EQL[2]):
        dyn_top[(_tx, _ty)] = "eqL"
for _ty in range(TOP_EQR[1], TOP_EQR[1] + TOP_EQR[3]):
    for _tx in range(TOP_EQR[0], TOP_EQR[0] + TOP_EQR[2]):
        dyn_top[(_tx, _ty)] = "eqR"
for _tx in range(TOP_PROG_C0, TOP_PROG_C1 + 1):
    dyn_top[(_tx, TOP_PROG_R)] = "tprog"

dyn_bot = {}
for _row in (BOT_PROG_R, BOT_PROG_R + 1):
    for _tx in range(BOT_PROG_C0, BOT_PROG_C1 + 1):
        dyn_bot[(_tx, _row)] = "prog"
for _key, (_c0, _r0, _w, _h) in BTN_CELLS.items():
    for _r in range(_h):
        for _c in range(_w):
            dyn_bot[(_c0 + _c, _r0 + _r)] = _key
for _r in range(HEART_H):
    for _c in range(HEART_W):
        dyn_bot[(HEART_C0 + _c, HEART_R0 + _r)] = "heart"
_ec0, _er0, _ew, _eh = BOT_EQ
for _r in range(_eh):
    for _c in range(_ew):
        dyn_bot[(_ec0 + _c, _er0 + _r)] = "eqh"
for _i, (_c0, _r0, _cw, _ch) in enumerate(TAB_CELLS):
    for _r in range(_ch):
        for _c in range(_cw):
            dyn_bot[(_c0 + _c, _r0 + _r)] = "tab%d" % _i

top_h, top_info = emit("ui_top", grid_top, dyn_top, top_extras)
bot_h, bot_info = emit("ui_bot", grid_bot, dyn_bot, bot_extras)

os.makedirs("assets", exist_ok=True)
open("assets/ui_top.h", "w").write(top_h)
open("assets/ui_bot.h", "w").write(bot_h)


def uarr(name, ids):
    return ("static const unsigned short " + name + "[" + str(len(ids)) +
            "] = {" + ",".join(map(str, ids)) + "};")


LN = ["/* MixtapeDSi v1.0.7 UI layout (generated). */",
      "#ifndef MIXTAPEDSI_UI_LAYOUT_H",
      "#define MIXTAPEDSI_UI_LAYOUT_H"]
d = [
    ("UI_PROG_R", BOT_PROG_R), ("UI_PROG_C0", BOT_PROG_C0), ("UI_PROG_C1", BOT_PROG_C1),
    ("UI_TPROG_R", TOP_PROG_R), ("UI_TPROG_C0", TOP_PROG_C0), ("UI_TPROG_C1", TOP_PROG_C1),
    ("UI_B_SHUF_X", 32), ("UI_B_PREV_X", 88), ("UI_B_PLAY_X", 128),
    ("UI_B_NEXT_X", 168), ("UI_B_REP_X", 224), ("UI_B_Y", BTN_Y), ("UI_B_R", 22),
    ("UI_RC_SHUF_X0", 3), ("UI_RC_PREV_X0", 10), ("UI_RC_PLAY_X0", 14),
    ("UI_RC_NEXT_X0", 20), ("UI_RC_REP_X0", 27),
    ("UI_RC_Y0", 19), ("UI_RC_PLAY_Y0", 18),
    ("UI_RC_SIDE_W", 2), ("UI_RC_SIDE_H", 2),
    ("UI_RC_PLAY_W", 4), ("UI_RC_PLAY_H", 4),
    ("UI_HEART_X0", 224), ("UI_HEART_X1", 240),
    ("UI_HEART_Y0", 8), ("UI_HEART_Y1", 24),
    ("UI_HEART_C0", HEART_C0), ("UI_HEART_R0", HEART_R0),
    ("UI_TAB_Y0", TAB_Y0), ("UI_TAB_Y1", 191),
    ("UI_TAB0_X0", 0), ("UI_TAB0_X1", 85),
    ("UI_TAB1_X0", 86), ("UI_TAB1_X1", 170),
    ("UI_TAB2_X0", 171), ("UI_TAB2_X1", 255),
    ("UI_TAB_R0", TAB_CELLS[0][1]), ("UI_TAB_W", TAB_CELLS[0][2]),
    ("UI_TAB_H", TAB_CELLS[0][3]),
    ("UI_TAB0_C0", TAB_CELLS[0][0]), ("UI_TAB1_C0", TAB_CELLS[1][0]),
    ("UI_TAB2_C0", TAB_CELLS[2][0]),
    ("UI_TAB_TEXT_R", TAB_TEXT_R),
    ("UI_TAB0_TX", TAB_TEXT_X[0]), ("UI_TAB1_TX", TAB_TEXT_X[1]),
    ("UI_TAB2_TX", TAB_TEXT_X[2]),
    ("UI_CARD_X0", BOT_CARD[0]), ("UI_CARD_Y0", BOT_CARD[1]),
    ("UI_CARD_X1", BOT_CARD[2]), ("UI_CARD_Y1", BOT_CARD[3]),
    ("UI_LIST_Y0", LIST_Y0), ("UI_LIST_ROW_H", LIST_ROW_H),
    ("UI_LIST_ROWS", LIST_ROWS),
    ("UI_PANEL_C0", PANEL_C0), ("UI_PANEL_C1", PANEL_C1),
    ("UI_PANEL_R0", PANEL_R0), ("UI_PANEL_R1", PANEL_R1),
    ("UI_EQ_TOP_L_C0", TOP_EQL[0]), ("UI_EQ_TOP_L_R0", TOP_EQL[1]),
    ("UI_EQ_TOP_R_C0", TOP_EQR[0]), ("UI_EQ_TOP_W", TOP_EQL[2]),
    ("UI_EQ_TOP_H", TOP_EQL[3]),
    ("UI_EQ_BOT_C0", BOT_EQ[0]), ("UI_EQ_BOT_R0", BOT_EQ[1]),
    ("UI_EQ_BOT_W", BOT_EQ[2]), ("UI_EQ_BOT_H", BOT_EQ[3]),
    ("UI_GRAD_START", GRAD_START), ("UI_GRAD_N", NGRAD),
    ("UI_SEEK_X0", 8), ("UI_SEEK_X1", 247), ("UI_SEEK_Y0", 116), ("UI_SEEK_Y1", 143),
    ("UI_TILE_TRK_EMPTY_TOP", bot_info["extra"]["trk_empty_top"]),
    ("UI_TILE_TRK_EMPTY_BOT", bot_info["extra"]["trk_empty_bot"]),
    ("UI_TILE_TRK_FILL_TOP", bot_info["extra"]["trk_fill_top"]),
    ("UI_TILE_TRK_FILL_BOT", bot_info["extra"]["trk_fill_bot"]),
    ("UI_TILE_TRK_KNOB_TOP", bot_info["extra"]["trk_knob_top"]),
    ("UI_TILE_TRK_KNOB_BOT", bot_info["extra"]["trk_knob_bot"]),
    ("UI_TILE_TTRK_EMPTY", top_info["extra"]["ttrk_empty"]),
    ("UI_TILE_TTRK_FILL", top_info["extra"]["ttrk_fill"]),
    ("UI_TILE_TTRK_KNOB", top_info["extra"]["ttrk_knob"]),
    ("UI_TILE_BLANK", bot_info["extra"]["blank"]),
]
for k, v in d:
    LN.append("#define %s %d" % (k, v))
LN.append(uarr("uiPanelTiles", [
    bot_info["extra"]["panel_body"], bot_info["extra"]["panel_left"],
    bot_info["extra"]["panel_right"], bot_info["extra"]["panel_bottom"],
    bot_info["extra"]["panel_bottom_left"],
    bot_info["extra"]["panel_bottom_right"]]))
LN.append(uarr("uiShufOn", bot_info["extra"]["shuf_on"]))
LN.append(uarr("uiShufOff", bot_info["extra"]["shuf_off"]))
LN.append(uarr("uiPrevIds", bot_info["extra"]["prev_ids"]))
LN.append(uarr("uiNextIds", bot_info["extra"]["next_ids"]))
LN.append(uarr("uiPauseTiles", bot_info["extra"]["pause_ids"]))
LN.append(uarr("uiPlayTiles", bot_info["extra"]["play_ids"]))
LN.append(uarr("uiRepOn", bot_info["extra"]["rep_on"]))
LN.append(uarr("uiRepOff", bot_info["extra"]["rep_off"]))
LN.append(uarr("uiHeartOn", bot_info["extra"]["heart_on"]))
LN.append(uarr("uiHeartOff", bot_info["extra"]["heart_off"]))
for i in range(3):
    LN.append(uarr("uiTab%dOn" % i, bot_info["extra"]["tab%d_on" % i]))
    LN.append(uarr("uiTab%dOff" % i, bot_info["extra"]["tab%d_off" % i]))
LN.append("#endif")
open("assets/ui_layout.h", "w").write("\n".join(LN) + "\n")

spr = ["/* MixtapeDSi EQ sprite stub (kept for build compat) */",
       "static const unsigned short eqBarPalG[16] = {0};",
       "static const unsigned short eqBarPalP[16] = {0};",
       "static const unsigned char eqBarFrames[8][128] = {{0}};"]
open("assets/ui_spr.h", "w").write("\n".join(spr) + "\n")
print("ui tiles: bot=%d top=%d" % (bot_info["tiles"], top_info["tiles"]))

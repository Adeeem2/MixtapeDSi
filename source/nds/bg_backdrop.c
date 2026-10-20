/* MixtapeDSi UI engine (ARM9): "Midnight Glass" deck.
 *
 * Layers (both screens):
 *   BG0 4bpp  console text (8px Unscii-8)      tile base 0,  map 31
 *   BG1 4bpp  16px lyric text (Mixtape Sans)   tile base 4,  map 26
 *   BG3 8bpp  static art + dynamic tiles       tile base 1,  map 28
 *
 * BG1 sits at 64 KB so it can never overlap the bg3 art tiles or the
 * runtime EQ tiles appended after them (tile ids < 768).
 * Palette banks: 0-4 console text, 5-7 16px text, 128-191 UI art. */
#ifdef ARM9

#include <nds.h>
#include <stdio.h>
#include <string.h>
#include "ui_top.h"
#include "ui_bot.h"
#include "ui_layout.h"
#include "font_ui.h"
#include "font_lyr.h"

/* palette banks for the 16px layer */
#define BIG_BANK_INK   5
#define BIG_BANK_AQUA  6
#define BIG_BANK_MUTED 7

/* runtime tile slots inside the art BG (8bpp, tile base 1).
 * Placed after the generated tiles, well below tile 768 (offset 0x10000,
 * where the 16px text layer starts). */
#define TOP_TILE0  (ui_topTilesLen / 64)
#define BOT_TILE0  (ui_botTilesLen / 64)
#define EQ_TOP_L_BASE (TOP_TILE0)
#define EQ_TOP_R_BASE (TOP_TILE0 + 8 * 4)
#define EQ_BOT_BASE   (BOT_TILE0)

/* palette values used by runtime-drawn tiles (art base 128) */
#define C_ACCENT_T 133   /* PAL index 5: aqua */
#define C_VI_T     134   /* PAL index 6: violet */

/* gradient band for an absolute pixel row (matches make_ui.py grad_idx) */
static unsigned char eq_bg_color(int y_px) {
    if (y_px < 0) y_px = 0;
    if (y_px > 191) y_px = 191;
    return (unsigned char)(128 + UI_GRAD_START +
                           (y_px * UI_GRAD_N) / 192);
}

static int s_bg_main = -1, s_bg_sub = -1;
static int s_bg_top_big = -1, s_bg_bot_big = -1;
static unsigned short *s_top_map = NULL, *s_bot_map = NULL;
static unsigned char *s_top_gfx = NULL, *s_bot_gfx = NULL;
static unsigned short *s_top_text = NULL, *s_bot_text = NULL;
static unsigned short *s_top_big = NULL, *s_bot_big = NULL;
/* pristine maps: EQ disable and the list panel restore from these */
static unsigned short s_top_base[768];
static unsigned short s_bot_base[768];

/* likes */
#define MAX_LIKES 64
static char s_likes[MAX_LIKES][256];
static int s_like_count = 0;

static void map_set(unsigned short *map, int tx, int ty, unsigned short tile) {
    if (tx >= 0 && tx < 32 && ty >= 0 && ty < 24)
        map[ty * 32 + tx] = tile;
}

static void map_block(unsigned short *map, int x0, int y0, int w, int h,
                      const unsigned short *tiles) {
    for (int r = 0; r < h; r++)
        for (int c = 0; c < w; c++)
            map_set(map, x0 + c, y0 + r, tiles[r * w + c]);
}

static void load_one(int main_screen, const unsigned short *pal,
                     const unsigned char *tiles, unsigned int tiles_len,
                     const unsigned short *map) {
    int id;
    if (main_screen) {
        id = bgInit(3, BgType_Text8bpp, BgSize_T_256x256, 28, 1);
        s_bg_main = id;
        dmaCopy(tiles, bgGetGfxPtr(id), tiles_len);
        dmaCopy(map, bgGetMapPtr(id), 32 * 24 * 2);
        /* UI art owns palette bank 128-191 only: entries 0-127 belong to
         * the text layers (brand text colors, set in ui_font_init). */
        dmaCopy(&pal[128], BG_PALETTE + 128, 64 * 2);
        bgSetPriority(id, 3);
        s_top_map = bgGetMapPtr(id);
        s_top_gfx = (unsigned char *)bgGetGfxPtr(id);
    } else {
        id = bgInitSub(3, BgType_Text8bpp, BgSize_T_256x256, 28, 1);
        s_bg_sub = id;
        dmaCopy(tiles, bgGetGfxPtr(id), tiles_len);
        dmaCopy(map, bgGetMapPtr(id), 32 * 24 * 2);
        dmaCopy(&pal[128], BG_PALETTE_SUB + 128, 64 * 2);
        bgSetPriority(id, 3);
        s_bot_map = bgGetMapPtr(id);
        s_bot_gfx = (unsigned char *)bgGetGfxPtr(id);
    }
}

/* ---- 16px text layer (BG1, 4bpp, Mixtape Sans from assets/font_lyr.h) ---- */
static void big_init(int main_screen) {
    int id;
    unsigned short *map;
    unsigned char *gfx;
    if (main_screen) {
        id = bgInit(1, BgType_Text4bpp, BgSize_T_256x256, 26, 4);
        s_top_big = bgGetMapPtr(id);
        s_bg_top_big = id;
        map = s_top_big;
        gfx = (unsigned char *)bgGetGfxPtr(id);
    } else {
        id = bgInitSub(1, BgType_Text4bpp, BgSize_T_256x256, 26, 4);
        s_bot_big = bgGetMapPtr(id);
        s_bg_bot_big = id;
        map = s_bot_big;
        gfx = (unsigned char *)bgGetGfxPtr(id);
    }
    memset(map, 0, 32 * 32 * sizeof(*map));
    dmaCopy(fontLyrTiles, gfx, fontLyrTilesLen);
    bgSetPriority(id, 1);
}

static void big_palette(unsigned short *pal) {
    pal[BIG_BANK_INK * 16 + 0] = 0;
    pal[BIG_BANK_INK * 16 + 1] = (unsigned short)(29 | (30 << 5) | (31 << 10));
    pal[BIG_BANK_AQUA * 16 + 0] = 0;
    pal[BIG_BANK_AQUA * 16 + 1] = (unsigned short)(5 | (28 << 5) | (31 << 10));
    pal[BIG_BANK_MUTED * 16 + 0] = 0;
    pal[BIG_BANK_MUTED * 16 + 1] = (unsigned short)(18 | (23 << 5) | (30 << 10));
}

/* Unscii-8 interface typeface (public domain). Finished UI text is composed
 * directly into the console map by ui_text(). */
void ui_font_init(PrintConsole *con_top, PrintConsole *con_bot) {
    static ConsoleFont mixtape_font = {
        (u16 *)fontUiTiles, /* gfx */
        0,                 /* pal */
        0,                 /* numColors */
        4,                 /* bpp */
        0,                 /* asciiOffset */
        256,               /* numChars */
        true,              /* convertSingleColor */
    };
    if (con_top) consoleSetFont(con_top, &mixtape_font);
    if (con_bot) consoleSetFont(con_bot, &mixtape_font);
    if (con_top) s_top_text = bgGetMapPtr(con_top->bgId);
    if (con_bot) s_bot_text = bgGetMapPtr(con_bot->bgId);

    /* Brand text palette, Midnight Glass cool set:
     * ice white, aqua, steel blue, violet, dim slate.
     *
     * consoleLoadFont() rewrites the font to single-colour nibbles (0xF),
     * so the visible entry is bank*16 + 15. It also stamps ANSI colours
     * over every 16th entry (15, 31, ... 159 ...), which would corrupt
     * the art palette. Write both candidate entries, then reload the art
     * palette so entries 128-191 are authoritative. */
    {
        static const unsigned short text_pal[5] = {
            (unsigned short)(29 | (30 << 5) | (31 << 10)), /* ink    */
            (unsigned short)(5 | (28 << 5) | (31 << 10)),  /* aqua   */
            (unsigned short)(18 | (23 << 5) | (30 << 10)), /* muted  */
            (unsigned short)(19 | (15 << 5) | (31 << 10)), /* violet */
            (unsigned short)(13 | (17 << 5) | (22 << 10)), /* dim    */
        };
        for (int b = 0; b < 5; b++) {
            BG_PALETTE[b * 16 + 1] = text_pal[b];
            BG_PALETTE[b * 16 + 15] = text_pal[b];
            BG_PALETTE_SUB[b * 16 + 1] = text_pal[b];
            BG_PALETTE_SUB[b * 16 + 15] = text_pal[b];
        }
    }

    big_palette(BG_PALETTE);
    big_palette(BG_PALETTE_SUB);

    /* consoleSetFont() clobbers every 16th entry with ANSI colours; the
     * art palette owns 128-191, so restore it last. */
    dmaCopy(&ui_topPal[128], BG_PALETTE + 128, 64 * 2);
    dmaCopy(&ui_botPal[128], BG_PALETTE_SUB + 128, 64 * 2);
}

static unsigned short *text_map(int top) {
    return top ? s_top_text : s_bot_text;
}

static unsigned short *big_map(int top) {
    return top ? s_top_big : s_bot_big;
}

void ui_text_clear(int top) {
    unsigned short *map = text_map(top);
    unsigned short *big = big_map(top);
    if (map) memset(map, 0, 32 * 24 * sizeof(*map));
    if (big) memset(big, 0, 32 * 24 * sizeof(*big));
}

void ui_text(int top, int x, int y, const char *text, int color, int max_cols) {
    unsigned short *map = text_map(top);
    int col = 0;
    if (!map || !text || y < 0 || y >= 24) return;
    if (x < 0) x = 0;
    if (x >= 32 || max_cols <= 0) return;
    while (*text && col < max_cols && x + col < 32) {
        unsigned char ch = (unsigned char)*text++;
        if (ch == '\n') break;
        map[y * 32 + x + col] = (unsigned short)ch |
                                (unsigned short)((color & 15) << 12);
        col++;
    }
}

void ui_text_center(int top, int y, const char *text, int color, int max_cols) {
    int len = 0;
    const char *p = text;
    while (p && *p && *p != '\n' && len < max_cols) { len++; p++; }
    ui_text(top, (32 - len) / 2, y, text, color, max_cols);
}

/* 16px text (Unscii-16): one glyph = two stacked tiles, one map column.
 * color: 0 ink, 1 aqua, 2 muted. */
static unsigned short big_bank(int color) {
    if (color == 1) return BIG_BANK_AQUA;
    if (color == 2) return BIG_BANK_MUTED;
    return BIG_BANK_INK;
}

void ui_text16(int top, int x, int y, const char *text, int color, int max_cols) {
    unsigned short *map = big_map(top);
    unsigned short bank;
    int col = 0;
    if (!map || !text || y < 0 || y >= 22) return;
    if (x < 0) x = 0;
    if (x >= 32 || max_cols <= 0) return;
    bank = big_bank(color);
    while (*text && col < max_cols && x + col < 32) {
        unsigned char ch = (unsigned char)*text++;
        unsigned short slot;
        if (ch == '\n') break;
        slot = lyrCpToSlot[ch];
        map[y * 32 + x + col] = (unsigned short)((bank << 12) | (slot * 2));
        map[(y + 1) * 32 + x + col] = (unsigned short)((bank << 12) | (slot * 2 + 1));
        col++;
    }
}

void ui_text16_center(int top, int y, const char *text, int color, int max_cols) {
    int len = 0;
    const char *p = text;
    while (p && *p && *p != '\n' && len < max_cols) { len++; p++; }
    ui_text16(top, (32 - len) / 2, y, text, color, max_cols);
}

/* Karaoke: per-glyph palette bank so the sung prefix is aqua and the
 * rest stays steel. Used by the animator every time the sweep advances. */
void ui_text16_span(int top, int x, int y, const char *text, int sung_chars,
                    int sung_color, int unsung_color, int max_cols) {
    unsigned short *map = big_map(top);
    unsigned short sung_bank, unsung_bank;
    int col = 0;
    if (!map || !text || y < 0 || y >= 22) return;
    if (x < 0) x = 0;
    if (x >= 32 || max_cols <= 0) return;
    sung_bank = big_bank(sung_color);
    unsung_bank = big_bank(unsung_color);
    while (*text && col < max_cols && x + col < 32) {
        unsigned char ch = (unsigned char)*text++;
        unsigned short bank, slot;
        if (ch == '\n') break;
        bank = (col < sung_chars) ? sung_bank : unsung_bank;
        slot = lyrCpToSlot[ch];
        map[y * 32 + x + col] = (unsigned short)((bank << 12) | (slot * 2));
        map[(y + 1) * 32 + x + col] = (unsigned short)((bank << 12) | (slot * 2 + 1));
        col++;
    }
}

void ui_big_scroll(int top, int y_px) {
    int id = top ? s_bg_top_big : s_bg_bot_big;
    if (id >= 0) bgSetScroll(id, 0, y_px);
}

void ui_text16_clear_rows(int top, int r0, int r1) {
    unsigned short *map = big_map(top);
    if (!map) return;
    if (r0 < 0) r0 = 0;
    if (r1 > 23) r1 = 23;
    if (r1 >= r0)
        memset(map + r0 * 32, 0, (size_t)(r1 - r0 + 1) * 32 * sizeof(*map));
}

/* ---- demo-shaped EQ strips, rendered once into art tile memory ---- */
static void eq_render(unsigned char *gfx, int base_tile, int cw, int ch,
                      const int *heights, int nbars, int x0_px,
                      int base_py, int level, int top_y) {
    unsigned char pix[24][24];
    int w = cw * 8, h = ch * 8;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) pix[y][x] = eq_bg_color(top_y + y);
    for (int i = 0; i < nbars; i++) {
        int bh = (heights[i] * level + 4) / 8;
        int bx = x0_px + i * 3;
        if (level > 0 && bh < 1) bh = 1;
        for (int d = 0; d < bh; d++) {
            int y = base_py - d;
            unsigned char c = (d == bh - 1) ? C_VI_T : C_ACCENT_T;
            if (y < 0 || y >= h) continue;
            if (bx >= 0 && bx < w) pix[y][bx] = c;
            if (bx + 1 >= 0 && bx + 1 < w) pix[y][bx + 1] = c;
        }
    }
    for (int cy = 0; cy < ch; cy++)
        for (int cx = 0; cx < cw; cx++) {
            unsigned char *t = gfx + (base_tile + cy * cw + cx) * 64;
            for (int py = 0; py < 8; py++)
                for (int px = 0; px < 8; px++)
                    t[py * 8 + px] = pix[cy * 8 + py][cx * 8 + px];
        }
}

/* Each screen owns only its own EQ strips. Rendering both layouts into
 * the same gfx buffer used to stamp the top strips over bottom art tiles
 * 222-285 (the vinyl, pill and transport row). */
static void eq_init_top(unsigned char *gfx) {
    static const int L[4] = {5, 9, 7, 11};
    static const int R[4] = {11, 7, 10, 5};
    for (int lvl = 1; lvl <= 8; lvl++) {
        eq_render(gfx, EQ_TOP_L_BASE + (lvl - 1) * 4, 2, 2, L, 4, 0, 14,
                  lvl, UI_EQ_TOP_L_R0 * 8);
        eq_render(gfx, EQ_TOP_R_BASE + (lvl - 1) * 4, 2, 2, R, 4, 4, 14,
                  lvl, UI_EQ_TOP_L_R0 * 8);
    }
}

static void eq_init_bot(unsigned char *gfx) {
    static const int B[6] = {5, 10, 13, 7, 11, 6};
    for (int lvl = 1; lvl <= 8; lvl++)
        eq_render(gfx, EQ_BOT_BASE + (lvl - 1) * 9, 3, 3, B, 6, 0, 17,
                  lvl, UI_EQ_BOT_R0 * 8);
}

void backdrop_init(PrintConsole *con_top, PrintConsole *con_bot) {
    load_one(1, ui_topPal, ui_topTiles, ui_topTilesLen, ui_topMap);
    load_one(0, ui_botPal, ui_botTiles, ui_botTilesLen, ui_botMap);
    /* pristine art maps: restored when the EQ is idle or the panel closes */
    memcpy(s_top_base, s_top_map, sizeof(s_top_base));
    memcpy(s_bot_base, s_bot_map, sizeof(s_bot_base));
    big_init(1);
    big_init(0);
    if (con_top) bgSetPriority(con_top->bgId, 0);
    if (con_bot) bgSetPriority(con_bot->bgId, 0);
    eq_init_top(s_top_gfx);
    eq_init_bot(s_bot_gfx);
    /* progress rails: empty base (two tile rows on the deck) */
    for (int x = UI_PROG_C0; x <= UI_PROG_C1; x++) {
        map_set(s_bot_map, x, UI_PROG_R, UI_TILE_TRK_EMPTY_TOP);
        map_set(s_bot_map, x, UI_PROG_R + 1, UI_TILE_TRK_EMPTY_BOT);
    }
    for (int x = UI_TPROG_C0; x <= UI_TPROG_C1; x++)
        map_set(s_top_map, x, UI_TPROG_R, UI_TILE_TTRK_EMPTY);
}

void backdrop_tick(int phase) {
    unsigned short base = ui_botPal[128 + 5]; /* aqua */
    int r = (base & 31), g = ((base >> 5) & 31), b = ((base >> 10) & 31);
    int mul = (phase & 1) ? 31 : 23;
    unsigned short aqua = (unsigned short)(((r * mul / 31) & 31) |
                                           (((g * mul / 31) & 31) << 5) |
                                           (((b * mul / 31) & 31) << 10));
    unsigned short violet = ui_botPal[128 + 6];
    if (!(phase & 1))
        violet = (unsigned short)(((violet & 31) * 22 / 31) |
                                  ((((violet >> 5) & 31) * 22 / 31) << 5) |
                                  ((((violet >> 10) & 31) * 22 / 31) << 10));
    BG_PALETTE[128 + 5] = aqua;
    BG_PALETTE_SUB[128 + 5] = aqua;
    BG_PALETTE[128 + 6] = violet;
    BG_PALETTE_SUB[128 + 6] = violet;
    (void)s_bg_main;
    (void)s_bg_sub;
}

static void progress_row(unsigned short *map, int row, int c0, int c1,
                         unsigned short t_empty, unsigned short t_fill,
                         unsigned short t_knob,
                         uint32_t pos_ms, uint32_t total_ms) {
    int n = c1 - c0 + 1;
    int fill = 0, knob = -1;
    if (total_ms > 0 && pos_ms < total_ms) {
        fill = (int)((pos_ms * (uint32_t)n) / total_ms);
        knob = c0 + fill;
        if (knob > c1) knob = c1;
    } else if (total_ms > 0) {
        fill = n;
    }
    for (int i = 0; i < n; i++) {
        int x = c0 + i;
        unsigned short t = (i < fill) ? t_fill : t_empty;
        if (x == knob) t = t_knob;
        map_set(map, x, row, t);
    }
}

/* ---- bottom deck seek rail: two stacked tile rows, 12px capsule knob ---- */
void ui_progress(uint32_t pos_ms, uint32_t total_ms) {
    int n = UI_PROG_C1 - UI_PROG_C0 + 1;
    int fill = 0, knob = -1;
    if (total_ms > 0) {
        if (pos_ms < total_ms) {
            fill = (int)(((uint64_t)pos_ms * (uint32_t)n) / total_ms);
            knob = UI_PROG_C0 + fill;
            if (knob > UI_PROG_C1) knob = UI_PROG_C1;
        } else {
            fill = n;
        }
    }
    for (int i = 0; i < n; i++) {
        int x = UI_PROG_C0 + i;
        unsigned short top = (i < fill) ? UI_TILE_TRK_FILL_TOP
                                        : UI_TILE_TRK_EMPTY_TOP;
        unsigned short bot = (i < fill) ? UI_TILE_TRK_FILL_BOT
                                        : UI_TILE_TRK_EMPTY_BOT;
        if (x == knob) {
            top = UI_TILE_TRK_KNOB_TOP;
            bot = UI_TILE_TRK_KNOB_BOT;
        }
        map_set(s_bot_map, x, UI_PROG_R, top);
        map_set(s_bot_map, x, UI_PROG_R + 1, bot);
    }
}

/* ---- top lyric progress rail (row 20, track mid-tile) ---- */
void ui_progress_top(uint32_t pos_ms, uint32_t total_ms) {
    progress_row(s_top_map, UI_TPROG_R, UI_TPROG_C0, UI_TPROG_C1,
                 UI_TILE_TTRK_EMPTY, UI_TILE_TTRK_FILL, UI_TILE_TTRK_KNOB,
                 pos_ms, total_ms);
}

/* ---- transport buttons (2x2 side wells, 4x4 hero) ---- */
void ui_transport(int playing, int shuffle, int repeat) {
    map_block(s_bot_map, UI_RC_SHUF_X0, UI_RC_Y0, UI_RC_SIDE_W, UI_RC_SIDE_H,
              shuffle ? uiShufOn : uiShufOff);
    map_block(s_bot_map, UI_RC_PREV_X0, UI_RC_Y0, UI_RC_SIDE_W, UI_RC_SIDE_H,
              uiPrevIds);
    map_block(s_bot_map, UI_RC_PLAY_X0, UI_RC_PLAY_Y0,
              UI_RC_PLAY_W, UI_RC_PLAY_H, playing ? uiPauseTiles : uiPlayTiles);
    map_block(s_bot_map, UI_RC_NEXT_X0, UI_RC_Y0, UI_RC_SIDE_W, UI_RC_SIDE_H,
              uiNextIds);
    map_block(s_bot_map, UI_RC_REP_X0, UI_RC_Y0, UI_RC_SIDE_W, UI_RC_SIDE_H,
              repeat ? uiRepOn : uiRepOff);
}

/* ---- heart (2x2 cells in the deck header) ---- */
void ui_heart(int liked) {
    const unsigned short *src = liked ? uiHeartOn : uiHeartOff;
    map_block(s_bot_map, UI_HEART_C0, UI_HEART_R0, 2, 2, src);
}

/* ---- segmented tabs (0=lib,1=now,2=queue, else none) ---- */
void ui_tabs(int active) {
    static const unsigned short *const on[3] = {uiTab0On, uiTab1On, uiTab2On};
    static const unsigned short *const off[3] = {uiTab0Off, uiTab1Off, uiTab2Off};
    static const int c0[3] = {UI_TAB0_C0, UI_TAB1_C0, UI_TAB2_C0};
    for (int i = 0; i < 3; i++)
        map_block(s_bot_map, c0[i], UI_TAB_R0, UI_TAB_W, UI_TAB_H,
                  active == i ? on[i] : off[i]);
}

/* ---- list panel: replace the vinyl card interior with a flat glass panel
 *      (Library/Queue/Home/Settings), or restore the deck art. ---- */
void ui_panel(int on) {
    for (int r = UI_PANEL_R0; r <= UI_PANEL_R1; r++) {
        int bottom = (r == UI_PANEL_R1);
        for (int c = UI_PANEL_C0; c <= UI_PANEL_C1; c++) {
            int idx = (bottom ? 3 : 0) +
                      ((c == UI_PANEL_C0) ? 1 : 0) +
                      ((c == UI_PANEL_C1) ? 2 : 0);
            unsigned short t = on ? uiPanelTiles[idx]
                                  : s_bot_base[r * 32 + c];
            map_set(s_bot_map, c, r, t);
        }
    }
}

/* ---- cover art: removed (deck has no cover image); kept as a no-op. ---- */
void ui_cover_show(const char *bmp_path) {
    (void)bmp_path;
}

static void eq_apply(unsigned short *map, const unsigned short *base_map,
                     int c0, int r0, int cw, int ch,
                     int base_tile, int level, int active) {
    if (!active || level <= 0) {
        for (int y = 0; y < ch; y++)
            for (int x = 0; x < cw; x++)
                map_set(map, c0 + x, r0 + y, base_map[(r0 + y) * 32 + c0 + x]);
        return;
    }
    if (level > 8) level = 8;
    {
        int b = base_tile + (level - 1) * cw * ch;
        for (int y = 0; y < ch; y++)
            for (int x = 0; x < cw; x++)
                map_set(map, c0 + x, r0 + y, (unsigned short)(b + y * cw + x));
    }
}

/* ---- top header EQ: 4 bars per side ---- */
void ui_eq_top(int level08, int tick, int active) {
    (void)tick;
    eq_apply(s_top_map, s_top_base, UI_EQ_TOP_L_C0, UI_EQ_TOP_L_R0,
             UI_EQ_TOP_W, UI_EQ_TOP_H, EQ_TOP_L_BASE, level08, active);
    eq_apply(s_top_map, s_top_base, UI_EQ_TOP_R_C0, UI_EQ_TOP_L_R0,
             UI_EQ_TOP_W, UI_EQ_TOP_H, EQ_TOP_R_BASE, level08, active);
}

/* ---- bottom deck header EQ: 6 bars ---- */
void ui_eq_bot(int level08, int tick, int active) {
    (void)tick;
    eq_apply(s_bot_map, s_bot_base, UI_EQ_BOT_C0, UI_EQ_BOT_R0,
             UI_EQ_BOT_W, UI_EQ_BOT_H, EQ_BOT_BASE, level08, active);
}

/* ---- likes ---- */
void ui_likes_load(void) {
    s_like_count = 0;
    FILE *f = fopen("likes.dat", "r");
    if (!f) return;
    char line[256];
    while (s_like_count < MAX_LIKES && fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
        if (n) {
            size_t cn = (n < 255) ? n : 255;
            memcpy(s_likes[s_like_count], line, cn);
            s_likes[s_like_count][cn] = '\0';
            s_like_count++;
        }
    }
    fclose(f);
}

int ui_liked(const char *path) {
    for (int i = 0; i < s_like_count; i++)
        if (strcmp(s_likes[i], path) == 0) return 1;
    return 0;
}

void ui_like_toggle(const char *path) {
    for (int i = 0; i < s_like_count; i++) {
        if (strcmp(s_likes[i], path) == 0) {
            memmove(s_likes[i], s_likes[i + 1], (size_t)(s_like_count - i - 1) * 256);
            s_like_count--;
            goto save;
        }
    }
    if (s_like_count < MAX_LIKES && path) {
        size_t pn = strlen(path);
        size_t cn = (pn < 255) ? pn : 255;
        memcpy(s_likes[s_like_count], path, cn);
        s_likes[s_like_count][cn] = '\0';
        s_like_count++;
    }
save: {
    FILE *f = fopen("likes.dat", "w");
    if (!f) return;
    for (int i = 0; i < s_like_count; i++) fprintf(f, "%s\n", s_likes[i]);
    fclose(f);
}
}

#endif /* ARM9 */

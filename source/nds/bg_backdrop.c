/* Graphical UI engine (ARM9): tiled mockup UI, dynamic map cells,
 * runtime cover art, tile EQ, likes persistence. See tools/make_ui.py. */
#ifdef ARM9

#include <nds.h>
#include <stdio.h>
#include <string.h>
#include "ui_top.h"
#include "ui_bot.h"
#include "ui_cover.h"
#include "ui_layout.h"
#include "font_ui.h"

static int s_bg_main = -1, s_bg_sub = -1;
static unsigned short *s_top_map = NULL, *s_bot_map = NULL;
static unsigned short *s_bot_gfx = NULL;
static unsigned short *s_top_text = NULL, *s_bot_text = NULL;

/* cached "off"-state tiles read from the static map at boot */
static unsigned short s_shuf_off[9], s_rep_on[9], s_play[9], s_heart_off[9];

/* button rects (cells): must match tools/make_ui.py */
#define RC_SHUF_X0 3
#define RC_PREV_X0 8
#define RC_PLAY_X0 14
#define RC_NEXT_X0 20
#define RC_REP_X0 25
#define RC_Y0 15

/* likes */
#define MAX_LIKES 64
static char s_likes[MAX_LIKES][256];
static int s_like_count = 0;

static void map_set(unsigned short *map, int tx, int ty, unsigned short tile) {
    if (tx >= 0 && tx < 32 && ty >= 0 && ty < 24)
        map[ty * 32 + tx] = tile;
}

static void map_block(unsigned short *map, int x0, int y0, const unsigned short *tiles) {
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++)
            map_set(map, x0 + c, y0 + r, tiles[r * 3 + c]);
}

static void map_read(unsigned short *map, int x0, int y0, unsigned short *out) {
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++)
            out[r * 3 + c] = map[(y0 + r) * 32 + x0 + c];
}

static void load_one(int main_screen, const unsigned short *pal,
                     const unsigned char *tiles, unsigned int tiles_len,
                     const unsigned short *map) {
    int id;
    unsigned short *dstpal;
    if (main_screen) {
        id = bgInit(3, BgType_Text8bpp, BgSize_T_256x256, 28, 1);
        s_bg_main = id;
        dmaCopy(tiles, bgGetGfxPtr(id), tiles_len);
        dmaCopy(map, bgGetMapPtr(id), 32 * 24 * 2);
        /* UI art owns palette bank 128-191 only: entries 0-127 belong to
         * the console text layer (default libnds text colors). */
        dmaCopy(&pal[128], BG_PALETTE + 128, 64 * 2);
        dstpal = BG_PALETTE;
        bgSetPriority(id, 3);
        s_top_map = bgGetMapPtr(id);
    } else {
        id = bgInitSub(3, BgType_Text8bpp, BgSize_T_256x256, 28, 1);
        s_bg_sub = id;
        dmaCopy(tiles, bgGetGfxPtr(id), tiles_len);
        dmaCopy(map, bgGetMapPtr(id), 32 * 24 * 2);
        dmaCopy(&pal[128], BG_PALETTE_SUB + 128, 64 * 2);
        dstpal = BG_PALETTE_SUB;
        bgSetPriority(id, 3);
        s_bot_map = bgGetMapPtr(id);
        s_bot_gfx = bgGetGfxPtr(id);
    }
    /* Cover bank 192-255: uniform RGB444 ramp (2 bits per channel).
     * Tile value = 192 + (r2<<4 | g2<<2 | b2), r2 = r>>6 etc. */
    for (int i = 0; i < 64; i++) {
        int r2 = (i >> 4) & 3, g2 = (i >> 2) & 3, b2 = i & 3;
        int r5 = (r2 * 31 + 1) / 3, g5 = (g2 * 31 + 1) / 3, b5 = (b2 * 31 + 1) / 3;
        dstpal[192 + i] = (unsigned short)(r5 | (g5 << 5) | (b5 << 10));
    }
}

/* Custom interface typeface (Unscii-8, public domain). The console layer is
 * retained as a compact font tile resource; finished UI text is composed
 * directly into its map by ui_text(). */
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
}

static unsigned short *text_map(int top) {
    return top ? s_top_text : s_bot_text;
}

void ui_text_clear(int top) {
    unsigned short *map = text_map(top);
    if (map) memset(map, 0, 32 * 24 * sizeof(*map));
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

void backdrop_init(PrintConsole *con_top, PrintConsole *con_bot) {
    load_one(1, ui_topPal, ui_topTiles, ui_topTilesLen, ui_topMap);
    load_one(0, ui_botPal, ui_botTiles, ui_botTilesLen, ui_botMap);
    if (con_top) bgSetPriority(con_top->bgId, 0);
    if (con_bot) bgSetPriority(con_bot->bgId, 0);
    /* cache default button tiles, then paint boot states */
    map_read(s_bot_map, RC_SHUF_X0, RC_Y0, s_shuf_off);
    map_read(s_bot_map, RC_REP_X0, RC_Y0, s_rep_on);
    map_read(s_bot_map, RC_PLAY_X0, RC_Y0, s_play);
    map_read(s_bot_map, 28, 7, s_heart_off);
    /* progress track base */
    map_set(s_bot_map, UI_PROG_C0, UI_PROG_R, UI_TILE_TRK_CAP_L);
    for (int x = UI_PROG_C0 + 1; x < UI_PROG_C1; x++)
        map_set(s_bot_map, x, UI_PROG_R, UI_TILE_TRK_EMPTY);
    map_set(s_bot_map, UI_PROG_C1, UI_PROG_R, UI_TILE_TRK_CAP_R);
    /* tab underlines dim */
    for (int x = 2; x <= 30; x++)
        map_set(s_bot_map, x, 23, UI_TILE_TAB_DIM);
    /* EQ empty (top) */
    for (int r = 1; r <= 3; r++)
        for (int x = 1; x <= 4; x++) map_set(s_top_map, x, r, UI_TILE_EQ_EMPTY);
    for (int r = 1; r <= 3; r++)
        for (int x = 27; x <= 30; x++) map_set(s_top_map, x, r, UI_TILE_EQ_EMPTY);
    /* placeholder cover */
    for (int i = 0; i < 64; i++)
        dmaCopy(&coverPlaceTiles[i * 64], (unsigned char *)s_bot_gfx + uiCoverCells[i] * 64, 64);
}

void backdrop_tick(int phase) {
    unsigned short base = ui_botPal[128 + 41];
    int r = (base & 31), g = ((base >> 5) & 31), b = ((base >> 10) & 31);
    int mul = (phase & 1) ? 31 : 23;
    unsigned short emerald = (unsigned short)(((r * mul / 31) & 31) |
                                               (((g * mul / 31) & 31) << 5) |
                                               (((b * mul / 31) & 31) << 10));
    unsigned short violet = ui_botPal[128 + 45];
    if (!(phase & 1))
        violet = (unsigned short)(((violet & 31) * 22 / 31) |
                                  ((((violet >> 5) & 31) * 22 / 31) << 5) |
                                  ((((violet >> 10) & 31) * 22 / 31) << 10));
    BG_PALETTE[128 + 41] = emerald;
    BG_PALETTE_SUB[128 + 41] = emerald;
    BG_PALETTE[128 + 45] = violet;
    BG_PALETTE_SUB[128 + 45] = violet;
    (void)s_bg_main;
    (void)s_bg_sub;
}

/* ---- progress: fill segs + knob ---- */
void ui_progress(uint32_t pos_ms, uint32_t total_ms) {
    int n = UI_PROG_C1 - UI_PROG_C0 - 1; /* inner segs */
    int fill = 0, knob = -1;
    if (total_ms > 0 && pos_ms < total_ms) {
        fill = (int)((pos_ms * (uint32_t)n) / total_ms);
        knob = UI_PROG_C0 + 1 + fill;
        if (knob >= UI_PROG_C1) knob = UI_PROG_C1 - 1;
    } else if (total_ms > 0) {
        fill = n;
    }
    for (int i = 0; i < n; i++) {
        int x = UI_PROG_C0 + 1 + i;
        unsigned short t = (i < fill) ? UI_TILE_TRK_FILL : UI_TILE_TRK_EMPTY;
        if (x == knob) t = UI_TILE_TRK_KNOB;
        map_set(s_bot_map, x, UI_PROG_R, t);
    }
}

/* ---- transport buttons ---- */
void ui_transport(int playing, int shuffle, int repeat) {
    map_block(s_bot_map, RC_SHUF_X0, RC_Y0, shuffle ? uiShufOn : s_shuf_off);
    map_block(s_bot_map, RC_REP_X0, RC_Y0, repeat ? s_rep_on : uiRepOff);
    map_block(s_bot_map, RC_PLAY_X0, RC_Y0, playing ? uiPauseTiles : uiPlayTiles);
}

/* ---- heart ---- */
void ui_heart(int liked) {
    const unsigned short *src = liked ? uiHeartOn : s_heart_off;
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++)
            map_set(s_bot_map, 28 + c, 7 + r, src[r * 3 + c]);
}

/* ---- tabs: 0=lib,1=now,2=queue, else none ---- */
void ui_tabs(int active) {
    static const int ranges[3][2] = {{2, 8}, {13, 19}, {24, 30}};
    for (int x = 2; x <= 30; x++)
        map_set(s_bot_map, x, 23, UI_TILE_TAB_DIM);
    if (active >= 0 && active < 3)
        for (int x = ranges[active][0]; x <= ranges[active][1]; x++)
            map_set(s_bot_map, x, 23, UI_TILE_TAB_ON);
}

/* ---- cover art: 64x64 24-bit BMP or placeholder ---- */
void ui_cover_show(const char *bmp_path) {
    unsigned char tiles[4096];
    int ok = 0;
    if (bmp_path && bmp_path[0]) {
        FILE *f = fopen(bmp_path, "rb");
        if (f) {
            unsigned char hdr[54];
            if (fread(hdr, 1, 54, f) == 54 && hdr[0] == 'B' && hdr[1] == 'M' &&
                hdr[28] == 24 && hdr[30] == 0 && hdr[31] == 0 && hdr[32] == 0 && hdr[33] == 0) {
                int w = hdr[18] | (hdr[19] << 8) | (hdr[20] << 16) | (hdr[21] << 24);
                int h = hdr[22] | (hdr[23] << 8) | (hdr[24] << 16) | (hdr[25] << 24);
                unsigned int off = hdr[10] | (hdr[11] << 8) | (hdr[12] << 16) | (hdr[13] << 24);
                if (w == 64 && h == 64) {
                    int stride = (64 * 3 + 3) & ~3;
                    static unsigned char row[64 * 3 + 4];
                    ok = 1;
                    for (int y = 0; y < 64 && ok; y++) {
                        fseek(f, (long)(off + (63 - y) * stride), SEEK_SET);
                        if (fread(row, 1, 64 * 3, f) != 64 * 3) { ok = 0; break; }
                        for (int x = 0; x < 64; x++) {
                            int b = row[x * 3], g = row[x * 3 + 1], r = row[x * 3 + 2];
                            tiles[(y / 8) * 512 + (y % 8) * 8 + (x / 8) * 64 + (x % 8)] =
                                (unsigned char)(192 + (r >> 6) * 16 + (g >> 6) * 4 + (b >> 6));
                        }
                    }
                }
            }
            fclose(f);
        }
    }
    if (!ok) {
        for (int i = 0; i < 64; i++)
            dmaCopy(&coverPlaceTiles[i * 64], (unsigned char *)s_bot_gfx + uiCoverCells[i] * 64, 64);
    } else {
        for (int i = 0; i < 64; i++)
            dmaCopy(&tiles[i * 64], (unsigned char *)s_bot_gfx + uiCoverCells[i] * 64, 64);
    }
}

/* ---- tile EQ (top): 4 bars per side, heights 0..24 ---- */
void ui_eq(int level08, int tick, int active) {
    int base = active ? level08 * 3 : 0;
    if (base > 24) base = 24;
    int cols[8] = {1, 2, 3, 4, 27, 28, 29, 30};
    int fac[4] = {10, 7, 5, 8};
    for (int i = 0; i < 8; i++) {
        int h = (base * fac[i % 4]) / 10 + ((tick + i) & 1);
        if (h > 24) h = 24;
        if (h < 0) h = 0;
        for (int k = 0; k < 3; k++) {
            int fill = h - (2 - k) * 8;
            unsigned short t;
            if (fill >= 8) t = UI_TILE_EQ_FULL;
            else if (fill <= 0) t = UI_TILE_EQ_EMPTY;
            else t = (unsigned short)(UI_TILE_EQ_P1 - 1 + fill);
            map_set(s_top_map, cols[i], 1 + k, t);
        }
    }
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

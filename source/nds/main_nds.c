/* MixtapeDSi v1.0.7 - ARM9 entry point (devkitARM/libnds + maxmod + dr_mp3).
 * "Midnight Glass" deck: top lyric stage, persistent bottom player deck.
 * Host builds (tests) never compile this file (guarded by ARM9). */
#ifdef ARM9

#include <nds.h>
#include <fat.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include <maxmod9.h>
#include "music_library.h"
#include "playback_controller.h"
#include "lrc_parser.h"
#include "lyric_timeline.h"
#include "mp3_decoder.h"
#include "audio_hw.h"
#include "app_config.h"
#include "ui_display.h"
#include "ui_layout.h"
#include "lyric_text.h"
#include "version.h"

typedef enum {
    VIEW_HOME = 0,
    VIEW_NOW,
    VIEW_LYRICS, /* alias: bottom renders NOW; top always shows lyrics */
    VIEW_LIB,
    VIEW_QUEUE,
    VIEW_SETTINGS,
    VIEW_COUNT
} ViewId;

static MusicLibrary g_lib;
static PlaybackController g_pb;
static Mp3Decoder g_dec;
static int g_have_track = 0;
static AppConfig g_cfg;
static LrcSong g_lrc;
static int g_view = VIEW_NOW; /* boot to player deck (demo screen) */
static int g_sel = 0;          /* library selection */
static int g_qsel = 0;         /* queue selection */
static int g_menu = 0;         /* home menu selection */
static PrintConsole *g_con_top = NULL;
static PrintConsole *g_con_bot = NULL;

/* ---- lyric stage animation (karaoke sweep + sub-tile glide) ---- */
#define LYR_ACTIVE_ROW 11
#define LYR_ROWS 3
#define LYR_COLS 30
static int s_glide = -1;      /* index into k_glide; -1 = settled */
static int s_anim_cur = -2;   /* line currently painted on the 16px layer */
static int s_anim_sung = -1;  /* last painted sung-column count */
static int s_anim_total = 0;  /* display chars in that line */
/* sub-tile glide: line enters 16 px low and eases into place */
static const int k_glide_steps[] = {16, 13, 10, 7, 5, 3, 1, 0};
#define N_GLIDE ((int)(sizeof(k_glide_steps) / sizeof(k_glide_steps[0])))

/* ---- stylus scrub state (preview while dragging, seek on release) ---- */
static int s_scrub = 0;
static uint32_t s_scrub_ms = 0;

static void to_latin1(char *dst, const char *src, size_t n);

static uint32_t track_total_ms(void) {
    if (!g_have_track || !g_lrc.meta.has_length) return 0;
    return (uint32_t)g_lrc.meta.length_ms;
}

static uint32_t now_pos_ms(void) {
    if (s_scrub) return s_scrub_ms;
    return g_have_track ? audio_hw_position_ms() : 0;
}

static void draw_text(int top, int x, int y, const char *text, int color, int max) {
    char tmp[96];
    to_latin1(tmp, text ? text : "", sizeof(tmp));
    ui_text(top, x, y, tmp, color, max);
}

static void draw_center(int top, int y, const char *text, int color, int max) {
    char tmp[96];
    to_latin1(tmp, text ? text : "", sizeof(tmp));
    ui_text_center(top, y, tmp, color, max);
}

static void draw_right(int top, int y, const char *text, int color, int max) {
    char tmp[48];
    to_latin1(tmp, text ? text : "", sizeof(tmp));
    size_t len = strlen(tmp);
    if ((int)len > max) len = (size_t)max;
    ui_text(top, 31 - (int)len, y, tmp, color, max);
}

/* UTF-8 -> Latin-1 for display (DS font has Latin-1 glyphs, not UTF-8).
 * French accents, curly apostrophe; everything else becomes '?'. */
static void to_latin1(char *dst, const char *src, size_t n) {
    size_t o = 0;
    while (*src && o + 1 < n) {
        unsigned char c = (unsigned char)*src;
        if (c < 0x80) { dst[o++] = *src++; }
        else if ((c & 0xE0) == 0xC0 && src[1]) {
            unsigned cp = ((c & 0x1F) << 6) | (src[1] & 0x3F);
            dst[o++] = (cp >= 0x80 && cp <= 0xFF) ? (char)cp : '?';
            src += 2;
        } else if (c == 0xE2 && (unsigned char)src[1] == 0x80 &&
                   (unsigned char)src[2] == 0x99) {
            dst[o++] = '\''; src += 3; /* ’ */
        } else {
            dst[o++] = '?'; src++;
            while ((*src & 0xC0) == 0x80) src++;
        }
    }
    dst[o] = '\0';
}

/* Segmented tab labels (row 22): active tab aqua, others muted. Must be
 * drawn AFTER the view so its ui_text_clear() cannot erase them. */
static void draw_tab_labels(int active) {
    draw_text(0, UI_TAB0_TX, UI_TAB_TEXT_R, "Library", active == 0 ? 1 : 2, 7);
    draw_text(0, UI_TAB1_TX, UI_TAB_TEXT_R, "Now", active == 1 ? 1 : 2, 3);
    draw_text(0, UI_TAB2_TX, UI_TAB_TEXT_R, "Queue", active == 2 ? 1 : 2, 5);
}

static void fmt_time(char *dst, size_t n, uint32_t ms) {
    snprintf(dst, n, "%02lu:%02lu",
             (unsigned long)(ms / 60000), (unsigned long)((ms / 1000) % 60));
}

/* Display names: ID3 tags win; otherwise the file's own name (never a
 * hardcoded demo string). */
static const char *base_name(const char *path) {
    const char *b = path ? path : "";
    for (const char *p = b; *p; p++)
        if (*p == '/' || *p == '\\' || *p == ':') b = p + 1;
    return b;
}

static void strip_ext(char *dst, size_t n, const char *base) {
    size_t i = 0, dot = (size_t)-1;
    if (!n) return;
    while (base[i] && i + 1 < n) { if (base[i] == '.') dot = i; i++; }
    {
        size_t len = (dot == (size_t)-1) ? i : dot;
        if (len >= n) len = n - 1;
        memcpy(dst, base, len);
        dst[len] = '\0';
    }
}

static char s_namebuf[64];
static const char *track_title(LibraryTrack *t) {
    if (t->meta.title[0]) return t->meta.title;
    strip_ext(s_namebuf, sizeof(s_namebuf), base_name(t->path));
    return s_namebuf[0] ? s_namebuf : "?";
}

static void view_home(void) {
    static const char *items[] = {"NOW PLAYING", "LIBRARY", "QUEUE", "SETTINGS"};
    ui_text_clear(0);
    ui_panel(1);
    draw_text(0, 2, 1, "MIXTAPEDSi " MIXTAPEDSI_VERSION, 1, 20);
    {
        char count[24];
        snprintf(count, sizeof(count), "%d TRACK%s", g_lib.count,
                 g_lib.count == 1 ? "" : "S");
        draw_text(0, 2, 2, count, 4, 12);
    }
    for (int i = 0; i < 4; i++) {
        int y = 4 + i * 2;
        draw_text(0, 1, y, i == g_menu ? ">" : " ", i == g_menu ? 1 : 3, 1);
        draw_text(0, 3, y, items[i], i == g_menu ? 1 : 0, 22);
    }
    if (!g_lib.count)
        draw_text(0, 2, 11, "ADD MUSIC TO THE music FOLDER", 2, 29);
    else if (!g_have_track)
        draw_text(0, 2, 11, "CHOOSE A TRACK IN LIBRARY", 2, 27);
    draw_text(0, 2, 13, "A OPEN    B BACK    START VIEWS", 4, 30);
}

static void view_now_playing(void) {
    int idx = pb_current_index(&g_pb);
    const char *title = "Nothing playing";
    const char *artist = "Pick a track in Library";
    const char *album = "";
    const char *path = "";
    uint32_t pos = now_pos_ms();
    uint32_t total = track_total_ms();
    if (idx >= 0 && g_have_track) {
        LibraryTrack *t = &g_lib.tracks[idx];
        path = t->path;
        title = track_title(t);
        artist = t->meta.artist[0] ? t->meta.artist : "";
        album = t->meta.album;
    }
    ui_text_clear(0);
    ui_panel(0);
    draw_text(0, 2, 1, "NOW PLAYING", 1, 12);
    if (g_have_track && idx >= 0) {
        char trackbuf[40];
        int tot_t = g_lib.count > 0 ? g_lib.count : 1;
        snprintf(trackbuf, sizeof(trackbuf), "TRACK %02d/%02d %s",
                 idx + 1, tot_t,
                 g_pb.state == PB_PLAYING ? "PLAYING" : "PAUSED");
        draw_text(0, 2, 2, trackbuf, 4, 20);
    } else {
        draw_text(0, 2, 2, "NO TRACK LOADED", 4, 16);
    }
    {
        char t16[96];
        to_latin1(t16, title, sizeof(t16));
        ui_text16(0, 11, 4, t16, g_have_track ? 0 : 2, 19);
    }
    draw_text(0, 11, 8, artist, 2, 19);
    draw_text(0, 11, 9, album, 4, 19);
    /* quality chip painted into the glass pill */
    draw_text(0, 12, 11, g_have_track ? "MP3" : "IDLE", 3, 7);
    {
        char tb[16];
        fmt_time(tb, sizeof(tb), pos);
        draw_text(0, 1, 14, tb, 1, 8);
        fmt_time(tb, sizeof(tb), total);
        draw_text(0, 24, 14, tb, 2, 8);
    }
    if (g_have_track)
        draw_center(0, 14, "44.1 KHZ", 4, 8);
    (void)path;
}

/* Print up to max_rows wrapped lines, centered, starting at start_row. */
static void print_lyric_block(const char *text,
                              int start_row, int max_rows, int color) {
    char scratch[256];
    const char *lines[4];
    int n = wrap_lines(text, LYR_COLS, max_rows > 4 ? 4 : max_rows, scratch, lines);
    for (int i = 0; i < n; i++)
        draw_center(1, start_row + i, lines[i], color, LYR_COLS);
}

/* Paint the active line on the 16px layer: aqua prefix for the part of
 * the line already "sung", muted for the rest. Up to three wrapped rows
 * (30 columns each) so long lines keep their final words on screen.
 * Returns the number of wrapped rows. */
static int draw_active_lyric(int cur, uint32_t pos) {
    char latin[LYR_ROWS][256];
    char scratch[768];
    const char *lines[LYR_ROWS];
    int rows, total = 0, sung, offset = 0;
    uint32_t song_len;

    if (cur < 0 || cur >= g_lrc.count) return 0;
    rows = wrap_lines(g_lrc.lines[cur].text, LYR_COLS, LYR_ROWS, scratch, lines);
    for (int i = 0; i < rows; i++) {
        to_latin1(latin[i], lines[i], sizeof(latin[i]));
        total += (int)strlen(latin[i]);
    }
    song_len = g_lrc.meta.has_length ? (uint32_t)g_lrc.meta.length_ms : 0;
    sung = (g_cfg.anim == ANIM_OFF)
         ? total
         : lyric_sweep_chars(&g_lrc, cur, pos, total, song_len);

    /* clear the 3-row active band (row 17 holds the upcoming line) */
    ui_text16_clear_rows(1, LYR_ACTIVE_ROW - 1,
                         LYR_ACTIVE_ROW + (LYR_ROWS - 1) * 2 + 1);
    for (int i = 0; i < rows; i++) {
        int len = (int)strlen(latin[i]);
        int x = (32 - len) / 2;
        if (x < 0) x = 0;
        ui_text16_span(1, x, LYR_ACTIVE_ROW + i * 2, latin[i],
                       sung - offset, 1, 2, LYR_COLS);
        offset += len;
    }
    s_anim_cur = cur;
    s_anim_total = total;
    s_anim_sung = sung;
    return rows;
}

/* Per-frame animation driver. Runs every frame (not only on full
 * redraws) so the karaoke sweep and glide stay smooth without
 * repainting the whole UI. */
static void lyric_anim_tick(uint32_t pos) {
    int cur, sung;
    uint32_t song_len;

    /* advance an in-flight glide by one step */
    if (s_glide >= 0) {
        if (s_glide < N_GLIDE) ui_big_scroll(1, k_glide_steps[s_glide]);
        s_glide++;
        if (s_glide >= N_GLIDE) {
            s_glide = -1;
            ui_big_scroll(1, 0);
        }
    }

    if (!g_have_track || g_lrc.count == 0) return;
    cur = lyric_active_index(&g_lrc, pos);
    if (cur < 0) {
        s_anim_cur = -1;
        s_anim_sung = -1;
        return;
    }
    if (cur != s_anim_cur) {
        draw_active_lyric(cur, pos);
        if (g_cfg.anim == ANIM_FULL) {
            s_glide = 1;
            ui_big_scroll(1, k_glide_steps[0]);
        } else {
            s_glide = -1;
            ui_big_scroll(1, 0);
        }
        return;
    }
    if (g_cfg.anim == ANIM_OFF) return; /* static full-aqua line */
    song_len = g_lrc.meta.has_length ? (uint32_t)g_lrc.meta.length_ms : 0;
    sung = lyric_sweep_chars(&g_lrc, cur, pos, s_anim_total, song_len);
    if (sung != s_anim_sung) draw_active_lyric(cur, pos);
}

static void view_lyrics(void) {
    /* Lyric stage: TOP screen only, always (matches screen_lyrics.png). */
    int idx = pb_current_index(&g_pb);
    uint32_t pos = now_pos_ms();
    int prv = -1, cur = -1, nxt = -1;
    const char *title = "No track";
    const char *artist = "";

    if (idx >= 0 && g_have_track) {
        LibraryTrack *t = &g_lib.tracks[idx];
        title = track_title(t);
        if (t->meta.artist[0]) artist = t->meta.artist;
        lyric_window(&g_lrc, pos, &prv, &cur, &nxt);
    }

    ui_text_clear(1);
    draw_center(1, 1, title, 0, LYR_COLS);
    draw_center(1, 2, artist, 2, LYR_COLS);

    if (cur >= 0) {
        /* Stage geometry: previous y64, active 16px rows 11/13/15,
         * upcoming y136. */
        int line_changed = (cur != s_anim_cur);
        int n_cur;
        if (prv >= 0) {
            char prv_scratch[256];
            const char *prv_lines[2];
            int n_prv = wrap_lines(g_lrc.lines[prv].text, LYR_COLS, 1,
                                   prv_scratch, prv_lines);
            if (n_prv > 0) draw_center(1, 8, prv_lines[0], 2, LYR_COLS);
        }
        n_cur = draw_active_lyric(cur, pos);
        if (g_cfg.anim == ANIM_FULL) {
            /* only start a glide when the displayed line actually changed;
             * redraws for volume/tabs must not restart it */
            if (line_changed) {
                s_glide = 1;
                ui_big_scroll(1, k_glide_steps[0]);
            }
        } else {
            s_glide = -1;
            ui_big_scroll(1, 0);
        }
        if (nxt >= 0 && n_cur < LYR_ROWS) {
            char nxt_scratch[256];
            const char *nxt_lines[2];
            int n_nxt = wrap_lines(g_lrc.lines[nxt].text, LYR_COLS, 1,
                                   nxt_scratch, nxt_lines);
            if (n_nxt > 0)
                draw_center(1, 17, nxt_lines[0], 2, LYR_COLS);
        }
    } else if (g_have_track && g_lrc.count == 0) {
        s_anim_cur = -1;
        s_anim_sung = -1;
        draw_center(1, 12, "No synchronized lyrics", 2, LYR_COLS);
    } else if (g_have_track) {
        s_anim_cur = -1;
        s_anim_sung = -1;
        draw_center(1, 12, "(intro...)", 2, LYR_COLS);
        if (nxt >= 0) {
            char nxt_scratch[256];
            const char *nxt_lines[4];
            int n_nxt = wrap_lines(g_lrc.lines[nxt].text, LYR_COLS, 3,
                                   nxt_scratch, nxt_lines);
            for (int i = 0; i < n_nxt; i++)
                draw_center(1, 15 + i, nxt_lines[i], 2, LYR_COLS);
        }
    } else {
        /* Neutral idle state: never show hardcoded demo strings. */
        s_anim_cur = -1;
        s_anim_sung = -1;
        print_lyric_block("Nothing playing", 11, 2, 0);
        print_lyric_block("Pick a track in Library", 14, 2, 2);
    }
    {
        uint32_t total = g_lrc.meta.has_length ? (uint32_t)g_lrc.meta.length_ms : 0;
        if (!g_have_track) { pos = 0; total = 0; }
        char curbuf[16], totbuf[16];
        fmt_time(curbuf, sizeof(curbuf), pos);
        fmt_time(totbuf, sizeof(totbuf), total);
        draw_text(1, 3, 21, curbuf, 2, 6);
        draw_center(1, 21, "KARAOKE", 3, 12);
        draw_text(1, 24, 21, totbuf, 2, 6);
    }
    if (cur < 0) {
        s_glide = -1;
        ui_big_scroll(1, 0);
    }
}

static void view_library(void) {
    ui_text_clear(0);
    ui_panel(1);
    draw_text(0, 2, 1, "LIBRARY", 1, 12);
    if (!g_lib.count) {
        draw_center(0, 6, "No tracks yet", 0, 26);
        draw_center(0, 8, "Add MP3s to the music folder", 2, 26);
        draw_text(0, 2, 13, "B BACK", 4, 20);
        return;
    }
    {
        char count[40];
        snprintf(count, sizeof(count), "%d TRACK%s", g_lib.count,
                 g_lib.count == 1 ? "" : "S");
        draw_text(0, 2, 2, count, 4, 16);
    }
    /* five two-line entries inside the glass panel */
    int start = g_sel > UI_LIST_ROWS - 1 ? g_sel - (UI_LIST_ROWS - 1) : 0;
    for (int row = 0, i = start; i < g_lib.count && row < UI_LIST_ROWS;
         i++, row++) {
        LibraryTrack *t = &g_lib.tracks[i];
        int y = 4 + row * 2;
        int cur = (i == pb_current_index(&g_pb));
        draw_text(0, 1, y, i == g_sel ? ">" : (cur ? "*" : " "),
                  i == g_sel ? 1 : 3, 1);
        draw_text(0, 3, y, track_title(t), i == g_sel ? 1 : 0, 26);
        draw_text(0, 3, y + 1,
                  cur ? "NOW PLAYING" :
                  (t->meta.artist[0] ? t->meta.artist : ""), 2, 26);
    }
}

static void view_queue(void) {
    ui_text_clear(0);
    ui_panel(1);
    draw_text(0, 2, 1, "QUEUE", 1, 12);
    if (!g_pb.order_len) {
        draw_center(0, 6, "Queue is empty", 0, 26);
        draw_center(0, 8, "Choose a track from Library", 2, 26);
        draw_text(0, 2, 13, "B BACK", 4, 20);
        return;
    }
    {
        char count[40];
        snprintf(count, sizeof(count), "%d UP NEXT", g_pb.order_len);
        draw_text(0, 2, 2, count, 4, 16);
    }
    int start = g_qsel > UI_LIST_ROWS - 1 ? g_qsel - (UI_LIST_ROWS - 1) : 0;
    for (int row = 0, i = start; i < g_pb.order_len && row < UI_LIST_ROWS;
         i++, row++) {
        int li = g_pb.order[i];
        LibraryTrack *t = &g_lib.tracks[li];
        int y = 4 + row * 2;
        draw_text(0, 1, y, i == g_qsel ? ">" : (i == g_pb.order_pos ? "*" : " "),
                  i == g_qsel ? 1 : 3, 1);
        draw_text(0, 3, y, track_title(t), i == g_qsel ? 1 : 0, 26);
        draw_text(0, 3, y + 1, i == g_pb.order_pos ? "NOW PLAYING" : "UP NEXT",
                  2, 26);
    }
}

static void view_settings(void) {
    static const char *anim_name[] = {"FULL", "REDUCED", "OFF"};
    ui_text_clear(0);
    ui_panel(1);
    draw_text(0, 2, 1, "SETTINGS", 1, 12);
    draw_text(0, 2, 2, MIXTAPEDSI_VERSION "  TAP A ROW", 4, 18);
    draw_text(0, 3, 4, "ANIMATION", 0, 12);
    draw_text(0, 18, 4, anim_name[(int)g_cfg.anim], 1, 8);
    draw_text(0, 3, 6, "SHUFFLE", 0, 12);
    draw_text(0, 18, 6, g_pb.shuffle ? "ON" : "OFF", 1, 8);
    draw_text(0, 3, 8, "REPEAT", 0, 12);
    draw_text(0, 18, 8, g_pb.repeat == REPEAT_ONE ? "ONE" :
              g_pb.repeat == REPEAT_ALL ? "ALL" : "OFF", 1, 8);
    {
        char volume[16];
        snprintf(volume, sizeof(volume), "%d%%", g_cfg.volume * 100 / 127);
        draw_text(0, 3, 10, "VOLUME", 0, 12);
        draw_text(0, 18, 10, volume, 1, 8);
    }
    draw_text(0, 2, 13, "L/R ANIM   A SHUF   Y REPEAT", 4, 30);
}

static void save_config(void) {
    FILE *f = fopen("mixtapedsi.cfg", "w");
    if (!f) return;
    fprintf(f, "lyrics_screen=%d\nauto_follow=%d\nanim=%d\nshuffle=%d\nrepeat=%d\nvolume=%d\nmusic_dir=%s\n",
            (int)g_cfg.lyrics_screen, g_cfg.auto_follow, (int)g_cfg.anim,
            g_cfg.shuffle_default, g_cfg.repeat_default, g_cfg.volume, g_cfg.music_dir);
    fclose(f);
}

static void load_track(int idx) {
    audio_hw_close();
    mp3_close(&g_dec);
    g_have_track = 0;
    memset(&g_lrc, 0, sizeof(g_lrc));
    s_scrub = 0;
    s_scrub_ms = 0;
    /* reset lyric animation state for the new track */
    s_glide = -1;
    s_anim_cur = -2;
    s_anim_sung = -1;
    s_anim_total = 0;
    ui_big_scroll(1, 0);
    if (idx < 0 || idx >= g_lib.count) return;
    LibraryTrack *t = &g_lib.tracks[idx];
    if (mp3_open(&g_dec, t->path) != 0) return;
    if (audio_hw_open(&g_dec) != 0) { mp3_close(&g_dec); return; }
    audio_hw_set_volume(g_cfg.volume);
    /* prime the ring so playback starts with a full buffer */
    for (int i = 0; i < 6; i++) audio_hw_pump();
    g_have_track = 1;
    FILE *f = fopen(t->lrc_path, "rb");
    if (f) {
        static char buf[32 * 1024];
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n] = '\0';
        fclose(f);
        lrc_parse(buf, &g_lrc);
    }
}

static void do_pause(void) {
    pb_pause(&g_pb);
    audio_hw_set_paused(1);
}

static void do_resume(void) {
    pb_resume(&g_pb);
    audio_hw_set_paused(0);
}

/* Start a track from the library/queue and jump to the player deck. */
static void play_track(int idx) {
    if (idx < 0 || idx >= g_lib.count) return;
    pb_play_index(&g_pb, idx);
    load_track(idx);
    do_resume();
    g_view = VIEW_NOW;
}

static int hit_circle(int px, int py, int cx, int cy, int r) {
    int dx = px - cx, dy = py - cy;
    return dx * dx + dy * dy <= r * r;
}

/* Single tap dispatcher. Tabs, heart and transport are persistent deck
 * controls and therefore work on every view; panel rows are view-specific. */
static void handle_tap(int px, int py) {
    int ci = pb_current_index(&g_pb);

    if (py >= UI_TAB_Y0 && py <= UI_TAB_Y1) {
        if (px >= UI_TAB0_X0 && px <= UI_TAB0_X1) { g_view = VIEW_LIB; return; }
        if (px >= UI_TAB1_X0 && px <= UI_TAB1_X1) { g_view = VIEW_NOW; return; }
        if (px >= UI_TAB2_X0 && px <= UI_TAB2_X1) { g_view = VIEW_QUEUE; return; }
        return;
    }
    if (px >= UI_HEART_X0 && px < UI_HEART_X1 &&
        py >= UI_HEART_Y0 && py < UI_HEART_Y1) {
        if (ci >= 0 && g_have_track) {
            ui_like_toggle(g_lib.tracks[ci].path);
            ui_heart(ui_liked(g_lib.tracks[ci].path));
        }
        return;
    }
    if (hit_circle(px, py, UI_B_PLAY_X, UI_B_Y, 26)) {
        if (g_have_track) {
            if (g_pb.state == PB_PLAYING) do_pause();
            else do_resume();
        } else if (g_lib.count > 0) {
            load_track(0);
            do_resume();
        }
        return;
    }
    if (hit_circle(px, py, UI_B_PREV_X, UI_B_Y, 16)) {
        if (g_have_track && pb_prev(&g_pb)) { load_track(pb_current_index(&g_pb)); do_resume(); }
        else if (g_lib.count > 0) { load_track(0); do_resume(); }
        return;
    }
    if (hit_circle(px, py, UI_B_NEXT_X, UI_B_Y, 16)) {
        if (g_have_track && pb_next(&g_pb)) { load_track(pb_current_index(&g_pb)); do_resume(); }
        else if (g_lib.count > 0) { load_track(0); do_resume(); }
        return;
    }
    if (hit_circle(px, py, UI_B_SHUF_X, UI_B_Y, 16)) {
        pb_set_shuffle(&g_pb, !g_pb.shuffle);
        g_cfg.shuffle_default = g_pb.shuffle;
        save_config();
        return;
    }
    if (hit_circle(px, py, UI_B_REP_X, UI_B_Y, 16)) {
        pb_cycle_repeat(&g_pb);
        g_cfg.repeat_default = (int)g_pb.repeat;
        save_config();
        return;
    }

    /* glass panel rows (Library / Queue / Home / Settings) */
    if (py >= UI_LIST_Y0 && py < UI_LIST_Y0 + UI_LIST_ROWS * UI_LIST_ROW_H) {
        int row = (py - UI_LIST_Y0) / UI_LIST_ROW_H;
        if (g_view == VIEW_LIB && g_lib.count) {
            int start = g_sel > UI_LIST_ROWS - 1
                      ? g_sel - (UI_LIST_ROWS - 1) : 0;
            int idx = start + row;
            if (idx < g_lib.count) { g_sel = idx; play_track(idx); }
        } else if (g_view == VIEW_QUEUE && g_pb.order_len) {
            int start = g_qsel > UI_LIST_ROWS - 1
                      ? g_qsel - (UI_LIST_ROWS - 1) : 0;
            int q = start + row;
            if (q < g_pb.order_len) {
                g_qsel = q;
                pb_play_order_pos(&g_pb, q);
                load_track(pb_current_index(&g_pb));
                do_resume();
                g_view = VIEW_NOW;
            }
        } else if (g_view == VIEW_HOME) {
            static const int dest[4] = {VIEW_NOW, VIEW_LIB, VIEW_QUEUE,
                                        VIEW_SETTINGS};
            if (row < 4) { g_menu = row; g_view = dest[row]; }
        } else if (g_view == VIEW_SETTINGS) {
            if (row == 0) {
                g_cfg.anim = (AnimLevel)(((int)g_cfg.anim + 1) % 3);
                save_config();
            } else if (row == 1) {
                pb_set_shuffle(&g_pb, !g_pb.shuffle);
                g_cfg.shuffle_default = g_pb.shuffle;
                save_config();
            } else if (row == 2) {
                pb_cycle_repeat(&g_pb);
                g_cfg.repeat_default = (int)g_pb.repeat;
                save_config();
            } else if (row == 3) {
                g_cfg.volume = (g_cfg.volume + 32) % 128;
                audio_hw_set_volume(g_cfg.volume);
                save_config();
            }
        }
    }
}

int main(void) {
    /* Pure 2D engine: BG3 art, BG1 16px lyrics, BG2/BG0 console text. */
    videoSetMode(MODE_0_2D);
    videoSetModeSub(MODE_0_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankC(VRAM_C_SUB_BG);
    static PrintConsole con_top, con_bot;
    /* Top console on BG2 (BG1 is the 16px lyric layer). */
    g_con_top = consoleInit(&con_top, 2, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
    g_con_bot = consoleInit(&con_bot, 0, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);

    backdrop_init(g_con_top, g_con_bot);
    ui_font_init(g_con_top, g_con_bot);

    config_defaults(&g_cfg);
    FILE *cf = NULL;

    if (!fatInitDefault()) {
        ui_text_clear(0);
        draw_text(0, 2, 1, "MIXTAPEDSi", 1, 12);
        draw_text(0, 2, 6, "SD CARD NOT READY", 1, 24);
        draw_center(0, 9, "Check the card and restart", 2, 28);
        while (1) swiWaitForVBlank();
    }
    if (chdir("sd:/") != 0) chdir("fat:/");
    ui_likes_load();

    /* N.B.: file access only AFTER fatInitDefault. */
    cf = fopen("mixtapedsi.cfg", "r");
    if (cf) {
        char cbuf[1024];
        size_t n = fread(cbuf, 1, sizeof(cbuf) - 1, cf);
        cbuf[n] = '\0';
        fclose(cf);
        config_parse(&g_cfg, cbuf);
    }
    /* v1.0.0 rule: lyrics live on the TOP screen only. Ignore any
     * stored lyrics_screen value from older configs. */
    g_cfg.lyrics_screen = LYRICS_TOP;

    library_init(&g_lib, g_cfg.music_dir);
    extern int nds_scan(MusicLibrary *lib);
    nds_scan(&g_lib);
    pb_init(&g_pb, &g_lib);
    pb_set_shuffle(&g_pb, g_cfg.shuffle_default);
    pb_set_repeat(&g_pb, (RepeatMode)g_cfg.repeat_default);
    g_pb.sample_rate = 44100;
    if (g_lib.count > 0) {
        load_track(0);
        do_pause();
    }
    g_view = VIEW_NOW;

    while (1) {
        scanKeys();
        uint32_t down = keysDown();
        uint32_t held = keysHeld();
        uint32_t up = keysUp();

        /* Bounded decode work: keeps audio alive through UI + browsing. */
        if (g_pb.state == PB_PLAYING && g_have_track) {
            audio_hw_pump();
            if (audio_hw_drained()) {
                if (pb_on_track_end(&g_pb))
                    load_track(pb_current_index(&g_pb));
                else {
                    pb_stop(&g_pb);
                    audio_hw_close();
                    mp3_close(&g_dec);
                    g_have_track = 0;
                }
            }
            /* mirror HW clock into controller (UI + tests compat) */
            g_pb.sample_rate = audio_hw_rate();
            pb_seek_ms(&g_pb, audio_hw_position_ms());
        }
        audio_hw_update();

        static int s_lid_closed = 0;
        int lid_now = (held & KEY_LID) ? 1 : 0;
        if (lid_now != s_lid_closed) {
            s_lid_closed = lid_now;
            if (s_lid_closed) {
                powerOff(PM_BACKLIGHT_TOP | PM_BACKLIGHT_BOTTOM);
            } else {
                powerOn(PM_BACKLIGHT_TOP | PM_BACKLIGHT_BOTTOM);
            }
        }

        if (down & KEY_START) {
            /* cycle bottom-deck views; lyrics stay on top throughout */
            do { g_view = (g_view + 1) % VIEW_COUNT; } while (g_view == VIEW_LYRICS);
        }
        if (g_view == VIEW_HOME) {
            if (down & KEY_DOWN) g_menu = (g_menu + 1) % 4;
            if (down & KEY_UP) g_menu = (g_menu + 3) % 4;
            if (down & KEY_A) {
                static const int dest[4] = {VIEW_NOW, VIEW_LIB, VIEW_QUEUE, VIEW_SETTINGS};
                g_view = dest[g_menu];
            }
        } else if (g_view == VIEW_LIB) {
            if (g_lib.count) {
                if (down & KEY_DOWN) g_sel = (g_sel + 1) % g_lib.count;
                if (down & KEY_UP) g_sel = (g_sel - 1 + g_lib.count) % g_lib.count;
                if (down & KEY_A) {
                    pb_play_index(&g_pb, g_sel);
                    load_track(g_sel);
                    g_view = VIEW_NOW;
                }
            }
            if (down & KEY_B) g_view = VIEW_HOME;
        } else if (g_view == VIEW_QUEUE) {
            if (g_pb.order_len) {
                if (down & KEY_DOWN) g_qsel = (g_qsel + 1) % g_pb.order_len;
                if (down & KEY_UP) g_qsel = (g_qsel - 1 + g_pb.order_len) % g_pb.order_len;
                if (down & KEY_A) {
                    pb_play_order_pos(&g_pb, g_qsel);
                    load_track(pb_current_index(&g_pb));
                    g_view = VIEW_NOW;
                }
            }
            if (down & KEY_B) g_view = VIEW_HOME;
        } else if (g_view == VIEW_SETTINGS) {
            int dirty = 0;
            if (down & KEY_LEFT || down & KEY_RIGHT) {
                /* animation level: FULL -> REDUCED -> OFF */
                g_cfg.anim = (AnimLevel)(((int)g_cfg.anim + 1) % 3);
                dirty = 1;
            }
            if (down & KEY_A) {
                pb_set_shuffle(&g_pb, !g_pb.shuffle);
                g_cfg.shuffle_default = g_pb.shuffle;
                dirty = 1;
            }
            if (down & KEY_Y) {
                pb_cycle_repeat(&g_pb);
                g_cfg.repeat_default = (int)g_pb.repeat;
                dirty = 1;
            }
            if (down & KEY_UP && g_cfg.volume < 127) { g_cfg.volume++; audio_hw_set_volume(g_cfg.volume); dirty = 1; }
            if (down & KEY_DOWN && g_cfg.volume > 0) { g_cfg.volume--; audio_hw_set_volume(g_cfg.volume); dirty = 1; }
            if (dirty) save_config();
            if (down & KEY_B) g_view = VIEW_HOME;
            if (down & KEY_X) g_view = VIEW_NOW;
        } else {
            if (down & KEY_A) {
                if (g_have_track) {
                    if (g_pb.state == PB_PLAYING) do_pause();
                    else do_resume();
                } else if (g_lib.count > 0) {
                    load_track(0);
                    do_resume();
                }
            }
            if (down & KEY_X) g_view = VIEW_NOW;
            if (down & KEY_Y) pb_cycle_repeat(&g_pb);
            if ((down & KEY_R) && g_lib.count) {
                if (!g_have_track) { load_track(0); do_resume(); }
                else if (pb_next(&g_pb)) { load_track(pb_current_index(&g_pb)); do_resume(); }
            }
            if ((down & KEY_L) && g_lib.count) {
                if (!g_have_track) { load_track(0); do_resume(); }
                else if (pb_prev(&g_pb)) { load_track(pb_current_index(&g_pb)); do_resume(); }
            }
            if (down & KEY_B) g_view = VIEW_HOME;
            if (down & KEY_SELECT) {
                int ci = pb_current_index(&g_pb);
                if (ci >= 0 && g_have_track) {
                    ui_like_toggle(g_lib.tracks[ci].path);
                    ui_heart(ui_liked(g_lib.tracks[ci].path));
                }
            }
        }

        /* ---- stylus: timeline scrub, tabs, heart, transport, list rows ---- */
        {
            touchPosition tch;
            int touching = (held & KEY_TOUCH) != 0;
            memset(&tch, 0, sizeof(tch));
            if (touching) touchRead(&tch);

            /* Start scrubbing when the pen goes down on the seek rail. */
            if (g_have_track && !s_scrub && touching &&
                tch.py >= UI_SEEK_Y0 && tch.py <= UI_SEEK_Y1 &&
                tch.px >= UI_SEEK_X0 && tch.px <= UI_SEEK_X1) {
                s_scrub = 1;
            }
            /* While held, preview the target time (rail + lyric stage). */
            if (s_scrub && touching) {
                uint32_t total = track_total_ms();
                int span = UI_SEEK_X1 - UI_SEEK_X0;
                int x = tch.px - UI_SEEK_X0;
                if (x < 0) x = 0;
                if (x > span) x = span;
                s_scrub_ms = total
                    ? (uint32_t)(((uint64_t)x * total) / (uint32_t)span) : 0;
            }
            /* Release (or lost pen) commits the seek. */
            if (s_scrub && (!touching || (up & KEY_TOUCH))) {
                if (g_have_track && track_total_ms() > 0) {
                    audio_hw_seek_ms(s_scrub_ms);
                    pb_seek_ms(&g_pb, s_scrub_ms);
                }
                s_scrub = 0;
            }
            /* Taps: only when this touch did not become a scrub. */
            if (!s_scrub && (down & KEY_TOUCH))
                handle_tap(tch.px, tch.py);
        }

        /* Keyboard seek: 5 s steps, repeating while the D-pad is held. */
        {
            static int s_seek_hold = 0;
            if (g_have_track && (held & (KEY_LEFT | KEY_RIGHT))) {
                if (s_seek_hold == 0 || (s_seek_hold % 12) == 0) {
                    uint32_t total = track_total_ms();
                    int32_t target = (int32_t)now_pos_ms() +
                                     ((held & KEY_RIGHT) ? 5000 : -5000);
                    if (target < 0) target = 0;
                    if (total && (uint32_t)target > total)
                        target = (int32_t)total;
                    audio_hw_seek_ms((uint32_t)target);
                    pb_seek_ms(&g_pb, (uint32_t)target);
                }
                s_seek_hold++;
            } else {
                s_seek_hold = 0;
            }
        }

        /* Redraw only when something visible changed: full console
         * redraws every frame steal ARM9 time from the MP3 decoder. */
        static int l_view = -1, l_sel = -1, l_sec = -1, l_idx = -2;
        static int l_state = -1, l_count = -1, l_rep = -1, l_sh = -1;
        static int l_menu = -1, l_anim = -1, l_vol = -1, l_eqt = -1;
        static int l_qsel = -1, l_trk = -2, l_like = -1;
        static int l_scrub = -1;
        static uint32_t l_scrub_ms = 0xFFFFFFFFu;
        uint32_t pos_now = now_pos_ms();
        int idx_now = lyric_active_index(&g_lrc, pos_now);
        int sec_now = (int)(pos_now / 1000);
        int eqt_now = (int)(pos_now / 500);
        int trk_now = pb_current_index(&g_pb);
        int ci = (trk_now >= 0 && g_have_track) ? ui_liked(g_lib.tracks[trk_now].path) : 0;
        int animated = (g_cfg.anim != ANIM_OFF);
        int tab_active = g_view == VIEW_LIB ? 0 :
                         g_view == VIEW_NOW ? 1 :
                         g_view == VIEW_QUEUE ? 2 : -1;
        if (g_view != l_view || g_sel != l_sel || sec_now != l_sec ||
            idx_now != l_idx || (int)g_pb.state != l_state ||
            g_lib.count != l_count || (int)g_pb.repeat != l_rep ||
            g_pb.shuffle != l_sh || g_menu != l_menu ||
            (int)g_cfg.anim != l_anim || g_cfg.volume != l_vol ||
            g_qsel != l_qsel || trk_now != l_trk || ci != l_like ||
            s_scrub != l_scrub || s_scrub_ms != l_scrub_ms ||
            (animated && eqt_now != l_eqt)) {
        /* repaint dynamic tiles to match state (deck always visible) */
        uint32_t total_now = track_total_ms();
        uint32_t pos_bar = pos_now, total_bar = total_now;
        if (!g_have_track) { pos_bar = 0; total_bar = 0; }
        ui_transport(g_pb.state == PB_PLAYING, g_pb.shuffle, g_pb.repeat != REPEAT_OFF);
        ui_heart(ci);
        ui_tabs(tab_active);
        ui_progress(pos_bar, total_bar);
        ui_progress_top(pos_bar, total_bar);
        ui_eq_top(animated ? audio_hw_level() : 0, eqt_now,
                  animated && g_pb.state == PB_PLAYING && g_have_track);
        ui_eq_bot(animated ? audio_hw_level() : 0, eqt_now,
                  animated && g_pb.state == PB_PLAYING && g_have_track);
        /* Bottom deck: never lyrics. VIEW_LYRICS is an alias for NOW. */
        switch (g_view) {
            case VIEW_HOME: view_home(); break;
            case VIEW_LYRICS:
            case VIEW_NOW: view_now_playing(); break;
            case VIEW_LIB: view_library(); break;
            case VIEW_QUEUE: view_queue(); break;
            default: view_settings(); break;
        }
        /* Top stage: always lyrics, on every bottom view. */
        view_lyrics();
        /* Tab labels last: view redraws clear the console text layer. */
        draw_tab_labels(tab_active);
        l_view = g_view; l_sel = g_sel; l_sec = sec_now; l_idx = idx_now;
        l_state = (int)g_pb.state; l_count = g_lib.count;
        l_rep = (int)g_pb.repeat; l_sh = g_pb.shuffle;
        l_menu = g_menu; l_anim = (int)g_cfg.anim; l_vol = g_cfg.volume;
        l_eqt = eqt_now;
        l_qsel = g_qsel; l_trk = trk_now; l_like = ci;
        l_scrub = s_scrub; l_scrub_ms = s_scrub_ms;
        }
        /* smooth lyric sweep/glide: every frame, independent of full
         * redraws */
        lyric_anim_tick(pos_now);
        {
            /* accent pulse at 2 Hz, independent of redraws (off when ANIM_OFF) */
            static int l_tick = -1;
            if (eqt_now != l_tick) {
                l_tick = eqt_now;
                if (g_cfg.anim != ANIM_OFF) {
                    extern void backdrop_tick(int phase);
                    backdrop_tick(eqt_now);
                }
            }
        }
        swiWaitForVBlank();
    }
    return 0;
}

#endif /* ARM9 */
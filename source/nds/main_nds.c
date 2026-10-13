/* MixtapeDSi - ARM9 entry point (devkitARM/libnds + maxmod + dr_mp3).
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
static int g_view = VIEW_HOME; /* boot to home: no autoplay */
static int g_sel = 0;          /* library selection */
static int g_qsel = 0;         /* queue selection */
static int g_menu = 0;         /* home menu selection */
static PrintConsole *g_con_top = NULL;
static PrintConsole *g_con_bot = NULL;

static void to_latin1(char *dst, const char *src, size_t n);

static uint32_t now_pos_ms(void) {
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

static void draw_header(const char *title) {
    ui_text_clear(0);
    draw_text(0, 2, 1, title, 1, 20);
    draw_text(0, 27, 1, "MIX", 3, 3);
}

static void draw_footer(const char *left, const char *right) {
    draw_text(0, 2, 22, left, 2, 20);
    if (right) {
        char tmp[40];
        to_latin1(tmp, right, sizeof(tmp));
        ui_text(0, 32 - (int)strlen(tmp), 22, tmp, 3, 12);
    }
}

static void draw_row(int row, int selected, const char *title,
                     const char *detail, int playing) {
    char title_buf[32];
    char detail_buf[32];
    to_latin1(title_buf, title ? title : "", sizeof(title_buf));
    to_latin1(detail_buf, detail ? detail : "", sizeof(detail_buf));
    title_buf[21] = '\0';
    detail_buf[10] = '\0';
    draw_text(0, 2, row, selected ? ">" : (playing ? "*" : ""), selected ? 1 : 3, 1);
    draw_text(0, 4, row, title_buf, selected ? 1 : 0, 21);
    draw_text(0, 25, row, detail_buf, 2, 7);
}

static void draw_empty_state(const char *title, const char *detail) {
    draw_center(0, 9, title, 1, 26);
    draw_center(0, 12, detail, 2, 26);
}

static void save_config(void) {
    FILE *f = fopen("mixtapedsi.cfg", "w");
    if (!f) return;
    fprintf(f, "lyrics_screen=%d\nauto_follow=%d\nanim=%d\nshuffle=%d\nrepeat=%d\nvolume=%d\nmusic_dir=%s\n",
            (int)g_cfg.lyrics_screen, g_cfg.auto_follow, (int)g_cfg.anim,
            g_cfg.shuffle_default, g_cfg.repeat_default, g_cfg.volume, g_cfg.music_dir);
    fclose(f);
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

static void view_home(void) {
    static const char *items[] = {"Now Playing", "Library", "Queue", "Settings"};
    draw_header("MIXTAPEDSi");
    {
        char count[24];
        snprintf(count, sizeof(count), "%d TRACK%s", g_lib.count,
                 g_lib.count == 1 ? "" : "S");
        draw_text(0, 2, 3, count, 2, 20);
    }
    for (int i = 0; i < 4; i++) {
        draw_row(6 + i * 3, i == g_menu, items[i],
                 i == 0 && g_have_track ? "READY" : "OPEN", 0);
    }
    if (!g_lib.count)
        draw_empty_state("Your library is quiet", "Add MP3s to the music folder");
    else if (!g_have_track)
        draw_empty_state("Choose a track", "Open Library to begin listening");
    draw_footer("UP/DOWN SELECT", "A OPEN");
}

static void view_now_playing(void) {
    int idx = pb_current_index(&g_pb);
    if (idx < 0 || !g_have_track) {
        draw_header("NOW PLAYING");
        draw_empty_state("Nothing playing", "Open Library to choose a track");
        draw_footer("B BACK", "START VIEWS");
        return;
    }
    LibraryTrack *t = &g_lib.tracks[idx];
    /* title/artist beside cover (cols 11+, 15 wide), latin-1 */
    ui_text_clear(0);
    draw_text(0, 11, 7, t->meta.title[0] ? t->meta.title : "Unknown", 0, 15);
    draw_text(0, 11, 9, t->meta.artist[0] ? t->meta.artist : "Unknown artist", 2, 15);
    uint32_t pos = now_pos_ms();
    uint32_t total = g_lrc.meta.has_length ? (uint32_t)g_lrc.meta.length_ms : 0;
    {
        char timebuf[32];
        snprintf(timebuf, sizeof(timebuf), "%02lu:%02lu",
                 (unsigned long)(pos / 60000), (unsigned long)((pos / 1000) % 60));
        draw_text(0, 11, 13, timebuf, 2, 8);
    }
    if (total > 0) {
        char timebuf[16];
        snprintf(timebuf, sizeof(timebuf), "%02lu:%02lu",
                 (unsigned long)(total / 60000), (unsigned long)((total / 1000) % 60));
        draw_text(0, 25, 13, timebuf, 2, 7);
    }
    ui_progress(pos, total);
    ui_transport(g_pb.state == PB_PLAYING, g_pb.shuffle, g_pb.repeat != REPEAT_OFF);
    ui_heart(ui_liked(t->path));
}

/* Print up to max_rows wrapped lines, centered, starting at start_row. */
static void print_lyric_block(const char *text,
                              int start_row, int max_rows, int color) {
    char scratch[256];
    const char *lines[4];
    int n = wrap_lines(text, 28, max_rows > 4 ? 4 : max_rows, scratch, lines);
    for (int i = 0; i < n; i++)
        draw_center(1, start_row + i, lines[i], color, 28);
}

static void view_lyrics(void) {
    /* Lyric stage: TOP screen only, always. The bottom screen never
     * shows lyrics (control deck). */
    int idx = pb_current_index(&g_pb);
    uint32_t pos = now_pos_ms();
    int prv, cur, nxt;
    lyric_window(&g_lrc, pos, &prv, &cur, &nxt);
    ui_text_clear(1);
    if (idx >= 0 && g_have_track) {
        LibraryTrack *t = &g_lib.tracks[idx];
        draw_center(1, 1, t->meta.title[0] ? t->meta.title : "Unknown", 1, 28);
        draw_center(1, 2, t->meta.artist[0] ? t->meta.artist : "", 2, 28);
    }
    if (prv >= 0) {
        print_lyric_block(g_lrc.lines[prv].text, 8, 2, 2);
    }
    if (cur >= 0) {
        print_lyric_block(g_lrc.lines[cur].text, 11, 2, 1);
    } else if (!g_have_track) {
        print_lyric_block("Nothing playing", 11, 1, 1);
        print_lyric_block("Pick a track in Library", 13, 1, 2);
    } else if (g_lrc.count == 0) {
        print_lyric_block("No synchronized lyrics", 11, 1, 2);
    } else {
        print_lyric_block("(intro...)", 11, 1, 2);
    }
    if (nxt >= 0) {
        print_lyric_block(g_lrc.lines[nxt].text, 14, 2, 2);
    }
    {
        uint32_t total = g_lrc.meta.has_length ? (uint32_t)g_lrc.meta.length_ms : 0;
        char timebuf[40];
        if (total > 0)
            snprintf(timebuf, sizeof(timebuf), "%02lu:%02lu / %02lu:%02lu",
                     (unsigned long)(pos / 60000), (unsigned long)((pos / 1000) % 60),
                     (unsigned long)(total / 60000), (unsigned long)((total / 1000) % 60));
        else if (g_have_track)
            snprintf(timebuf, sizeof(timebuf), "%02lu:%02lu",
                     (unsigned long)(pos / 60000), (unsigned long)((pos / 1000) % 60));
        else timebuf[0] = '\0';
        draw_center(1, 22, timebuf, 2, 24);
    }
    /* Lyric-change transition: short vertical glide + brightness is
     * handled by the redraw itself; the glide runs only on ANIM_FULL
     * so UI rendering never stalls the decode pump. */
    {
        static int last_idx = -99;
        if (cur != last_idx) {
            last_idx = cur;
            if (g_cfg.anim == ANIM_FULL) {
                for (int s = 6; s > 0; s -= 2) {
                    bgSetScroll(g_con_top->bgId, 0, -s);
                    swiWaitForVBlank();
                }
                bgSetScroll(g_con_top->bgId, 0, 0);
            }
        }
    }
}

static void view_library(void) {
    draw_header("Library");
    if (!g_lib.count) {
        draw_empty_state("No tracks yet", "Add MP3s to the music folder");
        draw_footer("B BACK", "START VIEWS");
        return;
    }
    int start = g_sel > 8 ? g_sel - 8 : 0;
    for (int row = 0, i = start; i < g_lib.count && row < 6; i++, row++) {
        LibraryTrack *t = &g_lib.tracks[i];
        draw_row(4 + row * 2, i == g_sel, t->meta.title,
                 t->meta.artist[0] ? t->meta.artist : "Unknown artist",
                 i == pb_current_index(&g_pb));
    }
    draw_footer("A PLAY  B BACK", "START VIEWS");
}

static void view_queue(void) {
    draw_header("Queue");
    if (!g_pb.order_len) {
        draw_empty_state("Queue is empty", "Choose a track from Library");
        draw_footer("B BACK", "START VIEWS");
        return;
    }
    int start = g_qsel > 8 ? g_qsel - 8 : 0;
    for (int row = 0, i = start; i < g_pb.order_len && row < 6; i++, row++) {
        int li = g_pb.order[i];
        LibraryTrack *t = &g_lib.tracks[li];
        draw_row(4 + row * 2, i == g_qsel, t->meta.title,
                 i == g_pb.order_pos ? "NOW PLAYING" : "UP NEXT",
                 i == g_pb.order_pos);
    }
    draw_footer("A PLAY  B BACK", "START VIEWS");
}

static void load_track(int idx) {
    audio_hw_close();
    mp3_close(&g_dec);
    g_have_track = 0;
    memset(&g_lrc, 0, sizeof(g_lrc));
    if (idx < 0 || idx >= g_lib.count) return;
    LibraryTrack *t = &g_lib.tracks[idx];
    if (mp3_open(&g_dec, t->path) != 0) return;
    if (audio_hw_open(&g_dec) != 0) { mp3_close(&g_dec); return; }
    audio_hw_set_volume(g_cfg.volume);
    ui_cover_show(t->cover_path);
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

int main(void) {
    videoSetMode(MODE_0_2D);
    videoSetModeSub(MODE_0_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankC(VRAM_C_SUB_BG);
    static PrintConsole con_top, con_bot;
    g_con_top = consoleInit(&con_top, 0, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
    g_con_bot = consoleInit(&con_bot, 0, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);

    extern void backdrop_init(PrintConsole *t, PrintConsole *b);
    backdrop_init(g_con_top, g_con_bot);
    ui_font_init(g_con_top, g_con_bot);

    config_defaults(&g_cfg);
    FILE *cf = NULL;

    if (!fatInitDefault()) {
        draw_header("Error");
        iprintf("SD init failed.\n");
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
    /* Redesign rule: lyrics live on the TOP screen only. Ignore any
     * stored lyrics_screen value from older configs. */
    g_cfg.lyrics_screen = LYRICS_TOP;

    library_init(&g_lib, g_cfg.music_dir);
    extern int nds_scan(MusicLibrary *lib);
    nds_scan(&g_lib);
    pb_init(&g_pb, &g_lib);
    pb_set_shuffle(&g_pb, g_cfg.shuffle_default);
    pb_set_repeat(&g_pb, (RepeatMode)g_cfg.repeat_default);
    g_pb.sample_rate = 44100;
    /* No autoplay: home screen first, user picks a track. */

    while (1) {
        scanKeys();
        uint32_t down = keysDown();
        uint32_t held = keysHeld();

        /* Bounded decode work: keeps audio alive through UI + browsing.
         * Never call systemSleep(): lid-close must not stop the engine. */
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
            pb_seek_ms(&g_pb, now_pos_ms());
        }
        audio_hw_update();

        if (down & KEY_LID) {
            /* lid event deliberately ignored: decoding continues,
             * backlights stay as-is; audio keeps playing. */
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
            if ((down & KEY_A) && g_have_track) {
                if (g_pb.state == PB_PLAYING) do_pause();
                else do_resume();
            }
            if (down & KEY_X) g_view = VIEW_NOW;
            if (down & KEY_Y) pb_cycle_repeat(&g_pb);
            if ((down & KEY_R) && g_lib.count) {
                if (pb_next(&g_pb)) { load_track(pb_current_index(&g_pb)); do_resume(); }
            }
            if ((down & KEY_L) && g_lib.count) {
                if (pb_prev(&g_pb)) { load_track(pb_current_index(&g_pb)); do_resume(); }
            }
            if (down & KEY_B) g_view = VIEW_HOME;
            if (down & KEY_SELECT) {
                int ci = pb_current_index(&g_pb);
                if (ci >= 0 && g_have_track) {
                    ui_like_toggle(g_lib.tracks[ci].path);
                    ui_heart(ui_liked(g_lib.tracks[ci].path));
                }
            }
            if ((down & KEY_TOUCH) && g_view == VIEW_NOW) {
                touchPosition tch;
                touchRead(&tch);
                int ci = pb_current_index(&g_pb);
                if ((tch.px - UI_B_PLAY_X) * (tch.px - UI_B_PLAY_X) +
                    (tch.py - UI_B_Y) * (tch.py - UI_B_Y) < UI_B_R * UI_B_R * 4) {
                    if (g_have_track) {
                        if (g_pb.state == PB_PLAYING) do_pause();
                        else do_resume();
                    }
                } else if ((tch.px - UI_B_PREV_X) * (tch.px - UI_B_PREV_X) +
                           (tch.py - UI_B_Y) * (tch.py - UI_B_Y) < UI_B_R * UI_B_R * 4) {
                    if (pb_prev(&g_pb)) { load_track(pb_current_index(&g_pb)); do_resume(); }
                } else if ((tch.px - UI_B_NEXT_X) * (tch.px - UI_B_NEXT_X) +
                           (tch.py - UI_B_Y) * (tch.py - UI_B_Y) < UI_B_R * UI_B_R * 4) {
                    if (pb_next(&g_pb)) { load_track(pb_current_index(&g_pb)); do_resume(); }
                } else if ((tch.px - UI_B_SHUF_X) * (tch.px - UI_B_SHUF_X) +
                           (tch.py - UI_B_Y) * (tch.py - UI_B_Y) < UI_B_R * UI_B_R * 4) {
                    pb_set_shuffle(&g_pb, !g_pb.shuffle);
                    g_cfg.shuffle_default = g_pb.shuffle;
                    save_config();
                } else if ((tch.px - UI_B_REP_X) * (tch.px - UI_B_REP_X) +
                           (tch.py - UI_B_Y) * (tch.py - UI_B_Y) < UI_B_R * UI_B_R * 4) {
                    pb_cycle_repeat(&g_pb);
                    g_cfg.repeat_default = (int)g_pb.repeat;
                    save_config();
                } else if (tch.px >= UI_HEART_X0 && tch.px <= UI_HEART_X1 &&
                           tch.py >= UI_HEART_Y0 && tch.py <= UI_HEART_Y1) {
                    if (ci >= 0 && g_have_track) {
                        ui_like_toggle(g_lib.tracks[ci].path);
                        ui_heart(ui_liked(g_lib.tracks[ci].path));
                    }
                } else if (tch.py >= UI_TAB_Y0 && tch.py <= UI_TAB_Y1) {
                    if (tch.px >= UI_TAB0_X0 && tch.px <= UI_TAB0_X1) g_view = VIEW_LIB;
                    else if (tch.px >= UI_TAB1_X0 && tch.px <= UI_TAB1_X1) g_view = VIEW_NOW;
                    else if (tch.px >= UI_TAB2_X0 && tch.px <= UI_TAB2_X1) g_view = VIEW_QUEUE;
                }
            }
            if (g_have_track && g_pb.state == PB_PLAYING) {
                if (held & KEY_LEFT) {
                    uint32_t p = now_pos_ms();
                    uint32_t t = p > 5000 ? p - 5000 : 0;
                    audio_hw_seek_ms(t);
                }
                if (held & KEY_RIGHT) {
                    uint32_t p = now_pos_ms();
                    audio_hw_seek_ms(p + 5000);
                }
            }
        }

        /* Redraw only when something visible changed: full console
         * redraws every frame steal ARM9 time from the MP3 decoder. */
        static int l_view = -1, l_sel = -1, l_sec = -1, l_idx = -2;
        static int l_state = -1, l_count = -1, l_rep = -1, l_sh = -1;
        static int l_menu = -1, l_anim = -1, l_vol = -1, l_eqt = -1;
        static int l_qsel = -1, l_trk = -2, l_like = -1;
        uint32_t pos_now = now_pos_ms();
        int idx_now = lyric_active_index(&g_lrc, pos_now);
        int sec_now = (int)(pos_now / 1000);
        int eqt_now = (int)(pos_now / 500);
        int trk_now = pb_current_index(&g_pb);
        int ci = (trk_now >= 0 && g_have_track) ? ui_liked(g_lib.tracks[trk_now].path) : 0;
        int animated = (g_cfg.anim != ANIM_OFF);
        if (g_view != l_view || g_sel != l_sel || sec_now != l_sec ||
            idx_now != l_idx || (int)g_pb.state != l_state ||
            g_lib.count != l_count || (int)g_pb.repeat != l_rep ||
            g_pb.shuffle != l_sh || g_menu != l_menu ||
            (int)g_cfg.anim != l_anim || g_cfg.volume != l_vol ||
            g_qsel != l_qsel || trk_now != l_trk || ci != l_like ||
            (animated && eqt_now != l_eqt)) {
        /* repaint dynamic tiles to match state */
        ui_transport(g_pb.state == PB_PLAYING, g_pb.shuffle, g_pb.repeat != REPEAT_OFF);
        ui_heart(ci);
        ui_tabs(g_view == VIEW_LIB ? 0 : g_view == VIEW_NOW ? 1 : g_view == VIEW_QUEUE ? 2 : -1);
        if (g_view == VIEW_NOW)
            ui_progress(pos_now, g_lrc.meta.has_length ? (uint32_t)g_lrc.meta.length_ms : 0);
        ui_eq(animated ? audio_hw_level() : 0, eqt_now,
              animated && g_pb.state == PB_PLAYING && g_have_track);
        /* Bottom deck: never lyrics. VIEW_LYRICS is an alias for NOW. */
        switch (g_view) {
            case VIEW_HOME: view_home(); break;
            case VIEW_LYRICS:
            case VIEW_NOW: view_now_playing(); break;
            case VIEW_LIB: view_library(); break;
            case VIEW_QUEUE: view_queue(); break;
            default: {
                static const char *anim_name[] = {"FULL", "REDUCED", "OFF"};
                draw_header("Settings");
                draw_row(5, 0, "Lyrics stage", "TOP ONLY", 0);
                draw_row(8, 0, "Animation", anim_name[(int)g_cfg.anim], 0);
                draw_row(11, 0, "Shuffle", g_pb.shuffle ? "ON" : "OFF", 0);
                draw_row(14, 0, "Repeat", g_pb.repeat == REPEAT_ONE ? "ONE" :
                         g_pb.repeat == REPEAT_ALL ? "ALL" : "OFF", 0);
                {
                    char volume[16];
                    snprintf(volume, sizeof(volume), "%d%%", g_cfg.volume * 100 / 127);
                    draw_row(17, 0, "Volume", volume, 0);
                }
                draw_footer("L/R ANIM  UP/DN VOL", "B BACK");
                break;
            }
        }
        /* Top stage: always lyrics, on every bottom view. */
        view_lyrics();
        l_view = g_view; l_sel = g_sel; l_sec = sec_now; l_idx = idx_now;
        l_state = (int)g_pb.state; l_count = g_lib.count;
        l_rep = (int)g_pb.repeat; l_sh = g_pb.shuffle;
        l_menu = g_menu; l_anim = (int)g_cfg.anim; l_vol = g_cfg.volume;
        l_eqt = eqt_now;
        l_qsel = g_qsel; l_trk = trk_now; l_like = ci;
        }
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

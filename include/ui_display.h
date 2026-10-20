#ifndef MIXTAPEDSI_UI_DISPLAY_H
#define MIXTAPEDSI_UI_DISPLAY_H

/* MixtapeDSi v1.0.7 graphical tile UI (ARM9 only).
 * "Midnight Glass" theme; text colors: 0 main, 1 aqua, 2 muted,
 * 3 violet, 4 dim. */
#include <stdint.h>
#include <nds.h>

#ifdef __cplusplus
extern "C" {
#endif

void backdrop_init(PrintConsole *con_top, PrintConsole *con_bot);
void backdrop_tick(int phase);
void ui_font_init(PrintConsole *con_top, PrintConsole *con_bot);
void ui_text_clear(int top);
void ui_text(int top, int x, int y, const char *text, int color, int max_cols);
void ui_text_center(int top, int y, const char *text, int color, int max_cols);
/* 16px Mixtape Sans layer (two tile rows per glyph). color: 0 ink, 1 aqua, 2 muted. */
void ui_text16(int top, int x, int y, const char *text, int color, int max_cols);
void ui_text16_center(int top, int y, const char *text, int color, int max_cols);
/* Karaoke span: the first sung_chars glyphs use sung_color, the rest
 * unsung_color. Both colors use the 16px palette banks (0 ink, 1 aqua,
 * 2 muted). x is the first column; the string is clipped to max_cols. */
void ui_text16_span(int top, int x, int y, const char *text, int sung_chars,
                    int sung_color, int unsung_color, int max_cols);
/* Clear whole 16px text tile rows (r0..r1 inclusive). */
void ui_text16_clear_rows(int top, int r0, int r1);
/* Smooth sub-tile glide of the 16px lyric layer (1/60 s steps). */
void ui_big_scroll(int top, int y_px);

/* dynamic tiles */
void ui_progress(uint32_t pos_ms, uint32_t total_ms);      /* bottom 2-row rail */
void ui_progress_top(uint32_t pos_ms, uint32_t total_ms);  /* top lyric rail */
void ui_transport(int playing, int shuffle, int repeat);
void ui_heart(int liked);
void ui_tabs(int active); /* 0=lib,1=now,2=queue, else none */
/* Replace the deck card interior with a flat panel (1) or restore the
 * vinyl art (0). Used by Library/Queue/Home/Settings. */
void ui_panel(int on);
void ui_cover_show(const char *bmp_path); /* NULL/failed -> placeholder */
void ui_eq_top(int level08, int tick, int active);
void ui_eq_bot(int level08, int tick, int active);

void ui_likes_load(void);
int ui_liked(const char *path);
void ui_like_toggle(const char *path);

#ifdef __cplusplus
}
#endif

#endif

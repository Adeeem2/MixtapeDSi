#ifndef MIXTAPEDSI_UI_DISPLAY_H
#define MIXTAPEDSI_UI_DISPLAY_H

/* Graphical tile UI (ARM9 only). */
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

/* bottom-screen dynamic tiles */
void ui_progress(uint32_t pos_ms, uint32_t total_ms);
void ui_transport(int playing, int shuffle, int repeat);
void ui_heart(int liked);
void ui_tabs(int active); /* 0=lib,1=now,2=queue, else none */
void ui_cover_show(const char *bmp_path); /* NULL/failed -> placeholder */
void ui_eq(int level08, int tick, int active);

void ui_likes_load(void);
int ui_liked(const char *path);
void ui_like_toggle(const char *path);

#ifdef __cplusplus
}
#endif

#endif

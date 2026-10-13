#ifndef MIXTAPEDSI_CONFIG_H
#define MIXTAPEDSI_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { LYRICS_TOP = 0, LYRICS_BOTTOM = 1 } LyricsScreen;
typedef enum { ANIM_FULL = 0, ANIM_REDUCED = 1, ANIM_OFF = 2 } AnimLevel;

typedef struct {
    char music_dir[128];   /* default "/music/" */
    LyricsScreen lyrics_screen;
    int auto_follow;       /* bool: lyrics auto-scroll */
    AnimLevel anim;
    int shuffle_default;
    int repeat_default;    /* RepeatMode */
    int volume;            /* 0..127 */
} AppConfig;

void config_defaults(AppConfig *c);
/* Parse "key=value" lines; unknown keys ignored. Returns 0 always. */
int config_parse(AppConfig *c, const char *text);

#ifdef __cplusplus
}
#endif

#endif

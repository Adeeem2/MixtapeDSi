#include "app_config.h"
#include <string.h>
#include <stdlib.h>

void config_defaults(AppConfig *c) {
    memset(c, 0, sizeof(*c));
    strcpy(c->music_dir, "/music/");
    c->lyrics_screen = LYRICS_TOP;
    c->auto_follow = 1;
    c->anim = ANIM_FULL;
    c->shuffle_default = 0;
    c->repeat_default = 0;
    c->volume = 100;
}

static void valcpy(char *d, size_t n, const char *v) {
    strncpy(d, v, n - 1); d[n - 1] = '\0';
}

int config_parse(AppConfig *c, const char *text) {
    if (!c || !text) return -1;
    const char *p = text;
    char line[256];
    while (*p) {
        size_t i = 0;
        while (*p && *p != '\n' && i + 1 < sizeof(line)) line[i++] = *p++;
        if (*p == '\n') p++;
        line[i] = '\0';
        if (i > 0 && line[i-1] == '\r') line[i-1] = '\0';
        if (!line[0] || line[0] == '#') continue;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char *k = line, *v = eq + 1;
        if (strcmp(k, "music_dir") == 0) valcpy(c->music_dir, sizeof(c->music_dir), v);
        else if (strcmp(k, "lyrics_screen") == 0)
            c->lyrics_screen = (atoi(v) == 1) ? LYRICS_BOTTOM : LYRICS_TOP;
        else if (strcmp(k, "auto_follow") == 0) c->auto_follow = atoi(v) ? 1 : 0;
        else if (strcmp(k, "anim") == 0) {
            int a = atoi(v);
            c->anim = (a < 0 || a > 2) ? ANIM_FULL : (AnimLevel)a;
        }
        else if (strcmp(k, "shuffle") == 0) c->shuffle_default = atoi(v) ? 1 : 0;
        else if (strcmp(k, "repeat") == 0) {
            int r = atoi(v);
            c->repeat_default = (r < 0 || r > 2) ? 0 : r;
        }
        else if (strcmp(k, "volume") == 0) {
            int vol = atoi(v);
            if (vol < 0) vol = 0;
            if (vol > 127) vol = 127;
            c->volume = vol;
        }
    }
    return 0;
}

#ifndef MIXTAPEDSI_LRC_PARSER_H
#define MIXTAPEDSI_LRC_PARSER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_LYRIC_LINES 256
#define MAX_LYRIC_LENGTH 256
#define MAX_LRC_TAG_FIELD 96

typedef struct {
    uint32_t timestamp_ms;
    char text[MAX_LYRIC_LENGTH];
} LyricLine;

typedef struct {
    char title[MAX_LRC_TAG_FIELD];
    char artist[MAX_LRC_TAG_FIELD];
    char album[MAX_LRC_TAG_FIELD];
    char by[MAX_LRC_TAG_FIELD];
    char author_re[MAX_LRC_TAG_FIELD]; /* [re:] creator field */
    char version[MAX_LRC_TAG_FIELD];   /* [ve:] */
    int32_t length_ms;   /* -1 when absent */
    int has_length;
} LrcMeta;

typedef struct {
    LrcMeta meta;
    LyricLine lines[MAX_LYRIC_LINES];
    int count;          /* number of valid lines */
    int truncated;      /* 1 if input exceeded MAX_LYRIC_LINES */
} LrcSong;

/* Parse NUL-terminated LRC text into out. Returns 0 on success.
 * Never fails on malformed input: bad lines are skipped, out->count
 * reflects the usable timeline (possibly 0). */
int lrc_parse(const char *text, LrcSong *out);

/* Parse a metadata length value "mm:ss", "mm:ss.xx" into ms.
 * Returns -1 on failure. */
int32_t lrc_parse_length_ms(const char *s);

#ifdef __cplusplus
}
#endif

#endif

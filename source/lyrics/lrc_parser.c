#include "lrc_parser.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* ---- helpers ---- */

static void copy_field(char *dst, size_t dstn, const char *src, size_t srclen) {
    if (dstn == 0) return;
    if (srclen >= dstn) srclen = dstn - 1;
    memcpy(dst, src, srclen);
    dst[srclen] = '\0';
}

static void trim(char *s) {
    /* leading */
    size_t n = strlen(s), i = 0;
    while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r')) i++;
    if (i) memmove(s, s + i, n - i + 1);
    /* trailing */
    n = strlen(s);
    while (n > 0 && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\r')) s[--n] = '\0';
}

/* Parse "mm:ss.xx" / "mm:ss.xxx" / "mm:ss" -> ms. Returns -1 on error. */
static int32_t parse_timestamp(const char *s, size_t len) {
    /* s like 00:03.86 (len given). Copy to temp. */
    char tmp[32];
    if (len == 0 || len >= sizeof(tmp)) return -1;
    memcpy(tmp, s, len);
    tmp[len] = '\0';
    /* find ':' */
    char *colon = strchr(tmp, ':');
    if (!colon) return -1;
    *colon = '\0';
    char *min_s = tmp, *sec_s = colon + 1;
    /* minutes: 1-3 digits */
    for (const char *p = min_s; *p; p++)
        if (!isdigit((unsigned char)*p)) return -1;
    if (strlen(min_s) == 0 || strlen(min_s) > 3) return -1;
    /* seconds part: ss[.xx[x]] */
    char *dot = strchr(sec_s, '.');
    char *frac = NULL;
    if (dot) { *dot = '\0'; frac = dot + 1; }
    if (strlen(sec_s) != 2) return -1;
    if (!isdigit((unsigned char)sec_s[0]) || !isdigit((unsigned char)sec_s[1])) return -1;
    int mins = atoi(min_s);
    int secs = atoi(sec_s);
    if (secs >= 60 || mins >= 100) return -1;
    int32_t ms = (int32_t)((mins * 60 + secs) * 1000);
    if (frac) {
        size_t fl = strlen(frac);
        if (fl < 2 || fl > 3) return -1;
        for (size_t i = 0; i < fl; i++)
            if (!isdigit((unsigned char)frac[i])) return -1;
        if (fl == 2) ms += (frac[0]-'0')*100 + (frac[1]-'0')*10;
        else ms += (frac[0]-'0')*100 + (frac[1]-'0')*10 + (frac[2]-'0');
    }
    return ms;
}

int32_t lrc_parse_length_ms(const char *s) {
    while (*s == ' ' || *s == '\t') s++;
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' || s[len-1] == '\r')) len--;
    return parse_timestamp(s, len);
}

static int tag_is(const char *tag, size_t taglen, const char *name) {
    size_t nl = strlen(name);
    if (taglen != nl) return 0;
    for (size_t i = 0; i < nl; i++)
        if (tolower((unsigned char)tag[i]) != name[i]) return 0;
    return 1;
}

static int line_compare(const void *a, const void *b) {
    uint32_t ta = ((const LyricLine*)a)->timestamp_ms;
    uint32_t tb = ((const LyricLine*)b)->timestamp_ms;
    if (ta < tb) return -1;
    if (ta > tb) return 1;
    return 0; /* stable-ish: qsort not stable but duplicates group together */
}

int lrc_parse(const char *text, LrcSong *out) {
    if (!text || !out) return -1;
    memset(out, 0, sizeof(*out));
    out->meta.length_ms = -1;

    const char *p = text;
    char linebuf[1024];

    while (*p) {
        /* extract one physical line (handle \n, \r\n, \r) */
        size_t li = 0;
        while (*p && *p != '\n' && *p != '\r' && li + 1 < sizeof(linebuf))
            linebuf[li++] = *p++;
        /* skip remainder of overlong line */
        while (*p && *p != '\n' && *p != '\r') p++;
        if (*p == '\r') p++;
        if (*p == '\n') p++;
        linebuf[li] = '\0';

        /* Scan all [...] brackets in this line, left to right. */
        uint32_t stamps[16];
        int nstamps = 0;
        size_t last_bracket_end = 0; /* offset just past last ']' */
        int has_any_bracket = 0;

        size_t i = 0;
        while (linebuf[i]) {
            if (linebuf[i] != '[') { i++; continue; }
            const char *close = strchr(linebuf + i, ']');
            if (!close) break; /* malformed: no closing */
            size_t inner_len = (size_t)(close - (linebuf + i + 1));
            const char *inner = linebuf + i + 1;
            has_any_bracket = 1;
            /* metadata? name: value */
            const char *colon = memchr(inner, ':', inner_len);
            int consumed_as_meta = 0;
            if (colon) {
                size_t namelen = (size_t)(colon - inner);
                const char *val = colon + 1;
                size_t vallen = inner_len - namelen - 1;
                if (tag_is(inner, namelen, "ti"))
                    { copy_field(out->meta.title, sizeof(out->meta.title), val, vallen); consumed_as_meta = 1; }
                else if (tag_is(inner, namelen, "ar"))
                    { copy_field(out->meta.artist, sizeof(out->meta.artist), val, vallen); consumed_as_meta = 1; }
                else if (tag_is(inner, namelen, "al"))
                    { copy_field(out->meta.album, sizeof(out->meta.album), val, vallen); consumed_as_meta = 1; }
                else if (tag_is(inner, namelen, "by"))
                    { copy_field(out->meta.by, sizeof(out->meta.by), val, vallen); consumed_as_meta = 1; }
                else if (tag_is(inner, namelen, "re"))
                    { copy_field(out->meta.author_re, sizeof(out->meta.author_re), val, vallen); consumed_as_meta = 1; }
                else if (tag_is(inner, namelen, "ve"))
                    { copy_field(out->meta.version, sizeof(out->meta.version), val, vallen); consumed_as_meta = 1; }
                else if (tag_is(inner, namelen, "length")) {
                    char tmp[32];
                    size_t cl = vallen < sizeof(tmp)-1 ? vallen : sizeof(tmp)-1;
                    memcpy(tmp, val, cl); tmp[cl] = '\0';
                    int32_t ms = lrc_parse_length_ms(tmp);
                    if (ms >= 0) { out->meta.length_ms = ms; out->meta.has_length = 1; }
                    consumed_as_meta = 1;
                }
            }
            if (!consumed_as_meta) {
                int32_t ms = parse_timestamp(inner, inner_len);
                if (ms >= 0) {
                    if (nstamps < (int)(sizeof(stamps)/sizeof(stamps[0])))
                        stamps[nstamps++] = (uint32_t)ms;
                }
                /* else: malformed bracket content -> ignore */
            }
            last_bracket_end = (size_t)(close - linebuf) + 1;
            i = last_bracket_end;
            void *tmp_use = NULL; (void)tmp_use;
        }

        if (nstamps > 0) {
            /* Lyric text = remainder after last ']' */
            const char *txt = linebuf + last_bracket_end;
            char tmp[1024];
            strncpy(tmp, txt, sizeof(tmp)-1);
            tmp[sizeof(tmp)-1] = '\0';
            trim(tmp);
            for (int k = 0; k < nstamps; k++) {
                if (out->count >= MAX_LYRIC_LINES) { out->truncated = 1; break; }
                LyricLine *L = &out->lines[out->count++];
                L->timestamp_ms = stamps[k];
                {
                    size_t tl = strlen(tmp);
                    if (tl >= MAX_LYRIC_LENGTH) tl = MAX_LYRIC_LENGTH - 1;
                    memcpy(L->text, tmp, tl);
                    L->text[tl] = '\0';
                }
            }
        } else {
            /* No timestamps: line is metadata-only, blank, or garbage.
             * Nothing to do (metadata already captured). */
            (void)has_any_bracket;
        }
    }

    /* Sort chronologically (input may be unsorted). */
    if (out->count > 1)
        qsort(out->lines, (size_t)out->count, sizeof(LyricLine), line_compare);

    return 0;
}

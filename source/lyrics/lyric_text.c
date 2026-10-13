#include "lyric_text.h"
#include <string.h>

uint32_t utf8_decode(const char **p) {
    const unsigned char *s = (const unsigned char *)*p;
    uint32_t cp;
    if (s[0] < 0x80) { *p += 1; return s[0]; }
    if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        cp = ((uint32_t)(s[0] & 0x1F) << 6) | (s[1] & 0x3F);
        *p += 2;
        return cp;
    }
    if ((s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        cp = ((uint32_t)(s[0] & 0x0F) << 12) | ((uint32_t)(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        *p += 3;
        return cp;
    }
    if ((s[0] & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 &&
        (s[3] & 0xC0) == 0x80) {
        cp = ((uint32_t)(s[0] & 0x07) << 18) | ((uint32_t)(s[1] & 0x3F) << 12) |
             ((uint32_t)(s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        *p += 4;
        return cp;
    }
    *p += 1;
    return 0xFFFD;
}

uint32_t display_fold(uint32_t cp) {
    if (cp < 0x80) return cp;
    if (cp >= 0xA0 && cp <= 0xFF) return cp;
    switch (cp) {
        case 0x2019: case 0x2018: return 0x27;
        case 0x201C: case 0x201D: return 0x22;
        case 0x2013: case 0x2014: return 0x2D;
        case 0x2026: return 0x2E;
        case 0x2190: return 0x3C;
        case 0x2192: return 0x3E;
        case 0x2665: return 0x2B;
        default: return 0x3F;
    }
}

/* copy at most max_cps codepoints (NUL-terminated); returns bytes written */
static size_t cpcopy(char *dst, const char *src, size_t src_bytes, int max_cps) {
    const char *p = src;
    const char *end = src + src_bytes;
    int c = 0;
    char *o = dst;
    while (p < end && c < max_cps) {
        const char *q = p;
        (void)utf8_decode(&q);
        size_t n = (size_t)(q - p);
        memcpy(o, p, n);
        o += n;
        p = q;
        c++;
    }
    *o = '\0';
    return (size_t)(o - dst);
}

int wrap_lines(const char *text, int max_chars, int max_lines,
               char *scratch, const char **lines) {
    if (max_lines <= 0 || max_chars <= 0) return 0;
    int nlines = 0;
    char *o = scratch;
    char *line_start = o;
    int line_w = 0;
    const char *p = text ? text : "";
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        const char *w = p;
        int ww = 0;
        while (*p && *p != ' ') { const char *q = p; (void)utf8_decode(&q); ww++; p = q; }
        size_t wbytes = (size_t)(p - w);
        if (line_w > 0 && line_w + 1 + ww > max_chars) {
            if (nlines + 1 >= max_lines) {
                int room = max_chars - line_w;
                if (room > 1) {
                    *o++ = ' ';
                    o += cpcopy(o, w, wbytes, room - 1);
                }
                *o++ = '\0';
                lines[nlines++] = line_start;
                return nlines;
            }
            *o++ = '\0';
            lines[nlines++] = line_start;
            line_start = o;
            line_w = 0;
        }
        if (line_w > 0) { *o++ = ' '; line_w++; }
        if (ww > max_chars - line_w) {
            o += cpcopy(o, w, wbytes, max_chars - line_w);
            *o++ = '\0';
            lines[nlines++] = line_start;
            if (nlines >= max_lines) return nlines;
            line_start = o;
            line_w = 0;
            continue;
        }
        memcpy(o, w, wbytes);
        o += wbytes;
        line_w += ww;
    }
    *o++ = '\0';
    if (nlines < max_lines) lines[nlines++] = line_start;
    return nlines;
}

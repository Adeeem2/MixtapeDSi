#include "metadata.h"
#include <string.h>
#include <stdlib.h>

static void set_field(char *dst, size_t n, const char *src, size_t slen) {
    if (n == 0) return;
    /* strip embedded NULs already handled by caller; trim spaces */
    while (slen > 0 && (src[slen-1] == ' ' || src[slen-1] == '\0')) slen--;
    size_t i = 0;
    while (i < slen && i + 1 < n && src[i]) { dst[i] = src[i]; i++; }
    /* also copy when src contains no NUL inside slen */
    while (i < slen && i + 1 < n) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static uint32_t syncsafe(const uint8_t *b) {
    return ((uint32_t)(b[0] & 0x7F) << 21) | ((uint32_t)(b[1] & 0x7F) << 14) |
           ((uint32_t)(b[2] & 0x7F) << 7) | (uint32_t)(b[3] & 0x7F);
}

/* Decode an ID3v2 text frame body to ASCII-ish (UTF-8 passthrough for
 * encoding 0x03; ISO-8859-1 copied byte-wise which preserves UTF-8-like
 * display for plain ASCII + passes through high bytes). */
static void decode_text_frame(const uint8_t *body, size_t body_len,
                              char *dst, size_t dstn) {
    if (body_len == 0 || dstn == 0) { if (dstn) dst[0] = '\0'; return; }
    uint8_t enc = body[0];
    const uint8_t *txt = body + 1;
    size_t tlen = body_len - 1;
    if (enc == 0x01 || enc == 0x02) {
        /* UTF-16 with/without BOM: crude conversion, keep ASCII subset. */
        size_t o = 0;
        size_t start = 0;
        if (tlen >= 2 && ((txt[0] == 0xFF && txt[1] == 0xFE) ||
                          (txt[0] == 0xFE && txt[1] == 0xFF))) start = 2;
        int be = (tlen >= 2 && txt[0] == 0xFE && txt[1] == 0xFF);
        for (size_t i = start; i + 1 < tlen && o + 1 < dstn; i += 2) {
            uint16_t ch = be ? (uint16_t)((txt[i] << 8) | txt[i+1])
                             : (uint16_t)(txt[i] | (txt[i+1] << 8));
            if (ch == 0) break;
            dst[o++] = (ch < 128) ? (char)ch : '?';
        }
        dst[o] = '\0';
    } else {
        /* 0x00 ISO-8859-1, 0x03 UTF-8: byte copy up to NUL */
        size_t o = 0;
        for (size_t i = 0; i < tlen && o + 1 < dstn; i++) {
            if (txt[i] == '\0') break;
            dst[o++] = (char)txt[i];
        }
        dst[o] = '\0';
    }
    (void)set_field; /* keep helper for future use */
}

size_t metadata_audio_offset(const uint8_t *data, size_t len) {
    if (!data || len < 10) return 0;
    if (memcmp(data, "ID3", 3) != 0) return 0;
    uint32_t tag_size = syncsafe(data + 6);
    size_t off = 10 + tag_size;
    /* skip extended/footer edge cases */
    if (off > len) return len;
    return off;
}

int metadata_parse_buffer(const uint8_t *data, size_t len, TrackMeta *out) {
    if (!data || !out) return 0;
    memset(out, 0, sizeof(*out));
    int found = 0;

    /* ID3v2 */
    if (len >= 10 && memcmp(data, "ID3", 3) == 0) {
        uint8_t ver = data[3];
        uint32_t tag_size = syncsafe(data + 6);
        size_t off = 10;
        size_t end = off + tag_size;
        if (end > len) end = len;
        while (off + 10 <= end) {
            char id[5];
            memcpy(id, data + off, 4); id[4] = '\0';
            if (id[0] == '\0') break;
            uint32_t fsize;
            if (ver == 4) fsize = syncsafe(data + off + 4);
            else fsize = ((uint32_t)data[off+4] << 24) | ((uint32_t)data[off+5] << 16) |
                         ((uint32_t)data[off+6] << 8) | data[off+7];
            size_t body = off + 10;
            if (body + fsize > end) break;
            if (strcmp(id, "TIT2") == 0) { decode_text_frame(data+body, fsize, out->title, sizeof(out->title)); found = 1; }
            else if (strcmp(id, "TPE1") == 0) { decode_text_frame(data+body, fsize, out->artist, sizeof(out->artist)); found = 1; }
            else if (strcmp(id, "TALB") == 0) { decode_text_frame(data+body, fsize, out->album, sizeof(out->album)); found = 1; }
            else if (strcmp(id, "TRCK") == 0) { decode_text_frame(data+body, fsize, out->track_num, sizeof(out->track_num)); found = 1; }
            off = body + fsize;
        }
    }
    /* ID3v1 (fills gaps only) */
    if (len >= 128 && memcmp(data + len - 128, "TAG", 3) == 0) {
        const uint8_t *t = data + len - 128;
        char tmp[32];
        if (!out->title[0]) { memcpy(tmp, t+3, 30); tmp[30]='\0'; for(int i=29;i>=0&&(tmp[i]==' '||tmp[i]=='\0');i--) tmp[i]='\0'; strncpy(out->title, tmp, sizeof(out->title)-1); }
        if (!out->artist[0]) { memcpy(tmp, t+33, 30); tmp[30]='\0'; for(int i=29;i>=0&&(tmp[i]==' '||tmp[i]=='\0');i--) tmp[i]='\0'; strncpy(out->artist, tmp, sizeof(out->artist)-1); }
        if (!out->album[0]) { memcpy(tmp, t+63, 30); tmp[30]='\0'; for(int i=29;i>=0&&(tmp[i]==' '||tmp[i]=='\0');i--) tmp[i]='\0'; strncpy(out->album, tmp, sizeof(out->album)-1); }
        found = 1;
    }
    return found;
}

void metadata_fallback_from_filename(const char *path, TrackMeta *meta) {
    if (!path || !meta) return;
    const char *base = strrchr(path, '/');
    const char *base2 = strrchr(path, '\\');
    if (base2 && (!base || base2 > base)) base = base2;
    base = base ? base + 1 : path;
    if (!meta->title[0]) {
        size_t n = strlen(base);
        /* strip extension */
        const char *dot = strrchr(base, '.');
        size_t copy = dot && dot > base ? (size_t)(dot - base) : n;
        if (copy >= sizeof(meta->title)) copy = sizeof(meta->title) - 1;
        memcpy(meta->title, base, copy);
        meta->title[copy] = '\0';
    }
}

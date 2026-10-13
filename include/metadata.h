#ifndef MIXTAPEDSI_METADATA_H
#define MIXTAPEDSI_METADATA_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define META_FIELD_LEN 128

typedef struct {
    char title[META_FIELD_LEN];
    char artist[META_FIELD_LEN];
    char album[META_FIELD_LEN];
    char track_num[16];
} TrackMeta;

/* Parse ID3v1 (last 128 bytes) and ID3v2.3/2.4 text frames from a memory
 * image of an MP3 file. Always NUL-terminates fields; missing fields
 * stay empty strings. Returns 1 if any tag found, else 0. */
int metadata_parse_buffer(const uint8_t *data, size_t len, TrackMeta *out);

/* Fill empty title from filename (strip dirs + extension). */
void metadata_fallback_from_filename(const char *path, TrackMeta *meta);

/* Skip ID3v2 tag, return offset of first MPEG audio frame (or 0). */
size_t metadata_audio_offset(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif

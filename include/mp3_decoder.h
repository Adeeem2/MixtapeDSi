#ifndef MIXTAPEDSI_MP3_DECODER_H
#define MIXTAPEDSI_MP3_DECODER_H

/* Streaming MP3 decoder abstraction.
 *
 * NDS build: backend = dr_mp3 (public domain, single header, integer DCT
 * path usable on ARM9) fetched into third_party/drmp3/. See docs.
 * Host/tests: stub backend synthesizes silence + parses frame headers
 * for duration estimates so pipeline logic is testable without audio HW.
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MP3_PCM_FRAMES 1152   /* samples per MP3 frame per channel */
#define MP3_MAX_CHANNELS 2

typedef struct {
    /* file reading */
    void *file;                 /* NDS: FILE*; host: FILE* */
    uint8_t *inbuf;
    size_t incap;               /* e.g. 8 KiB */
    size_t inlen;               /* valid bytes in inbuf */
    int eof;
    /* format */
    uint32_t sample_rate;       /* e.g. 44100 */
    int channels;
    uint64_t total_frames;      /* estimate, may be 0 if unknown/CBR guess */
    uint64_t decoded_frames;    /* frames produced so far */
    uint64_t file_size;         /* bytes, 0 if unknown */
    size_t audio_offset;        /* first frame byte (after ID3) */
    size_t inpos;               /* consume index into inbuf */
    int eos;                    /* 1 once decoder reported end-of-stream */
    int skip_frames;            /* reservoir-refill discards after seek */
    int opened;
    int backend_error;
    /* backend-private blob (drmp3 struct lives in .c when enabled) */
    void *backend;
} Mp3Decoder;

/* Open path (reads ID3 offset, inits backend). Returns 0 on success. */
int mp3_open(Mp3Decoder *d, const char *path);
/* Decode up to max_frames interleaved int16 PCM into pcm.
 * Returns frames actually produced (0 = EOS or need-more-input handled
 * internally; <0 = fatal error). Never blocks on UI. */
int mp3_decode_frames(Mp3Decoder *d, int16_t *pcm, int max_frames);
/* Seek to ms (frame-accurate enough for lyric sync; re-syncs bit
 * reservoir by re-opening and skipping). Returns 0 on success. */
int mp3_seek_ms(Mp3Decoder *d, uint32_t ms);
uint32_t mp3_position_ms(const Mp3Decoder *d);
void mp3_close(Mp3Decoder *d);

#ifdef __cplusplus
}
#endif

#endif

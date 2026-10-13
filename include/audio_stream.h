#ifndef MIXTAPEDSI_AUDIO_STREAM_H
#define MIXTAPEDSI_AUDIO_STREAM_H

/* Double-buffered streaming audio pipeline.
 *
 * ARM9 fills PCM buffers from Mp3Decoder; ARM7/DMA (via libnds sound
 * streaming API on NDS) consumes them. Position clock = decoded frames
 * minus buffered-but-unplayed frames, so lyric sync stays exact.
 */

#include <stdint.h>
#include "mp3_decoder.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STREAM_BUFFERS 2
#define STREAM_FRAMES_PER_BUF 2304  /* 2x MP3 frames; ~52ms @44.1kHz */

typedef struct {
    Mp3Decoder dec;
    int16_t *bufs[STREAM_BUFFERS]; /* malloc'd on host, DMA-able on NDS */
    int buf_frames[STREAM_BUFFERS];
    int play_buf;      /* buffer currently consumed by HW */
    int fill_buf;      /* buffer decoder writes to */
    uint32_t sample_rate;
    int channels;
    int playing;
    int underruns;
    /* frames in HW FIFO not yet heard (for clock correction) */
    uint32_t pending_frames;
} AudioStream;

int audio_stream_open(AudioStream *s, const char *path);
void audio_stream_close(AudioStream *s);
/* Decode next chunk into fill buffer; call from main loop / thread.
 * Returns frames decoded (0 at EOS). Bounded: decodes at most one buffer. */
int audio_stream_pump(AudioStream *s);
/* Called from audio callback/IRQ when HW consumed `frames`. */
void audio_stream_consumed(AudioStream *s, uint32_t frames);
uint32_t audio_stream_position_ms(const AudioStream *s);
int audio_stream_at_end(const AudioStream *s);

#ifdef __cplusplus
}
#endif

#endif

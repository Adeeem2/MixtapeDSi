#include "audio_stream.h"
#include <string.h>
#include <stdlib.h>

int audio_stream_open(AudioStream *s, const char *path) {
    if (!s || !path) return -1;
    memset(s, 0, sizeof(*s));
    if (mp3_open(&s->dec, path) != 0) return -1;
    for (int i = 0; i < STREAM_BUFFERS; i++) {
        s->bufs[i] = (int16_t*)malloc(
            (size_t)STREAM_FRAMES_PER_BUF * MP3_MAX_CHANNELS * sizeof(int16_t));
        if (!s->bufs[i]) { audio_stream_close(s); return -1; }
        s->buf_frames[i] = 0;
    }
    s->sample_rate = s->dec.sample_rate;
    s->channels = s->dec.channels;
    s->playing = 1;
    return 0;
}

void audio_stream_close(AudioStream *s) {
    if (!s) return;
    mp3_close(&s->dec);
    for (int i = 0; i < STREAM_BUFFERS; i++) {
        if (s->bufs[i]) free(s->bufs[i]);
        s->bufs[i] = NULL;
    }
    memset(s, 0, sizeof(*s));
}

int audio_stream_pump(AudioStream *s) {
    if (!s || !s->playing) return 0;
    /* fill the non-playing buffer */
    int idx = s->fill_buf;
    int got = mp3_decode_frames(&s->dec, s->bufs[idx], STREAM_FRAMES_PER_BUF);
    if (got < 0) { s->underruns++; return got; }
    if (got == 0) return 0; /* EOS */
    s->buf_frames[idx] = got;
    s->pending_frames += (uint32_t)got;
    /* swap fill buffer */
    s->fill_buf = (s->fill_buf + 1) % STREAM_BUFFERS;
    return got;
}

void audio_stream_consumed(AudioStream *s, uint32_t frames) {
    if (!s) return;
    if (frames > s->pending_frames) s->pending_frames = 0;
    else s->pending_frames -= frames;
    s->play_buf = (s->play_buf + 1) % STREAM_BUFFERS;
    s->buf_frames[s->play_buf] = 0;
}

uint32_t audio_stream_position_ms(const AudioStream *s) {
    if (!s || !s->sample_rate) return 0;
    uint64_t heard = s->dec.decoded_frames;
    if (heard < s->pending_frames) heard = 0;
    else heard -= s->pending_frames;
    return (uint32_t)((heard * 1000ull) / s->sample_rate);
}

int audio_stream_at_end(const AudioStream *s) {
    if (!s) return 1;
    if (!s->dec.opened) return 1;
    if (!s->dec.eos) {
        if (s->dec.total_frames &&
            s->dec.decoded_frames < s->dec.total_frames) return 0;
        if (!s->dec.total_frames) return 0; /* unknown length: not EOS until decoder says so */
    }
    return s->pending_frames == 0;
}

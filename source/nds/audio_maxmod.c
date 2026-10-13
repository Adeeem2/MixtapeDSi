/* ARM9 maxmod streaming backend. See audio_hw.h.
 * Only compiled in NDS builds (guarded by ARM9). */
#ifdef ARM9

#include <nds.h>
#include <maxmod9.h>
#include <string.h>
#include "audio_hw.h"

#define RING_FRAMES 13824         /* stereo frames (~422 ms @32768Hz) */
#define TMP_FRAMES  1152
#define PUMP_MAX_DECODE 8         /* MP3 frames decoded per call, max */

/* Maxmod streams top out at 32768 Hz: higher file rates are resampled. */
#define HW_OUT_RATE_MAX 32768

static int16_t s_ring[RING_FRAMES * 2];
static volatile int s_head = 0;   /* write index (main loop) */
static volatile int s_tail = 0;   /* read index (stream callback) */
static volatile int s_fill = 0;   /* frames in ring */
static volatile uint32_t s_copied = 0;  /* stereo frames handed to HW */
static volatile int s_underruns = 0;
static volatile int s_paused = 0;
static volatile int s_silence_out = 0;  /* consecutive silent callbacks */
static volatile int s_level = 0;        /* 0..8 smoothed output level */
static int s_vol = 100;           /* 0..127 */
static int s_mm_inited = 0;
static int s_stream_open = 0;
static uint32_t s_rate = 32768;   /* hardware stream rate */
static uint32_t s_in_rate = 44100;/* decoder (file) rate */
static int s_need_resample = 0;
static Mp3Decoder *s_dec = NULL;
static int16_t s_tmp[TMP_FRAMES * 2];
/* linear-resampler state (16.16 fixed point) */
static uint32_t s_rs_step = 0;    /* input frames per output frame */
static uint32_t s_rs_phase = 0;   /* fractional position carried */
static int16_t s_rs_prev_l = 0, s_rs_prev_r = 0;

static int ring_free(void) { return RING_FRAMES - s_fill; }

static mm_word stream_cb(mm_word length, mm_addr dest, mm_stream_formats format) {
    (void)format;
    int16_t *out = (int16_t *)dest;
    mm_word want = length;
    mm_word got = 0;
    if (!s_paused) {
        while (got < want && s_fill > 0) {
            int16_t l = s_ring[s_tail * 2];
            int16_t r = s_ring[s_tail * 2 + 1];
            if (s_vol != 127) {
                l = (int16_t)((l * s_vol) >> 7);
                r = (int16_t)((r * s_vol) >> 7);
            }
            out[got * 2] = l;
            out[got * 2 + 1] = r;
            s_tail = (s_tail + 1) % RING_FRAMES;
            s_fill--;
            got++;
        }
    }
    if (got < want) {
        memset(out + got * 2, 0, (size_t)(want - got) * 2 * sizeof(int16_t));
        if (!s_paused && s_stream_open) {
            s_underruns++;
            s_silence_out++;
        }
    } else {
        s_silence_out = 0;
    }
    s_copied += got;
    return want;
}

int audio_hw_open(Mp3Decoder *dec) {
    if (!dec || !dec->opened) return -1;
    if (!s_mm_inited) {
        mm_ds_system sys;
        sys.mod_count = 0;
        sys.samp_count = 0;
        sys.mem_bank = 0;
        mmInit(&sys);
        s_mm_inited = 1;
    }
    if (s_stream_open) audio_hw_close();
    s_dec = dec;
    s_in_rate = dec->sample_rate ? dec->sample_rate : 44100;
    /* maxmod streams support 1024..32768 Hz: resample anything above. */
    if (s_in_rate > HW_OUT_RATE_MAX) {
        s_rate = HW_OUT_RATE_MAX;
        s_need_resample = 1;
        s_rs_step = (uint32_t)(((uint64_t)s_in_rate << 16) / s_rate);
    } else {
        s_rate = s_in_rate;
        s_need_resample = 0;
        s_rs_step = 0;
    }
    s_rs_phase = 0;
    s_rs_prev_l = s_rs_prev_r = 0;
    s_head = s_tail = s_fill = 0;
    s_copied = 0;
    s_underruns = 0;
    s_silence_out = 0;
    s_paused = 0;

    static mm_stream s_stream;
    s_stream.sampling_rate = s_rate;
    s_stream.buffer_length = 4096;
    s_stream.callback = stream_cb;
    s_stream.format = MM_STREAM_16BIT_STEREO;
    s_stream.timer = MM_TIMER0;
    s_stream.manual = true;
    mmStreamOpen(&s_stream);
    s_stream_open = 1;
    return 0;
}

void audio_hw_pump(void) {
    if (!s_stream_open || !s_dec || s_paused) return;
    for (int n = 0; n < PUMP_MAX_DECODE; n++) {
        if (ring_free() < TMP_FRAMES + 2) break;
        int got = mp3_decode_frames(s_dec, s_tmp, TMP_FRAMES);
        if (got <= 0) break; /* EOS (dec->eos set) or error */
        /* level meter: sparse average of |samples|, smoothed */
        {
            int ch = s_dec->channels ? s_dec->channels : 2;
            long acc = 0;
            int cnt = 0;
            for (int i = 0; i < got * ch; i += 16) {
                int v = s_tmp[i];
                acc += (v < 0) ? -v : v;
                cnt++;
            }
            if (cnt > 0) {
                int inst = (int)(acc / cnt / 900);
                if (inst > 8) inst = 8;
                s_level = (s_level * 3 + inst + 2) / 4;
            }
        }
        int ch = s_dec->channels ? s_dec->channels : 2;
        if (!s_need_resample) {
            for (int i = 0; i < got; i++) {
                int16_t l, r;
                if (ch == 1) l = r = s_tmp[i];
                else { l = s_tmp[i * 2]; r = s_tmp[i * 2 + 1]; }
                s_ring[s_head * 2] = l;
                s_ring[s_head * 2 + 1] = r;
                s_head = (s_head + 1) % RING_FRAMES;
                s_fill++;
            }
        } else {
            /* linear downsample to HW_OUT_RATE_MAX (exact phase carry). */
            uint32_t pos = s_rs_phase;
            uint32_t end = (uint32_t)got << 16;
            int16_t pl = s_rs_prev_l, pr = s_rs_prev_r;
            while (pos < end && s_fill < RING_FRAMES) {
                int idx = (int)(pos >> 16) - 1; /* -1 => carried prev */
                int16_t al = (idx < 0) ? pl : (ch == 1 ? s_tmp[idx] : s_tmp[idx * 2]);
                int16_t ar = (idx < 0) ? pr : (ch == 1 ? s_tmp[idx] : s_tmp[idx * 2 + 1]);
                int16_t bl = (ch == 1) ? s_tmp[idx + 1] : s_tmp[(idx + 1) * 2];
                int16_t br = (ch == 1) ? s_tmp[idx + 1] : s_tmp[(idx + 1) * 2 + 1];
                uint32_t frac = pos & 0xFFFFu;
                int16_t l = (int16_t)(al + (((int32_t)(bl - al) * (int32_t)frac) >> 16));
                int16_t r = (int16_t)(ar + (((int32_t)(br - ar) * (int32_t)frac) >> 16));
                s_ring[s_head * 2] = l;
                s_ring[s_head * 2 + 1] = r;
                s_head = (s_head + 1) % RING_FRAMES;
                s_fill++;
                pos += s_rs_step;
            }
            s_rs_phase = pos - end;
            if (ch == 1) s_rs_prev_l = s_rs_prev_r = s_tmp[got - 1];
            else { s_rs_prev_l = s_tmp[(got - 1) * 2]; s_rs_prev_r = s_tmp[(got - 1) * 2 + 1]; }
        }
    }
}

void audio_hw_update(void) {
    /* Always update while open: stopping updates makes the hardware loop
     * the last buffer (stutter). Pause is handled in the callback, which
     * emits silence without consuming the ring. */
    if (s_stream_open) mmStreamUpdate();
    if (s_paused && s_level > 0) s_level--;
}

void audio_hw_close(void) {
    if (s_stream_open) { mmStreamClose(); s_stream_open = 0; }
    s_dec = NULL;
}

uint32_t audio_hw_position_ms(void) {
    if (!s_rate) return 0;
    return (uint32_t)(((uint64_t)s_copied * 1000u) / s_rate);
}

uint32_t audio_hw_rate(void) { return s_rate; }
int audio_hw_underruns(void) { return s_underruns; }
int audio_hw_level(void) { return (s_level < 0) ? 0 : (s_level > 8 ? 8 : s_level); }

int audio_hw_drained(void) {
    if (!s_dec) return 1;
    return s_dec->eos && s_fill == 0 && s_silence_out > 4;
}

void audio_hw_set_paused(int paused) { s_paused = paused ? 1 : 0; }

int audio_hw_seek_ms(uint32_t ms) {
    if (!s_dec) return -1;
    if (mp3_seek_ms(s_dec, ms) != 0) return -1;
    s_head = s_tail = s_fill = 0;
    s_copied = 0;
    s_silence_out = 0;
    s_rs_phase = 0;
    s_rs_prev_l = s_rs_prev_r = 0;
    return 0;
}

void audio_hw_set_volume(int vol127) {
    if (vol127 < 0) vol127 = 0;
    if (vol127 > 127) vol127 = 127;
    s_vol = vol127;
}

#endif /* ARM9 */

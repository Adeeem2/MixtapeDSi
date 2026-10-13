/* Standalone check of the audio_maxmod linear downsampler math
 * (mirrors the loop in source/nds/audio_maxmod.c). */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#define TMP 1152
static int16_t s_tmp[TMP * 2];
static uint32_t s_rs_step, s_rs_phase;
static int16_t s_rs_prev_l, s_rs_prev_r;

static int run_chunk(int got, int ch, int16_t *out, int cap) {
    uint32_t pos = s_rs_phase;
    uint32_t end = (uint32_t)got << 16;
    int16_t pl = s_rs_prev_l, pr = s_rs_prev_r;
    int n = 0;
    while (pos < end && n < cap) {
        int idx = (int)(pos >> 16) - 1;
        int16_t al = (idx < 0) ? pl : (ch == 1 ? s_tmp[idx] : s_tmp[idx * 2]);
        int16_t ar = (idx < 0) ? pr : (ch == 1 ? s_tmp[idx] : s_tmp[idx * 2 + 1]);
        int16_t bl = (ch == 1) ? s_tmp[idx + 1] : s_tmp[(idx + 1) * 2];
        int16_t br = (ch == 1) ? s_tmp[idx + 1] : s_tmp[(idx + 1) * 2 + 1];
        uint32_t frac = pos & 0xFFFFu;
        out[n * 2] = (int16_t)(al + (((int32_t)(bl - al) * (int32_t)frac) >> 16));
        out[n * 2 + 1] = (int16_t)(ar + (((int32_t)(br - ar) * (int32_t)frac) >> 16));
        n++;
        pos += s_rs_step;
    }
    s_rs_phase = pos - end;
    if (ch == 1) s_rs_prev_l = s_rs_prev_r = s_tmp[got - 1];
    else { s_rs_prev_l = s_tmp[(got - 1) * 2]; s_rs_prev_r = s_tmp[(got - 1) * 2 + 1]; }
    return n;
}

int main(void) {
    int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %d: %s\n", __LINE__, #c); fails++; } } while (0)
    /* 44100 Hz stereo sine -> 32768 Hz over 10 chunks */
    s_rs_step = (uint32_t)(((uint64_t)44100 << 16) / 32768);
    s_rs_phase = 0; s_rs_prev_l = s_rs_prev_r = 0;
    int total_in = 0, total_out = 0;
    int16_t out[TMP * 2 + 4];
    int16_t prev_last_l = 0;
    int first = 1;
    for (int c = 0; c < 10; c++) {
        for (int i = 0; i < TMP; i++) {
            double t = (double)(total_in + i) / 44100.0;
            int16_t v = (int16_t)(30000 * sin(2 * 3.14159265 * 440.0 * t));
            s_tmp[i * 2] = v; s_tmp[i * 2 + 1] = (int16_t)-v;
        }
        int n = run_chunk(TMP, 2, out, TMP + 2);
        CHECK(n > 850 && n < 860); /* 1152*32768/44100 = 856.0 */
        if (!first) {
            /* continuity across chunk boundary */
            CHECK(abs(out[0] - prev_last_l) < 3000);
        }
        first = 0;
        prev_last_l = out[(n - 1) * 2]; total_in += TMP; total_out += n;
    }
    double expect = total_in * 32768.0 / 44100.0;
    printf("in=%d out=%d expect=%.1f\n", total_in, total_out, expect);
    CHECK(abs(total_out - (int)expect) <= 2);
    /* no-resample path ratio sanity */
    CHECK(s_rs_step == 88200); /* 44100*65536/32768 = 88200 */
    if (!fails) printf("resample-math: ALL OK\n");
    return fails ? 1 : 0;
}

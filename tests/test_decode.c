/* Real-decoder test (USE_HELIX): decodes tests/data/tone.mp3 through
 * the actual fixed-point backend. Run from the repo root. */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "mp3_decoder.h"

static int checks = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); return 1; } checks++; } while (0)

int main(void) {
    Mp3Decoder d;
    CHECK(mp3_open(&d, "tests/data/tone.mp3") == 0);
    CHECK(d.opened && d.sample_rate == 44100 && d.channels == 2);

    static int16_t pcm[1152 * 2];
    int total = 0;
    for (int i = 0; i < 10; i++) {
        int got = mp3_decode_frames(&d, pcm, 1152);
        CHECK(got >= 0);
        if (got == 0) break;
        CHECK(got == 1152);
        total += got;
    }
    CHECK(total == 10 * 1152);
    uint32_t pos = mp3_position_ms(&d);
    CHECK(pos > 200 && pos < 400); /* ~261 ms */

    /* decoded content is a real tone, not silence */
    {
        long long energy = 0;
        int got = mp3_decode_frames(&d, pcm, 1152);
        CHECK(got == 1152);
        for (int i = 0; i < got * 2; i++) energy += llabs((long long)pcm[i]);
        CHECK(energy > 1000000LL);
    }

    /* seek + continue */
    CHECK(mp3_seek_ms(&d, 1000) == 0);
    CHECK(mp3_position_ms(&d) == 1000);
    {
        int got = mp3_decode_frames(&d, pcm, 1152);
        CHECK(got == 1152);
        CHECK(mp3_position_ms(&d) > 1000);
    }

    /* drain to EOS */
    {
        int guard = 0, got;
        do { got = mp3_decode_frames(&d, pcm, 1152); guard++; }
        while (got > 0 && guard < 500);
        CHECK(d.eos == 1);
    }

    /* missing file */
    {
        Mp3Decoder bad;
        CHECK(mp3_open(&bad, "tests/data/nope.mp3") != 0);
    }

    mp3_close(&d);
    printf("test_decode: ALL %d checks passed\n", checks);
    return 0;
}

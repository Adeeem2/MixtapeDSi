/* Decode a whole file, report anomalies. Usage: diag_mp3 <path> */
#include <stdio.h>
#include <stdint.h>
#include "mp3_decoder.h"

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    Mp3Decoder d;
    if (mp3_open(&d, argv[1]) != 0) { printf("open failed\n"); return 1; }
    printf("open: rate=%u ch=%d total_est=%llu\n",
           d.sample_rate, d.channels, (unsigned long long)d.total_frames);
    static int16_t pcm[1152 * 2];
    long frames = 0, calls = 0, zero_runs = 0, neg = 0;
    int last_ch = d.channels;
    uint32_t last_rate = d.sample_rate;
    for (;;) {
        int got = mp3_decode_frames(&d, pcm, 1152);
        calls++;
        if (got < 0) { neg++; break; }
        if (got == 0) { zero_runs++; if (d.eos || zero_runs > 5) break; continue; }
        if (got != 1152) printf("note: partial frame %d at call %ld\n", got, calls);
        if (d.channels != last_ch) { printf("channels %d -> %d at frame %ld\n", last_ch, d.channels, frames); last_ch = d.channels; }
        if (d.sample_rate != last_rate) { printf("rate %u -> %u at frame %ld\n", last_rate, d.sample_rate, frames); last_rate = d.sample_rate; }
        frames += got;
        if (calls > 20000) break;
    }
    printf("frames=%ld calls=%ld pos_ms=%u eos=%d err=%d\n",
           frames, calls, mp3_position_ms(&d), d.eos, d.backend_error);
    mp3_close(&d);
    return 0;
}

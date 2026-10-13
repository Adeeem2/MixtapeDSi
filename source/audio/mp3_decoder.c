/* Portable MP3 decoder front-end.
 *
 * NDS (USE_HELIX): Helix fixed-point decoder (third_party/helix/,
 * RealNetworks RPSL - see docs/ASSET_LICENSES.md). Integer-only ARM
 * path: ~10x cheaper than float decoders on the ARM9 (no FPU).
 * Frame-oriented API: sync search + refill loop around MP3Decode().
 *
 * Host/tests (default): lightweight stub that synthesizes silence so
 * pipeline/clock/seek logic is testable. tests/test_decode.c exercises
 * the real USE_HELIX backend against a real MP3.
 */
#include "mp3_decoder.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef USE_HELIX
#include "mp3dec.h"
#define HELIX_INBUF 8192
/* Refill before less than a full frame remains: decoding from a truncated
 * frame fails and error-skip cannot bridge the gap (death spiral). Max
 * MPEG1 frame ~1441 bytes; 4096 covers free-format outliers too. */
#define HELIX_MIN_AVAIL 4096
#define HELIX_MAX_TRIES 64
#endif

#ifndef USE_HELIX
static const uint32_t SR_TABLE[4] = {44100, 48000, 32000, 0};
#endif

/* Sniff first MPEG frame header for rate/channels (open-time estimate;
 * the decoder confirms exact values on the first decoded frame). */
static void sniff_header(FILE *f, uint32_t *rate, int *ch) {
    static const uint32_t srtab[4] = {44100, 48000, 32000, 0};
    uint8_t hdr[4096];
    size_t n = fread(hdr, 1, sizeof(hdr), f);
    *rate = 44100; *ch = 2;
    for (size_t i = 0; i + 4 <= n; i++) {
        if (hdr[i] == 0xFF && (hdr[i + 1] & 0xE0) == 0xE0 &&
            ((hdr[i + 1] >> 3) & 0x03) != 0x01) { /* valid layer bits */
            int sr = (hdr[i + 2] >> 2) & 0x03;
            if (srtab[sr]) *rate = srtab[sr];
            *ch = (((hdr[i + 3] >> 6) & 0x03) == 3) ? 1 : 2;
            break;
        }
    }
    fseek(f, 0, SEEK_SET);
}

int mp3_open(Mp3Decoder *d, const char *path) {
    if (!d || !path) return -1;
    memset(d, 0, sizeof(*d));
    d->file = fopen(path, "rb");
    if (!d->file) return -1;
    d->incap = 8192;
    d->inbuf = (uint8_t *)malloc(d->incap);
    if (!d->inbuf) { fclose((FILE *)d->file); d->file = NULL; return -1; }

    sniff_header((FILE *)d->file, &d->sample_rate, &d->channels);

    /* skip ID3v2 for decode start + CBR duration guess */
    fseek((FILE *)d->file, 0, SEEK_END);
    long fsz = ftell((FILE *)d->file);
    fseek((FILE *)d->file, 0, SEEK_SET);
    size_t audio_off = 0;
    {
        uint8_t h[10];
        if (fread(h, 1, 10, (FILE *)d->file) == 10 && memcmp(h, "ID3", 3) == 0) {
            audio_off = 10 + (((uint32_t)(h[6] & 0x7F) << 21) |
                              ((uint32_t)(h[7] & 0x7F) << 14) |
                              ((uint32_t)(h[8] & 0x7F) << 7) |
                              (uint32_t)(h[9] & 0x7F));
        }
        fseek((FILE *)d->file, (long)audio_off, SEEK_SET);
    }
    d->file_size = (fsz > 0) ? (uint64_t)fsz : 0;
    d->audio_offset = audio_off;
    if (fsz > (long)audio_off)
        d->total_frames = (uint64_t)(fsz - audio_off) * d->sample_rate / 16000u;

#ifdef USE_HELIX
    {
        HMP3Decoder h = MP3InitDecoder();
        if (!h) { mp3_close(d); return -1; }
        d->backend = h;
    }
#endif
    d->opened = 1;
    return 0;
}

#ifdef USE_HELIX
/* Refill input buffer: compact leftover, read more. Returns new avail. */
static int helix_fill(Mp3Decoder *d) {
    FILE *f = (FILE *)d->file;
    size_t unconsumed = (d->inlen > d->inpos) ? d->inlen - d->inpos : 0;
    if (unconsumed && d->inpos)
        memmove(d->inbuf, d->inbuf + d->inpos, unconsumed);
    d->inpos = 0;
    d->inlen = unconsumed;
    if (d->inlen < d->incap) {
        size_t n = fread(d->inbuf + d->inlen, 1, d->incap - d->inlen, f);
        d->inlen += n;
        if (n == 0) d->eof = 1;
    }
    return (int)(d->inlen - d->inpos);
}

static int helix_decode_once(Mp3Decoder *d, int16_t *pcm, int max_frames,
                             int discard) {
    HMP3Decoder h = (HMP3Decoder)d->backend;
    int tries = 0;
    while (tries++ < HELIX_MAX_TRIES) {
        int avail = (int)(d->inlen - d->inpos);
        if (avail < HELIX_MIN_AVAIL) {
            avail = helix_fill(d);
            if (avail <= 0) { d->eos = 1; return 0; }
        }
        int sync = MP3FindSyncWord(d->inbuf + d->inpos, avail);
        if (sync < 0) { /* no sync: drop buffer, refill */
            d->inpos = d->inlen;
            continue;
        }
        d->inpos += (size_t)sync;
        avail = (int)(d->inlen - d->inpos);
        uint8_t *p = d->inbuf + d->inpos;
        int left = avail;
        int16_t tmp[1152 * 2];
        int err = MP3Decode(h, &p, &left, tmp, 0);
        d->inpos += (size_t)(avail - left);
        if (err == 0) {
            MP3FrameInfo fi;
            MP3GetLastFrameInfo(h, &fi);
            if (fi.samprate > 0) d->sample_rate = (uint32_t)fi.samprate;
            if (fi.nChans > 0) d->channels = fi.nChans;
            /* refine duration from first frame bitrate */
            if (d->total_frames && fi.bitrate > 0 && d->file_size > d->audio_offset)
                d->total_frames = (d->file_size - d->audio_offset) * 8u *
                                  d->sample_rate / (uint32_t)fi.bitrate;
            int frames = fi.outputSamps / (fi.nChans ? fi.nChans : 2);
            if (frames <= 0 || frames > 1152) continue;
            if (discard) return 0; /* reservoir refill after seek */
            if (frames > max_frames) {
                /* caller room too small: rewind (rare; callers use 1152) */
                d->inpos -= (size_t)(avail - left);
                return 0;
            }
            if (d->channels == 1) {
                /* mono: compact layout, pump reads pcm[i] */
                for (int i = 0; i < frames; i++) pcm[i] = tmp[i];
            } else {
                for (int i = 0; i < frames * 2; i++) pcm[i] = tmp[i];
            }
            d->decoded_frames += (uint64_t)frames;
            return frames;
        } else if (err == ERR_MP3_INDATA_UNDERFLOW ||
                   err == ERR_MP3_MAINDATA_UNDERFLOW) {
            helix_fill(d);
            if (d->eof && (d->inlen - d->inpos) <= 0) { d->eos = 1; return 0; }
            continue;
        } else {
            d->inpos++; /* corrupt frame: skip a byte, resync */
            continue;
        }
    }
    d->backend_error = 1;
    return -1;
}
#endif

int mp3_decode_frames(Mp3Decoder *d, int16_t *pcm, int max_frames) {
    if (!d || !d->opened || !pcm || max_frames <= 0) return -1;
#ifdef USE_HELIX
    int produced = 0;
    int skips = d->skip_frames;
    while (produced < max_frames) {
        int got = helix_decode_once(d, pcm + produced * 2,
                                    max_frames - produced, skips > 0);
        if (got < 0) return (produced > 0) ? produced : -1;
        if (got == 0) {
            if (skips > 0) { skips--; d->skip_frames = skips; continue; }
            break; /* EOS */
        }
        produced += got;
        if (d->eos) break;
        /* one MP3 frame per call is the common case; loop for remainder */
        if (produced >= 1152) break;
    }
    return produced;
#else
    /* Host stub: synthesize silence; counts frames for clock tests. */
    int want = max_frames > MP3_PCM_FRAMES ? MP3_PCM_FRAMES : max_frames;
    if (d->total_frames && d->decoded_frames >= d->total_frames) { d->eos = 1; return 0; }
    memset(pcm, 0, (size_t)want * MP3_MAX_CHANNELS * sizeof(int16_t));
    d->decoded_frames += (uint64_t)want;
    return want;
#endif
}

int mp3_seek_ms(Mp3Decoder *d, uint32_t ms) {
    if (!d || !d->opened) return -1;
    uint64_t target = (uint64_t)ms * d->sample_rate / 1000u;
    uint64_t span = d->total_frames ? d->total_frames : target + 1;
    if (span == 0) span = 1;
    uint64_t off = d->audio_offset;
    if (d->file_size > d->audio_offset)
        off = d->audio_offset + (d->file_size - d->audio_offset) * target / span;
    fseek((FILE *)d->file, (long)off, SEEK_SET);
    d->inlen = d->inpos = 0;
    d->eof = 0;
    d->decoded_frames = target;
    d->eos = 0;
    d->backend_error = 0;
#ifdef USE_HELIX
    /* fresh reservoir after a jump: discard ~2 frames while it refills */
    MP3FreeDecoder((HMP3Decoder)d->backend);
    d->backend = MP3InitDecoder();
    if (!d->backend) return -1;
    d->skip_frames = 2;
#else
    if (d->total_frames && d->decoded_frames > d->total_frames)
        d->decoded_frames = d->total_frames;
#endif
    return 0;
}

uint32_t mp3_position_ms(const Mp3Decoder *d) {
    if (!d || !d->sample_rate) return 0;
    return (uint32_t)((d->decoded_frames * 1000ull) / d->sample_rate);
}

void mp3_close(Mp3Decoder *d) {
    if (!d) return;
#ifdef USE_HELIX
    if (d->backend) { MP3FreeDecoder((HMP3Decoder)d->backend); d->backend = NULL; }
#endif
    if (d->file) fclose((FILE *)d->file);
    if (d->inbuf) free(d->inbuf);
    memset(d, 0, sizeof(*d));
}

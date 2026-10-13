#include "playback_controller.h"
#include <string.h>

static uint32_t lcg_next(uint32_t *s) {
    *s = *s * 1664525u + 1013904223u;
    return *s;
}

static void rebuild_order(PlaybackController *pb) {
    int n = pb->library ? pb->library->count : 0;
    if (n > LIB_MAX_TRACKS) n = LIB_MAX_TRACKS;
    pb->order_len = n;
    for (int i = 0; i < n; i++) pb->order[i] = i;
    if (pb->shuffle && n > 1) {
        /* Fisher-Yates with internal LCG (deterministic, testable). */
        for (int i = n - 1; i > 0; i--) {
            uint32_t r = lcg_next(&pb->rng) % (uint32_t)(i + 1);
            int tmp = pb->order[i]; pb->order[i] = pb->order[r]; pb->order[r] = tmp;
        }
    }
    if (pb->order_pos >= n) pb->order_pos = n ? n - 1 : 0;
}

void pb_init(PlaybackController *pb, const MusicLibrary *lib) {
    memset(pb, 0, sizeof(*pb));
    pb->library = lib;
    pb->sample_rate = 44100;
    pb->rng = 0x12345678u;
    pb->rng_seed_default = 0x12345678u;
    rebuild_order(pb);
}

void pb_set_shuffle(PlaybackController *pb, int on) {
    int cur = pb_current_index(pb);
    pb->shuffle = on ? 1 : 0;
    if (!on) pb->rng = pb->rng_seed_default;
    rebuild_order(pb);
    /* keep current track selected */
    if (cur >= 0) {
        for (int i = 0; i < pb->order_len; i++)
            if (pb->order[i] == cur) { pb->order_pos = i; break; }
    }
}

void pb_set_repeat(PlaybackController *pb, RepeatMode mode) { pb->repeat = mode; }
void pb_cycle_repeat(PlaybackController *pb) {
    pb->repeat = (RepeatMode)((pb->repeat + 1) % 3);
}

int pb_play_index(PlaybackController *pb, int lib_index) {
    if (!pb->library || lib_index < 0 || lib_index >= pb->order_len) return -1;
    for (int i = 0; i < pb->order_len; i++)
        if (pb->order[i] == lib_index) { pb->order_pos = i; break; }
    pb->samples_played = 0;
    pb->state = PB_PLAYING;
    return 0;
}

int pb_play_order_pos(PlaybackController *pb, int opos) {
    if (opos < 0 || opos >= pb->order_len) return -1;
    pb->order_pos = opos;
    pb->samples_played = 0;
    pb->state = PB_PLAYING;
    return 0;
}

int pb_current_index(const PlaybackController *pb) {
    if (!pb->library || pb->order_len == 0) return -1;
    if (pb->order_pos < 0 || pb->order_pos >= pb->order_len) return -1;
    return pb->order[pb->order_pos];
}

void pb_pause(PlaybackController *pb) { if (pb->state == PB_PLAYING) pb->state = PB_PAUSED; }
void pb_resume(PlaybackController *pb) { if (pb->state == PB_PAUSED) pb->state = PB_PLAYING; }
void pb_toggle(PlaybackController *pb) {
    if (pb->state == PB_PLAYING) pb->state = PB_PAUSED;
    else if (pb->state == PB_PAUSED) pb->state = PB_PLAYING;
}
void pb_stop(PlaybackController *pb) { pb->state = PB_STOPPED; pb->samples_played = 0; }

int pb_next(PlaybackController *pb) {
    if (pb->order_len == 0) return 0;
    if (pb->repeat == REPEAT_ONE) { pb->samples_played = 0; pb->state = PB_PLAYING; return 1; }
    if (pb->order_pos + 1 < pb->order_len) {
        pb->order_pos++;
        pb->samples_played = 0;
        pb->state = PB_PLAYING;
        return 1;
    }
    if (pb->repeat == REPEAT_ALL) {
        pb->order_pos = 0;
        pb->samples_played = 0;
        pb->state = PB_PLAYING;
        return 1;
    }
    pb->state = PB_STOPPED;
    return 0;
}

int pb_prev(PlaybackController *pb) {
    if (pb->order_len == 0) return 0;
    /* restart track if >3s in (common player UX) */
    if (pb_position_ms(pb) > 3000) { pb->samples_played = 0; pb->state = PB_PLAYING; return 1; }
    if (pb->order_pos > 0) {
        pb->order_pos--;
        pb->samples_played = 0;
        pb->state = PB_PLAYING;
        return 1;
    }
    pb->samples_played = 0;
    return 1;
}

int pb_on_track_end(PlaybackController *pb) {
    if (pb->repeat == REPEAT_ONE) {
        pb->samples_played = 0;
        pb->state = PB_PLAYING;
        return 1;
    }
    return pb_next(pb);
}

void pb_advance_samples(PlaybackController *pb, uint32_t frames) {
    pb->samples_played += frames;
}

uint32_t pb_position_ms(const PlaybackController *pb) {
    if (!pb->sample_rate) return 0;
    return (uint32_t)((pb->samples_played * 1000ull) / pb->sample_rate);
}

void pb_seek_ms(PlaybackController *pb, uint32_t ms) {
    pb->samples_played = (uint64_t)ms * pb->sample_rate / 1000u;
}

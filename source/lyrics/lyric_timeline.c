#include "lyric_timeline.h"

int lyric_active_index(const LrcSong *song, uint32_t pos_ms) {
    if (!song || song->count <= 0) return -1;
    int lo = 0, hi = song->count - 1, ans = -1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        uint32_t t = song->lines[mid].timestamp_ms;
        if (t <= pos_ms) { ans = mid; lo = mid + 1; }
        else hi = mid - 1;
    }
    return ans;
}

void lyric_window(const LrcSong *song, uint32_t pos_ms,
                  int *prev_idx, int *cur_idx, int *next_idx) {
    int cur = lyric_active_index(song, pos_ms);
    int nxt = -1, prv = -1;
    if (song && song->count > 0) {
        if (cur < 0) nxt = 0;
        else {
            if (cur - 1 >= 0) prv = cur - 1;
            if (cur + 1 < song->count) nxt = cur + 1;
        }
    }
    if (prev_idx) *prev_idx = prv;
    if (cur_idx) *cur_idx = cur;
    if (next_idx) *next_idx = nxt;
}

int lyric_index_changed(const LrcSong *song, uint32_t old_pos,
                        uint32_t new_pos) {
    return lyric_active_index(song, old_pos) != lyric_active_index(song, new_pos);
}

#define LYR_MIN_LINE_MS 800u
#define LYR_MAX_LINE_MS 12000u
#define LYR_DEFAULT_LINE_MS 4000u

uint32_t lyric_line_duration_ms(const LrcSong *song, int idx,
                                uint32_t song_len_ms) {
    uint32_t start, end, dur;
    if (!song || idx < 0 || idx >= song->count) return 0;
    start = song->lines[idx].timestamp_ms;
    if (idx + 1 < song->count)
        end = song->lines[idx + 1].timestamp_ms;
    else if (song_len_ms > start)
        end = song_len_ms;
    else
        end = 0;
    dur = (end > start) ? end - start : 0;
    if (dur == 0) dur = LYR_DEFAULT_LINE_MS;
    if (dur < LYR_MIN_LINE_MS) dur = LYR_MIN_LINE_MS;
    if (dur > LYR_MAX_LINE_MS) dur = LYR_MAX_LINE_MS;
    return dur;
}

int lyric_sweep_chars(const LrcSong *song, int idx, uint32_t pos_ms,
                      int total_chars, uint32_t song_len_ms) {
    uint32_t start, dur, elapsed;
    if (!song || idx < 0 || idx >= song->count || total_chars <= 0) return 0;
    start = song->lines[idx].timestamp_ms;
    if (pos_ms <= start) return 0;
    dur = lyric_line_duration_ms(song, idx, song_len_ms);
    elapsed = pos_ms - start;
    if (elapsed >= dur) return total_chars;
    return (int)(((uint64_t)elapsed * (uint64_t)total_chars) / dur);
}

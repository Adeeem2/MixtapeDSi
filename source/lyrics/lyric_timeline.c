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

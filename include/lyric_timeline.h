#ifndef MIXTAPEDSI_LYRIC_TIMELINE_H
#define MIXTAPEDSI_LYRIC_TIMELINE_H

#include <stdint.h>
#include "lrc_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Return index of the latest line with timestamp <= pos_ms, or -1 if
 * no line is active yet (position before first lyric) or song empty. */
int lyric_active_index(const LrcSong *song, uint32_t pos_ms);

/* Fill prev/cur/next indices around pos_ms (-1 when absent). */
void lyric_window(const LrcSong *song, uint32_t pos_ms,
                  int *prev_idx, int *cur_idx, int *next_idx);

/* True when the active index changed between old_pos and new_pos. */
int lyric_index_changed(const LrcSong *song, uint32_t old_pos,
                        uint32_t new_pos);

/* Duration used to sweep line idx: distance to the next timestamp,
 * clamped to [800, 12000] ms. The last line falls back to song_len_ms
 * (when it is later than the line), then to 4000 ms. */
uint32_t lyric_line_duration_ms(const LrcSong *song, int idx,
                                uint32_t song_len_ms);

/* Karaoke progress for line idx: how many of total_chars display columns
 * should be highlighted at pos_ms (0..total_chars). */
int lyric_sweep_chars(const LrcSong *song, int idx, uint32_t pos_ms,
                      int total_chars, uint32_t song_len_ms);

#ifdef __cplusplus
}
#endif

#endif

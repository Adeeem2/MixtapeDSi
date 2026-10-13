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

#ifdef __cplusplus
}
#endif

#endif

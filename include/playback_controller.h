#ifndef MIXTAPEDSI_PLAYBACK_H
#define MIXTAPEDSI_PLAYBACK_H

#include <stdint.h>
#include "music_library.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PB_STOPPED = 0,
    PB_PLAYING,
    PB_PAUSED
} PlaybackState;

typedef enum {
    REPEAT_OFF = 0,
    REPEAT_ALL,
    REPEAT_ONE
} RepeatMode;

typedef struct {
    const MusicLibrary *library;
    int order[LIB_MAX_TRACKS]; /* playback order (shuffled or linear) */
    int order_len;
    int order_pos;             /* position inside order[] */
    PlaybackState state;
    RepeatMode repeat;
    int shuffle;               /* bool */
    uint32_t rng;              /* deterministic LCG for testable shuffle */
    uint64_t samples_played;   /* decoded samples consumed (mono equiv) */
    uint32_t sample_rate;      /* e.g. 44100 */
    uint32_t rng_seed_default;
} PlaybackController;

void pb_init(PlaybackController *pb, const MusicLibrary *lib);
void pb_set_shuffle(PlaybackController *pb, int on);
void pb_set_repeat(PlaybackController *pb, RepeatMode mode);
void pb_cycle_repeat(PlaybackController *pb);

/* Begin playing library track index (resolves through shuffle order).
 * Resets samples_played clock. */
int pb_play_index(PlaybackController *pb, int lib_index);
int pb_play_order_pos(PlaybackController *pb, int opos);
int pb_current_index(const PlaybackController *pb);
void pb_pause(PlaybackController *pb);
void pb_resume(PlaybackController *pb);
void pb_toggle(PlaybackController *pb);
void pb_stop(PlaybackController *pb);

/* Track advancement. Returns 1 if a new track started, 0 if stopped. */
int pb_next(PlaybackController *pb);
int pb_prev(PlaybackController *pb);
/* Called by audio engine at natural end-of-stream. Honors repeat one/all. */
int pb_on_track_end(PlaybackController *pb);

/* Clock: advance by n PCM frames consumed; query ms for lyric sync. */
void pb_advance_samples(PlaybackController *pb, uint32_t frames);
uint32_t pb_position_ms(const PlaybackController *pb);
void pb_seek_ms(PlaybackController *pb, uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif

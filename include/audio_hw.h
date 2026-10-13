#ifndef MIXTAPEDSI_AUDIO_HW_H
#define MIXTAPEDSI_AUDIO_HW_H

/* NDS (ARM9) audio output: maxmod PCM stream fed by Mp3Decoder.
 * Main loop pumps decoded frames into a lock-free ring; the maxmod
 * timer callback copies them to hardware. Clock = samples handed to
 * hardware, so lyric sync is exact. Host builds never use this. */

#include <stdint.h>
#include "mp3_decoder.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Open the hardware stream at the decoder's rate. Calls mmInit once.
 * Returns 0 on success. */
int audio_hw_open(Mp3Decoder *dec);

/* Feed decoded PCM into the ring (bounded: <=2 MP3 frames per call).
 * Call once per frame from the main loop while playing. */
void audio_hw_pump(void);

/* Must be called every frame (drives mmStreamUpdate). */
void audio_hw_update(void);

void audio_hw_close(void);

/* Samples handed to hardware since open/seek (for the lyric clock). */
uint32_t audio_hw_position_ms(void);
uint32_t audio_hw_rate(void);
int audio_hw_underruns(void);
int audio_hw_level(void); /* 0..8 smoothed output level for EQ */

/* True when decoder hit EOS and all buffered audio drained. */
int audio_hw_drained(void);

/* Pause gating: callback emits silence without advancing position. */
void audio_hw_set_paused(int paused);

/* Seek: flush ring, reset clock baseline, seek decoder. */
int audio_hw_seek_ms(uint32_t ms);

void audio_hw_set_volume(int vol127);

#ifdef __cplusplus
}
#endif

#endif

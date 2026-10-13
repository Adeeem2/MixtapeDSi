# MixtapeDSi — Architecture

## Modules

```
SD (/music/) --libfat--> music_library (scan, bounded 512 tracks)
MP3 file --> Mp3Decoder (dr_mp3 streaming, 8 KiB file buf)
Mp3Decoder --> AudioStream (2x 2304-frame PCM bufs, pump/consumed)
AudioStream --> libnds PCM channel (ARM9 fill, DMA consume)
AudioStream clock --> PlaybackController (samples_played / rate = ms)
Playback clock --> lyric_timeline (binary search active line)
UI views read controller + timeline; never block pump.
```

## Audio pipeline detail

- `mp3_open`: skips ID3v2 (`metadata_audio_offset`), inits Helix decoder.
- Main loop calls `audio_stream_pump()` once per VBlank (legacy host path) /
  `audio_hw_pump()` on NDS: decodes **bounded frames** (Helix integer path,
  ~10x cheaper than float) into a ~420 ms ring; resamples >32768 Hz files to
  maxmod's 32768 Hz ceiling with an exact phase-carry linear interpolator.
- HW consume callback calls `audio_stream_consumed(frames)`; position = `(decoded - pending) / rate`.
- Track change: `audio_stream_close/open` (flushes stale PCM — no bleed), `pb_play_index` resets clock, LRC reloaded.
- Seek: `mp3_seek_ms` (dr_mp3 frame seek) + `pb_seek_ms`; pending buffers flushed.
- Underrun accounting: `underruns` counter incremented on decode error/starve; displayed in Settings (debug).

## Lyric sync

- `lrc_parse` builds sorted `LyricLine[timestamp_ms, text]` (max 256, truncation flagged).
- Per frame: `pos = audio_stream_position_ms()` (or `pb_position_ms` fallback) -> `lyric_active_index` (binary search) -> redraw **only if index changed** (`lyric_index_changed`).
- Pause: clock frozen (no pump, no advance). Seek: index recomputed from new clock (handles forward/backward jumps). Track end: song replaced, index resets.

## Memory budget (DS mode, 4 MB)

- ARM9 binary + libnds: ~1 MB; dr_mp3 working: ~200 KB; inbuf 8 KB; PCM 2x2304x2chx2B = ~18 KB; LRC 256x256 B = 64 KB; library 512 tracks x ~900 B = ~460 KB (largest; acceptable, else reduce LIB_MAX_TRACKS). Total < 2 MB.

## UI

- Views: Now Playing / Lyrics / Library / Settings. Lyrics screen selectable TOP/BOTTOM via config. Reduced-animation mode skips scroll interpolation and EQ bars.
- 256x192 layout, console text in v1; sprite/BG upgrade path keeps same view interfaces.

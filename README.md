# MixtapeDSi — Spotify-inspired music player for Nintendo DSi

Native DSi homebrew (devkitARM + libnds + libfat, C99): MP3 playback from SD,
synchronized karaoke LRC lyrics, dual-screen UI (256x192 x2), library/shuffle/
repeat, lid-close-aware design. Original visual identity, no Spotify affiliation.

**Status:** portable engine + host tests complete and passing (58 checks).
The native UI uses tiled 8bpp panels, custom Unscii text, runtime cover art,
audio-driven EQ/progress, and a top-only lyric stage.
Emulator/hardware playback verification is the remaining step — see docs.

## NDS ROM (built and validated 2026-10-13)

Toolchain: portable MSYS2 + devkitPro `nds-dev` (devkitARM r68, libnds 2.0.2,
ndstool 2.3.1, stock calico ARM7 core), no-admin install.
Build: `source /etc/profile.d/devkit-env.sh && make` in devkitPro MSYS2 shell.

`mixtapedsi.nds` (322,560 bytes): Logo CRC OK, Header CRC OK, Banner CRC OK,
banner "MixtapeDSi / DSi music player". ARM9 uses the fixed-point Helix decoder
and a bounded PCM stream; ARM7 remains the stock calico core.

## Features

- MP3 streaming (fixed-point Helix backend on NDS, bounded file + PCM buffers).
- LRC parser: multi-timestamp lines, inline tags, unsorted/sloppy input, UTF-8
  passthrough, 256-line bound with truncation flag; binary-search sync clocked
  from decoded audio (not frame count).
- Library: recursive `/music/` scan, filename sort, sibling `.lrc`/`cover.bmp`
  discovery, ID3v1 + ID3v2.3 text frames with filename fallback.
- Controls: Home menu on boot (no autoplay). A play/pause, B home,
  X lyrics, Y repeat, A (in Settings) shuffle, L/R prev/next,
  Left/Right (hold) seek, Up/Down navigate/volume, START cycle views.
  Lyrics screen top/bottom + shuffle/repeat/volume in Settings (auto-saved).

## Quick start (host tests)

```sh
py run_tests.py        # or: python3 run_tests.py  (needs gcc on PATH)
```

## Build the .nds (needs devkitPro)

1. Install devkitPro devkitARM + libnds: https://devkitpro.org/wiki/Getting_Started
2. Ensure `third_party/helix/` is present (the fixed-point decoder is vendored).
3. `make` -> `mixtapedsi.nds`
4. Copy to SD: `mixtapedsi.nds` + your `/music/*.mp3` + sibling `.lrc` files.
5. Launch via TWiLight Menu++ / Unlaunch (DSi) or flashcard (DS mode).

SD layout (flat — just drop songs + lyrics side by side in `/music/`):

```
/music/
  Close to you.mp3
  Close to you.lrc        (same name as the MP3, optional)
  Another song.mp3        (no .lrc? plays fine, shows "instrumental")
  cover.bmp               (optional, shown when present)
```

Subfolders also work (`/music/Naza/...`) — the scanner is recursive and
matches each MP3 with the `.lrc` sharing its basename.

## Docs

- docs/RESEARCH.md — repo survey, decoder choice and native UI references.
- docs/ARCHITECTURE.md — pipeline, clock, memory budget.
- docs/LID_CLOSE.md — lid feasibility + hardware POC procedure (pending).
- docs/TESTING.md — host results (pass) + emulator/hardware checklists (pending).
- docs/ASSET_LICENSES.md — licenses.

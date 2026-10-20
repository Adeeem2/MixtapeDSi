# MixtapeDSi v1.0.7 — Spotify-inspired music player for Nintendo DSi

Native DSi homebrew (devkitARM + libnds + libfat, C99): MP3 playback from SD,
synchronized karaoke LRC lyrics, dual-screen UI (256x192 x2), library/shuffle/
repeat, lid-close-aware design. Original visual identity, no Spotify affiliation.

**Status:** portable engine + host tests complete and passing.
The native UI uses tiled 8bpp panels, custom Unscii-8 UI text and the
generated 16px "Mixtape Sans" lyric face, audio-driven EQ/progress, a
top-only lyric stage with a 3-row karaoke sweep, stylus timeline
scrubbing, and a persistent glass player deck.
Emulator/hardware playback verification is the remaining step — see docs.

## NDS ROM (built and validated 2026-10-19)

Toolchain: portable MSYS2 + devkitPro `nds-dev` (devkitARM r68, libnds 2.0.2,
ndstool 2.3.1, stock calico ARM7 core), no-admin install.
Build: `source /etc/profile.d/devkit-env.sh && make` in devkitPro MSYS2 shell.

`mixtapedsi.nds` (425,984 bytes): Logo CRC OK, Header CRC OK, Banner CRC OK,
banner "MixtapeDSi / DSi music player / v1.0.7", header title `NDS.TinyFB`
(homebrew marker — see the boot note below). ARM9 uses the fixed-point Helix
decoder and a bounded PCM stream; ARM7 remains the stock calico core.

### DSi boot note (old TWiLight Menu++ / nds-bootstrap)

TWiLight Menu++ v15 and nds-bootstrap v0.35 (2020 releases) detect homebrew by
reading the first instructions at the ARM9 entry and expecting the libnds 1.x
startup signature. libnds 2.x (2024) starts with a bootstub instead, so those
launchers classify the ROM as a retail game and nds-bootstrap stops with
"An error has occurred". Emulators ignore this and boot fine.

TWiLight also has a second, title-based homebrew marker: the ROM header title
`NDS.TinyFB`. The build sets it with `-g MXDX 00 "NDS.TinyFB"`, so the launcher
treats the ROM as modern homebrew and direct-boots it, bypassing nds-bootstrap
entirely. Nothing on the console or in TWiLight's settings needs to change.
Current TWiLight releases default unknown ROMs to homebrew, so the marker is
harmless there. If you build the ROM by hand, keep the `-g` argument.

## Features

- MP3 streaming (fixed-point Helix backend on NDS, bounded file + PCM buffers).
- LRC parser: multi-timestamp lines, inline tags, unsorted/sloppy input, UTF-8
  passthrough, 256-line bound with truncation flag; binary-search sync clocked
  from decoded audio (not frame count).
- Karaoke top stage: 16px Mixtape Sans active line with a per-glyph sweep
  (sung aqua / unsung steel) interpolated from the line duration, up to three
  wrapped rows of 30 columns (90 characters), 8px prev/next context lines,
  and an eased sub-tile glide on line changes (ANIM_FULL). No 3D layer.
- Stylus controls: tap the segmented tabs, transport discs, heart or list
  rows on any view; press/drag the timeline rail to preview a position and
  release to seek. Keyboard Left/Right also seek in 5 s steps.
- Library: recursive `/music/` scan, filename sort, sibling `.lrc`
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
3. `make` -> `mixtapedsi.nds` (the `NDS.TinyFB` homebrew marker is set
   automatically)
4. Copy to SD: `mixtapedsi.nds` + your `/music/*.mp3` + sibling `.lrc` files.
5. Launch via TWiLight Menu++ / Unlaunch (DSi) or flashcard (DS mode). The
   ROM direct-boots on 2020-era TWiLight Menu++ / nds-bootstrap — see the
   boot note above.

SD layout (flat — just drop songs + lyrics side by side in `/music/`):

```
/music/
  Close to you.mp3
  Close to you.lrc        (same name as the MP3, optional)
  Another song.mp3        (no .lrc? plays fine, shows "instrumental")
```

Subfolders also work (`/music/Naza/...`) — the scanner is recursive and
matches each MP3 with the `.lrc` sharing its basename.

## Docs

- docs/RESEARCH.md — repo survey, decoder choice and native UI references.
- docs/ARCHITECTURE.md — pipeline, clock, memory budget.
- docs/LID_CLOSE.md — lid feasibility + hardware POC procedure (pending).
- docs/TESTING.md — host results (pass) + emulator/hardware checklists (pending).
- docs/ASSET_LICENSES.md — licenses.

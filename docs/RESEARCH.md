# MixtapeDSi — Research

Target: **original Nintendo DSi** (DS mode / DSi-enhanced `.nds` via TWiLight Menu++ / Unlaunch / flashcard).
Date: 2026-10-13. Method: README + source inspection via web fetch; GBATEK/libnds API knowledge.

## 1. Investigated repositories

### libnds — https://github.com/devkitPro/libnds
- C library for NDS: video (2x 256x192), input incl. `KEY_LID`, sound (`soundEnable()`, streamed PCM), timers, DSi extensions (SD via `fatInitDefault()`, larger RAM in DSi mode).
- Active (1000+ commits, devkitPro maintained). License: custom permissive zlib-style (`libnds_license.txt`).
- Verdict: **chosen** as base SDK + libfat (bundled) for FAT/SD. Use `dirent` scanning, `swiWaitForVBlank` loop, never `systemSleep()` in player (see LID_CLOSE.md).

### devkitPro Maxmod — https://github.com/devkitPro/maxmod
- GBA/NDS tracker sound system: MOD/XM/S3M/IT + WAV/MS-ADPCM samples, `mmStream` mixer.
- **Verified: no MP3 decoding** (source = mixer + sample formats only). License: permissive (Maxmod license, ISC-like).
- Verdict: **no MP3 decoding** (verified in source: mixer + sample formats
  only — confirmed again against maxmod-nds 2.1.0 headers). **Chosen as the
  PCM output path**: ARM9 `mmStreamOpen` (16-bit stereo, manual fill,
  `mmStreamUpdate()` per frame; see `audio/maxmod/streaming` example) fed by
  dr_mp3, with the stock calico ARM7 core (`mmInstall`) on the ARM7 side.

### ctrmus (3DS) — https://github.com/deltabeard/ctrmus
- 3DS music player (C, GPL-3.0). Decodes MP3 via **mpg123**, FLAC/Vorbis/Opus; directory browser; plays with lid closed.
- Key learning: streaming file -> decode -> DSP double buffer; `mpg123` too heavy for DS (ARM9 67 MHz, 4 MB); 3DS closed-lid works because 3DS OS keeps DSP alive — **not transferable to DSi** (different PM/sleep hardware).
- Verdict: **architecture reference only** (browser + pump loop). License GPL-3.0 noted; no code copied.

### PandaFlow / ClouDS Music (3DS, LRC lyrics)
- PandaFlow: https://db.universal-team.net/3ds/pandaflow ; ClouDS Music: https://github.com/cadl/ClouDS-Music
- 3DS-only (libctru/NDSP). Useful LRC UX reference (active-line highlight, prev/next context). Not portable to ARM9/libnds.
- Verdict: **UX reference only**.

### DS-Homebrew wiki — https://wiki.ds-homebrew.com/ds-index/homebrew
- Confirms toolchain: devkitARM + libnds + libfat; DLDI patching for slot-1, native SD in DSi mode via TWiLight Menu++/Unlaunch; GBATEK (https://problemkaputt.de/gbatek.htm) as HW reference.
- Verdict: followed for build/launch docs.

### Additional DS/DSi-specific findings
- **Moonshell / DSOrganize / LMP-ng (Lick Media Player, Tuna-viDS)**: proven DS MP3 players exist. Moonshell historically used the **Helix fixed-point MP3 decoder** on ARM9 — proof MP3 @67 MHz is feasible (CBR ≤192 kbps typical).
- **Helix MP3 decoder** (fraunhofer IIS fixed-point, ~20 KB RAM): classic DS choice; restrictive-ish license history (now widely mirrored as "helix mp3 decoder" under RPSL-ish terms) — usable but attribution/care needed.
- **dr_mp3** (was chosen initially): float-DCT, no fixed-point mode. Proven
  too slow in practice: melonDS A/B test (stock maxmod sine = clean,
  commercial games = full speed, our dr_mp3 build = slow/choppy with `u:0`)
  showed decode CPU, not buffering, was the bottleneck. **Replaced.**
- **Helix fixed-point MP3 decoder** (`ultraembedded/libhelix-mp3`, RealNetworks
  RPSL/RCSL, vendored C core in `third_party/helix/`): integer-only, ARM-
  optimized (`__GNUC__`+`ARM` inline `smull` path), ~10x cheaper than float
  decoders on the FPU-less ARM9. The classic DS choice (Moonshell et al).
  Frame API (`MP3FindSyncWord`/`MP3Decode`) wrapped in `source/audio/
  mp3_decoder.c` with refill/resync/seek logic. **Chosen.**
- **minimp3** (lieff, CC0): verified floating-point (SSE/NEON) — same ARM9
  problem as dr_mp3. keyj's fixed-point fork fails ISO conformance. Rejected.
- **minimp3** high-level/ex API not used.
- **minimp3** (https://github.com/lieff/minimp3, CC0): fallback candidate; smaller but no seek helper — rejected for now (dr_mp3 seek needed for lyric-accurate seeking).
- **NightFox Lib** (https://github.com/knightfox75/nds_nflib): sprite/text helper, MIT-ish — optional, not required for v1 (console + tiled BG suffice).
- **mpg123 / libmad**: accurate but FPU/float-heavy or large tables; overkill vs dr_mp3/Helix on ARM9 — rejected.

## 2. Candidate MP3 decoders (DSi suitability)

| Decoder | RAM | CPU @67MHz | Seek | License | Verdict |
|---|---|---|---|---|---|
| Helix fixed-point | ~30 KB + tables | fast (integer, ARM asm) | byte-estimate + resync | RPSL/RCSL (vendored w/ license) | **chosen** |
| dr_mp3 | ~100-300 KB | too slow (float, no FPU) | yes | public domain | rejected after A/B test |
| minimp3 (lieff) | ~50-100 KB | too slow (float/SSE) | manual | CC0 | rejected (float) |
| minimp3 (keyj) | small | fast (fixed) | manual | unclear + fails conformance | rejected (quality) |
| mpg123 | MBs | too heavy | yes | LGPL | rejected |
| Maxmod | n/a (no MP3) | n/a | n/a | permissive | output path only |

Measurement plan on hardware (docs/TESTING.md): buffer underrun counter over 30-min session, 128/192/320 kbps files, DSi vs DS mode.

## 3. Candidate UI/graphics

- libnds console + tiled BGs (chosen for v1: zero deps, deterministic, 256x192-safe).
- NightFox Lib (optional later for sprites/transitions).
- easyGL2D (bundled example; overkill) — rejected.

## 4. Chosen architecture

- devkitARM + libnds + libfat; C99; `dr_mp3` streaming (`Mp3Decoder` wrapper) -> double-buffer `AudioStream` -> libnds PCM channel; clock = decoded frames minus pending; `PlaybackController` queue/shuffle/repeat; `LrcSong` + binary-search timeline driven by audio clock only.
- See docs/ARCHITECTURE.md.

## 5. Rejected alternatives

- 3DS stack (libctru/NDSP), mpg123, Maxmod-as-decoder, mobile UI paradigms, whole-file decode, float-heavy effects.

## 6. Unresolved / needs hardware

- Closed-lid behavior (see docs/LID_CLOSE.md), exact CPU headroom at 320 kbps VBR, DSi-mode SD throughput, cover.bmp decode cost (using preconverted 96x96 BMP v1 to avoid PNG).

## 7. Required native UI reference survey

The redesign was checked against current upstream source rather than desktop
or 3DS implementations:

- **Woopsi** (`ant512/Woopsi`, `source/` and `README.md`) provides retained-mode
  C++ widgets, clipping, scrolling panels, and 16-bit bitmap rendering. Its
  older devkitARM/C++ build and independent VRAM ownership conflict with this
  C99 tiled-background renderer, so it remains a technique reference only.
- **TWiLight Menu++** (`DS-Homebrew/TWiLightMenu`, `arm9/source/` and
  `graphics/`) demonstrates production DS screen setup, theme-driven selection
  states, box-art layout, VRAM/OAM setup, and grit/bin conversion rules. Its
  GPL assets and menu-specific runtime are not copied.
- **libnds** (`devkitPro/libnds`, `include/nds/arm9/{video,background,console,
  input,sprite}.h`) is the compatible hardware API used here for dual-screen
  modes, tiled backgrounds, custom console fonts, key/touch input, VBlank,
  DMA, and palette/VRAM control.
- **NightFox's Lib** (`knightfox75/nds_nflib`, `source/nf_tiledbg.c`,
  `source/nf_text.c`, `source/nf_sprite256.c`) supplies useful examples of
  dirty tiled maps, 8x8/8x16 text, sprites, and scrolling. Its current build
  path targets BlocksDS and is not added as a dependency; this project keeps
  the smaller libnds-only implementation.

The shipped UI keeps cached 8bpp panels and dynamic map cells, updates only
progress/EQ/transport cells, and uses the real audio position for lyric
transitions. The active lyric remains on the top display on every bottom-screen
view; no lyric text is drawn by library, queue, or settings views.

## 8. Additional current DS UI tooling research

The current BlocksDS SDK examples were also checked for a full replacement
path. `examples/graphics_2d/bg_regular_metamap` demonstrates compact metatile
maps expanded into hardware tilemaps; `sprites_animated` demonstrates either
preloaded animation frames or bounded frame copies; and the shared-palette and
extended-palette examples show the appropriate VRAM/OAM patterns. These
examples are CC0 and compatible with the native DS graphics model, but they do
not justify adding a new dependency for this player.

NFLib remains a viable MIT-licensed option for buffered text layers, named
tiled backgrounds, 256-colour sprites, and scrolling, but its current install
path targets BlocksDS rather than this repository's existing devkitPro setup.
Nitro Engine was rejected for this redesign because its GUI/text path is
3D-rendered and the application does not otherwise use the DS 3D engine.
TWiLight Menu++ remains a reference for VBlank-safe resource staging and
selection/scroll behavior; its GPL application and separately licensed theme
assets are not copied.

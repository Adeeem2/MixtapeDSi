# MixtapeDSi — Testing

## Automated host tests (EXECUTED 2026-10-13, this environment)

Runner: `py run_tests.py` (gcc MinGW 6.3, C99). Result: **ALL PASS**.

- `test_lrc` — 24 checks: standard tags/timestamps, multi-timestamp lines, inline metadata+timestamps, UTF-8/French passthrough, malformed/blank/unsorted handling, empty file, ms precision, by/re/ve, duplicate timestamps, 400-line truncation bound, bare mm:ss.
- `test_timeline` — 54 checks: before-first (-1), exact match, between-lines, forward/backward seek, pause/resume clock, track end + repeat off/all/one, prev-track restart rule, deterministic shuffle coverage, ID3v1 + ID3v2 TIT2 parsing, audio-offset skip, sibling `.lrc` derivation, stream pump/consume clock, config parse, flat-folder scan, karaoke line duration (next timestamp / last-line track length / 800–12000 ms clamps), sweep positions (start, half, last glyph, past end, invalid index/width).
- `test_lyric_text` — 33 checks: UTF-8 decode (1/2/3-byte), French codepoints, display_fold (quotes/dash/ellipsis/arrows, CJK+emoji→`?`), wrap (single line, word-boundary break, max_lines cap, overlong hard-split incl. multibyte-safe split, empty/NULL/zero-width), two-row active-line wrapping.

## NDS build v1.0.7 (EXECUTED 2026-10-19, devkitPro MSYS2)

- "Midnight Glass" 2D deck: 3-row x 30-column 16px Mixtape Sans lyrics,
  8bpp glass card + vinyl, two-row seek rail, segmented tabs, panel views.
- Removed the GPU3D record; both engines now run `MODE_0_2D`.
- Unified touch dispatcher (tabs/heart/transport/lists on every view) and
  stylus timeline scrubbing with seek-on-release.
- Fixed v1.0.7 visual regressions from the first device test:
  - `eq_init()` rendered every EQ layout into both screens' art gfx,
    stamping the top strips over bottom art tiles 222-285 (vinyl, pill,
    rail row, transport). Split into `eq_init_top()` / `eq_init_bot()`.
  - `consoleSetFont(convertSingleColor=true)` rewrites glyph pixels to
    nibble 15, so text palette entries must be written at `bank*16+15`.
  - The same call stamps ANSI colours over every 16th palette entry,
    including art entries 143/159/175/191; art palette 159 is used by
    top-art row 10, which showed as a pure-red band. `ui_font_init()` now
    restores art palette 128-191 after loading the font; the unused
    RGB444 cover ramp (entries 192-255) was deleted.
- ROM `mixtapedsi.nds` (425984 bytes): zero warnings, `MAKE-RC=0`,
  banner "MixtapeDSi / DSi music player / v1.0.7", header title
  `NDS.TinyFB`, Logo/Header/Banner CRC OK.
- Fixed the real-DSi boot failure ("An error has occurred"): TWiLight
  Menu++ v15 / nds-bootstrap v0.35 detect homebrew from the libnds 1.x
  startup signature at the ARM9 entry. libnds 2.x starts with a bootstub,
  so the ROM was classified as a retail game and sent to nds-bootstrap's
  retail path, which cannot boot it. TWiLight's fallback homebrew marker
  is the header title `NDS.TinyFB`; the Makefile now passes
  `-g MXDX 00 "NDS.TinyFB"` to ndstool. No console or launcher changes
  are needed. The earlier unit-code / DSi-extended-header patch
  (`tools/ds_only.py`) was based on a wrong hypothesis and was removed.
- Control ROM `boottest_hello_hb.nds`: stock devkitPro `hello_world` with
  the same header title marker (Logo/Header CRC OK). Unpatched stock
  `hello_world` fails identically on the user's setup, so this ROM
  isolates the fix from the app code.
- On-console verification (pending, user): copy `boottest_hello_hb.nds`
  and `mixtapedsi.nds` to the SD card and launch both from TWiLight v15.
  Expected: the control ROM shows the libnds "Hello World!" screen and the
  player boots. If the control ROM boots but the player does not, the
  failure is in app code, not in the launcher. Make sure no per-game
  settings file (`_nds/TWiLightMenu/gamesettings/mixtapedsi.nds.ini`)
  overrides the defaults.
- Host tests: ALL PASS (24 + 54 + 33 + resample math + 32 decode checks).
  Emulator/hardware verification pending (user).

## NDS build v1.0.6 (EXECUTED 2026-10-19, devkitPro MSYS2)

- Top screen now runs `MODE_0_3D`: translucent spinning vinyl record on the
  GPU3D layer, clocked from the audio position; transparent rear plane.
- 16px Unscii-16 active lyric line with per-glyph karaoke sweep (aqua sung /
  steel unsung) and eased sub-tile glide on line changes.
- ROM `mixtapedsi.nds` (406528 bytes): `MAKE-RC=0`,
  banner "MixtapeDSi / DSi music player / v1.0.6".
- Host tests: ALL PASS (24 + 54 + 33 + resample math + 32 decode checks).
  Emulator/hardware verification pending (user).

## NDS build v1.0.5 (EXECUTED 2026-10-15 20:48, devkitPro MSYS2)

- Light theme: ice-white background instead of black, ink-navy text;
  lime green replaced by light blue #5AAAF0 everywhere.
- ROM `mixtapedsi.nds` (389120 bytes): `MAKE-RC=0`, zero warnings in app code,
  banner "MixtapeDSi / DSi music player / v1.0.5".
- Host tests: ALL PASS. Emulator/hardware verification pending (user).

- Frutiger Aero retheme: glossy aqua #2EE6FF + lime #A4E635 on deep-sea
  glass (mockups, tiles, text palette). Lyric card removed earlier; active
  lyric is plain aqua, prev/next steel, KARAOKE lime.
- Cover art removed: no cover image/frame/placeholder anywhere; metadata
  and all lists span the full card width with filename fallback.
- "Unknown artist" line hidden when untagged (row left blank).
- ROM `mixtapedsi.nds` (389120 bytes): `MAKE-RC=0`, zero warnings in app code,
  banner "MixtapeDSi / DSi music player / v1.0.0".
- Host tests: ALL PASS. Emulator/hardware verification pending (user).

- Fixes over v1.0.0: lyric card removed (plain accent text on stage),
  emerald/violet replaced by amber #FFB545 + sky #6EB8FF everywhere
  (mockups, tiles, text palette, cover placeholder), track names use ID3
  tags with filename fallback (no hardcoded demo strings; neutral
  "Nothing playing" idle), touch tabs + heart work on every view
  (previously NOW-only, felt frozen elsewhere).
- ROM `mixtapedsi.nds` (398336 bytes): `MAKE-RC=0`, zero warnings in app code,
  banner "MixtapeDSi / DSi music player / v1.0.0".
- Host tests: ALL PASS. Emulator/hardware verification pending (user).

- From-scratch rebuild per `ui.py` demo (`screen_lyrics.png` + `screen_player.png`):
  deleted `assets/` + `source/nds/main_nds.c`, regenerated everything.
- ROM `mixtapedsi.nds` (399360 bytes): `MAKE-RC=0`, zero warnings in app code,
  banner "MixtapeDSi / DSi music player / v1.0.0".
- Verified programmatically: static tiles match demo pixels (dividers, lyric
  card, pill, rails, transport glyphs incl. shuffle shape, heart, tab icons +
  underline; avg color distance within RGB555 quantization, deltas only where
  the demo bakes text/fill that the DS draws at runtime via text layer +
  dynamic tiles). Tile budget bot 274 / top 62 (limit 1024).
- Known visual deltas vs mockup (documented in docs/UI.md): flat BG instead of
  per-pixel glow, 8px active lyric instead of 16px, lyric card 2px taller,
  progress rail 1px up on bottom deck.
- Emulator/hardware verification pending (user tests on melonDS 1.1 + real
  DSi at `D:\nds`); DeSmuME 0.9.13 cannot boot libnds-2.x ROMs (see below).

## Older NDS build notes

- `make`: ARM7 (stock calico template core) + ARM9 (all portable modules with
  `-DARM9 -DNDS_BUILD -DUSE_DRMP3`, `-lmm9 -lfat -lnds9`) link clean; one fixed
  issue (missing `<stdio.h>` in `filesystem_nds.c` under newlib).
- `ndstool` ROM: valid — Logo CRC OK, Header CRC OK, Banner CRC OK, banner
  "MixtapeDSi / DSi music player", ARM9 entry 0x2004800, ARM7 0x2380000.
- Fixed along the way: real `drmp3_get_pcm_frame_count_file` does not exist
  (EOS now detected from 0-frame reads); icon must be paletted BMP.
- Warnings eliminated in app code (only third-party dr_mp3 internals untouched).

## Emulator results (EXECUTED 2026-10-13, DeSmuME 0.9.13 win64)

- `mixtapedsi.nds`: white screens, no output. Control test with stock
  `hello_world` from nds-examples: **identical white screen**; bogus path
  shows black (ROM does load). Conclusion: DeSmuME 0.9.13 predates the
  current toolchain (libnds 2.x/calico) and cannot boot its ROMs — not an
  app bug. Unit-code-0 patch did not help.
- Recommendation: test on melonDS (needs own BIOS/firmware dumps) or real
  DSi hardware.
- Bug fixed during investigation: `mixtapedsi.cfg` was opened before
  `fatInitDefault()`; config load now happens after SD init + chdir.

## Decoder history (all verified, see docs/RESEARCH.md)

- dr_mp3 (float): too slow on FPU-less ARM9 — slow/choppy with `u:0`.
  A/B proof: stock maxmod sine ROM clean + commercial games full speed.
- maxmod streams cap at 32768 Hz: 44.1k files played slow until a
  phase-carry linear resampler was added (host-tested exact).
- Helix fixed-point: refill threshold bug (64 B) cut every 8th frame at the
  8 KB chunk edge → error spiral. Fixed to 4 KB; user's 320 kbps file now
  decodes 5487/5487 frames, exact 143.3 s, clean EOS (host-verified).

## Physical console checklist (NOT YET RUN)

1. Install devkitPro (devkitARM + libnds): https://devkitpro.org/wiki/Getting_Started
2. `tools/fetch_drmp3.sh` (vendors dr_mp3), then `make`.
3. Run `mixtapedsi.nds` in melonDS (DSi BIOS): browse `music/`, play MP3, toggle lyrics (X), seek (Left/Right), shuffle/repeat (Y), lid-close via emulator lid option.

## Physical console checklist (NOT YET RUN)

- [ ] 30-min session, 128/192/320 kbps: no underruns (Settings counter).
- [ ] Lyric sync error < 100 ms vs audible downbeat at 3 sample points.
- [ ] Library 200+ files: scan time, no audio gap while browsing.
- [ ] Corrupt MP3 / missing LRC / missing cover handled with on-screen message.
- [ ] Lid-close POC per docs/LID_CLOSE.md (DS + DSi mode, headphones).
- [ ] Battery: full-charge playback hours noted.

## Honesty note

Only the host suites above are claimed passed. Emulator/hardware rows are explicitly pending.

# MixtapeDSi — Testing

## Automated host tests (EXECUTED 2026-10-13, this environment)

Runner: `py run_tests.py` (gcc MinGW 6.3, C99). Result: **ALL PASS**.

- `test_lrc` — 24 checks: standard tags/timestamps, multi-timestamp lines, inline metadata+timestamps, UTF-8/French passthrough, malformed/blank/unsorted handling, empty file, ms precision, by/re/ve, duplicate timestamps, 400-line truncation bound, bare mm:ss.
- `test_timeline` — 39 checks: before-first (-1), exact match, between-lines, forward/backward seek, pause/resume clock, track end + repeat off/all/one, prev-track restart rule, deterministic shuffle coverage, ID3v1 + ID3v2 TIT2 parsing, audio-offset skip, sibling `.lrc` derivation, stream pump/consume clock, config parse, flat-folder scan.
- `test_lyric_text` — 29 checks: UTF-8 decode (1/2/3-byte), French codepoints, display_fold (quotes/dash/ellipsis/arrows, CJK+emoji→`?`), wrap (single line, word-boundary break, max_lines cap, overlong hard-split incl. multibyte-safe split, empty/NULL/zero-width).

## NDS build (EXECUTED 2026-10-13 20:53, devkitPro MSYS2)

- Redesign ROM `mixtapedsi.nds` (396288 bytes): `MAKE-RC=0`, zero warnings in
  app code (`lyric_text.c`, `bg_backdrop.c`, `main_nds.c` link clean).
- Included: Unscii-8 console typeface, top-only lyric stage with wrap +
  empty states, palette-bank fix (covers now visible), ANIM policy,
  `test_lyric_text` green with the other host suites.
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

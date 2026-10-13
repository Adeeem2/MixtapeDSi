# MixtapeDSi — UI layout specification (256×192 per screen)

Design: premium handheld player, dark atmospheric surfaces, emerald and violet
light, and editorial spacing. Tiled 8bpp backdrops (palette bank 128–191)
provide the visual identity; the text layer is content over the art, not the
layout itself. Runtime covers use bank 192–255.

## Absolute rule

Lyrics appear on the TOP screen only. The bottom screen never shows lyrics.
`VIEW_LYRICS` is kept as an enum alias that renders the NOW deck at the
bottom; the top stage always shows lyrics on every bottom view.

## Top screen — lyric stage

- Rows 1–2: song title (emerald, centered) + artist (gray, centered).
- Header sides: tile EQ columns (8 Harvey, driven by real PCM level).
- Rows 8–9: previous line, gray, centered, wrapped to ≤2 lines.
- Rows 11–12: active line, bright emerald, centered, wrapped to ≤2 lines.
- Rows 14–15: next line, default text color, centered, wrapped to ≤2 lines.
- Row 22: `mm:ss / mm:ss` progress readout (thin tile progress strip lives
  on the bottom deck next to the cover).
- Empty states: "Nothing playing" (no track), "No synchronized lyrics"
  (track has no `.lrc` — never fabricated), "(intro...)" (before first line).
- Lyric change: instant recolor + recenter; vertical glide only on
  ANIM_FULL (gated so UI never stalls the decode pump).

Long lines wrap (greedy word wrap, UTF-8 codepoint-safe) within 28-column
margins; overlong words hard-split without breaking multibyte sequences.

## Bottom screen — control deck

- Header (rows ~1–2): emerald view title; Now Playing adds cover + text:
  64×64 cover art (cells col 2–9, rows 7–14; `cover.bmp` 24-bit or
  procedural placeholder), title (col 11+, ≤15 chars), artist (gray),
  times row, heart (like) tile at top-right.
- Playback region: tile seek bar (row 12, cols 11–30, knob tile) + 5 round
  touch buttons at y=132: shuffle / prev / play-pause (prominent, ringed) /
  next / repeat. Touch zones match the drawn rings; SELECT also toggles like.
- Navigation region: tab bar (Library | Now Playing | Queue) with emerald
  underline on the active tab; all three are touch zones.
- Library: scrollable two-line rows with emerald `>` selection marker,
  violet `*` currently-playing marker, title, and artist; six rows are shown
  at native resolution.
- Queue: current-track `*` marker, `NOW PLAYING`/`UP NEXT` state, and the same
  selection treatment as Library.
- Settings: compact product rows for Lyrics TOP (fixed), animation mode,
  Shuffle, Repeat, and percentage Volume. Footer controls are styled labels,
  not a debug dump.

## Type

Unscii-8 (public domain) is loaded as a compact DS tile font, but finished
screens no longer render through `iprintf` or terminal cursor layout. A small
direct tile-map compositor writes bounded Latin-1 glyph runs with explicit
alignment and palette roles (primary, muted, emerald, violet). UTF-8 lyrics
are folded to Latin-1 before composition, preserving French accents.
Background art supplies hierarchy and atmosphere; text is content layered over
it rather than a collection of text-mode boxes.

## Hardware-driven visual limitations

- Active lyric size: a larger 8×16 lyric face exists (`assets/font_lyr.h`)
  but enabling it needs a second tile layer + custom blitter, which does not
  fit the VRAM budget alongside the 8bpp UI backdrops and 64-tile cover
  art. Active line uses bright emerald + centering instead.
- Cover art is palette-quantized (RGB444 uniform ramp, 64 colors) and fixed
  at 64×64 24-bit BMP; other sizes fall back to the placeholder.
- Lyric glide runs only on ANIM_FULL: smooth multi-frame scrolls would stall
  the ARM9 decode pump (~67 ms without `audio_hw_pump`), risking underruns.

## Animation policy (`g_cfg.anim`)

- FULL: EQ + emerald/violet ambient pulse + lyric glide.
- REDUCED: EQ + ambient pulse, no glide.
- OFF: static EQ/palette (one clear pass), no glide, no pulse.
- Song clock is always the audio HW position — never a frame counter.

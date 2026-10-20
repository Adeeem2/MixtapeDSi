# MixtapeDSi v1.0.7 — UI layout specification (256×192 per screen)

"The Midnight Glass" deck. The two demo PNGs (`screen_lyrics.png`,
`screen_player.png`) are historical references only; the shipped art is
generated from `tools/make_ui.py` and is not tied to those mockups.

## Palette (RGB888)

- Background gradient: `#07101C` → `#0D1E30` (24 tile-row steps)
- Glass card `#0E1D2E`, raised glass `#142B42`
- Aqua accent `#2EE6FF`, violet `#9B7BFF`
- Text `#EDF7FF`, muted `#94BDF7`, dim `#6B8CB5`
- Divider `#24405C`, empty rail `#1B3348`, tab bar `#0A1826`

All text roles are cool blue/aqua/violet; the deck has no green or red
tokens. Palette ownership is strict: text roles occupy entries 0-127 and
static/runtime art owns 128-191. `consoleSetFont()` rewrites every 16th
palette entry with libnds ANSI colours, so `ui_font_init()` restores the
art bank after loading the font.

## Absolute rule

Lyrics appear on the TOP screen only. The bottom screen never shows lyrics.
`VIEW_LYRICS` is kept as an enum alias that renders the NOW deck at the
bottom; the top stage always shows lyrics on every bottom view.

## Top screen — lyric stage

Static art: 24-step vertical gradient, a soft three-layer glow ellipse
behind the lyric band, header divider y=32 with an aqua pip, footer divider
y=156 with a violet pip, and the top progress rail base at y=164.

- Row 1: song title (near-white, centered). Row 2: artist (muted,
  centered). 30 columns max.
- Header sides: 4-bar EQ blocks (cols 2-3 and 28-29, rows 1-2), aqua bars
  with violet peaks, driven by the real PCM level.
- Row 8: previous line, muted, centered.
- Rows 11, 13, 15: active line, 16px Mixtape Sans, up to **three wrapped
  rows of 30 columns** (90 characters). Sung prefix is aqua; unsung text
  is muted. The three-row band keeps the ends of long LRC lines on screen.
- Row 17: next line, muted, centered.
- Row 20: progress rail (dynamic tiles, cols 3-28, track + knob).
- Row 21: elapsed / total (cols 3 / 24), `KARAOKE` centered violet.
- Idle (no track): neutral "Nothing playing" / "Pick a track in Library",
  00:00 clock. Track names come from ID3 tags with filename fallback.
- Empty states: "No synchronized lyrics" (track has no `.lrc` — never
  fabricated), "(intro...)" (before the first line).
- Lyric change: on ANIM_FULL the active band glides up 16 px in eight
  eased sub-tile steps. The glide runs in the per-frame animator, never as
  a blocking multi-frame loop.

There is no 3D layer: the main engine runs plain `MODE_0_2D`.

## Bottom screen — player deck

Static art: header divider y=26 with an aqua pip; glass card x8-247
y30-111; a static vinyl illustration x18-82 y40-104; quality pill
x92-154 y86-98; a **two-tile-row seek rail** at rows 15-16 (y120-135,
cols 1-30); transport wells at y=160; segmented tab rail y176-191 with
three 9×2 glass pills.

- Rows 1-2 (header): `NOW PLAYING` + `TRACK 03 / 12  PLAYING` (aqua/dim,
  cols 2-21). Bottom-right EQ block (cols 22-24, rows 0-2). Heart touch
  target cols 28-29, rows 1-2.
- Card lane, right of the vinyl: title 16px at rows 4-5 (col 11),
  artist row 8, album row 9, quality chip row 11 (inside the pill).
- Row 14: elapsed (aqua, col 1) and total (muted, col 24), `44.1 KHZ`
  centered dim.
- Rows 15-16: seek rail. The knob is a 12 px capsule spanning both rows;
  tile art is generated at 24 vertical gradient steps so it blends into
  the backdrop.
- Transport (cy=160): shuffle x32, prev x88, hero play/pause x128 (4×4
  cells), next x168, repeat x224 (all side buttons 2×2 cells). Icons are
  dynamic 8bpp tiles; the hero flips between play and pause.
- Tabs (y176-191): Library | Now | Queue. Each segment is a generated
  9×2 glass pill; the active one is filled aqua-tinted. Labels at row 22.

### Library / Queue / Home / Settings

These views call `ui_panel(1)`, which replaces the card interior
(rows 4-13, cols 1-30) with a flat glass panel using six generated
edge/body tiles. Leaving for NOW calls `ui_panel(0)` and restores the
pristine vinyl deck.

- Header row 1 (title) and row 2 (track/queue count).
- Five two-line entries in rows 4-13: title + artist (or `NOW PLAYING`
  / `UP NEXT`). The selected row is aqua with a `>` caret; the playing
  track is marked `*`.
- Home: four menu rows at rows 4, 6, 8, 10 plus a key hint row.
- Settings: animation, shuffle, repeat and volume rows; tapping a row
  cycles/toggles it. Key hints remain for the D-pad.

## Touch / stylus

All deck controls work on **every** view — this is the navigation fix:
tabs, heart, transport, and list rows are handled by one dispatcher in
the main loop, outside any view branch.

- Tap the tab rail (full thirds of the screen) to switch Library / Now /
  Queue.
- Tap the heart to like/unlike the current track.
- Tap the transport discs to play/pause, skip, shuffle or cycle repeat.
- Tap a Library or Queue row to start that track and jump to NOW.
- Tap Home rows to open a view; tap Settings rows to change a value.
- **Scrub the timeline**: press anywhere on the rail band (y116-143),
  drag, and the elapsed time, rail knob, and top-screen lyric preview
  follow the pen. Releasing commits `audio_hw_seek_ms()` and playback
  continues from that point. Keyboard Left/Right also seek in 5 s steps
  (repeating while held).

## Type

- UI text: Unscii-8 (public domain) as 4bpp console tiles; a direct
  tile-map compositor writes bounded Latin-1 runs with roles 0 main /
  1 aqua / 2 muted / 3 violet / 4 dim.
- Lyrics: **Mixtape Sans**, generated by `tools/make_lyric_font.py` from
  the OFL-licensed Barlow Semi Condensed SemiBold skeleton, refitted to
  our own 8×16 grid (squeeze, baseline, threshold, accents). 169 glyphs
  = two stacked tiles each, palette banks 5 ink / 6 aqua / 7 muted.
- UTF-8 lyrics fold to Latin-1 (French kept); curly quotes, dashes,
  ellipses and arrows fold to ASCII.

## Hardware-driven visual limitations

- The backdrop is a 24-step palette gradient, not a true per-pixel
  gradient; the lyric glow is three nested ellipse bands.
- Karaoke sweep resolution is one glyph: LRC files carry line
  timestamps, so the sweep position is interpolated across the line's
  duration. Enhanced per-word LRC timing is not parsed.
- Long lines beyond 90 characters (3 × 30 glyph cells) still clip; this
  is a hard screen-width limit.

## Animation policy (`g_cfg.anim`)

- FULL: karaoke sweep + lyric glide + EQs + ambient palette pulse.
- REDUCED: karaoke sweep + EQs + pulse; no glide.
- OFF: static full-aqua active line, no sweep, no pulse.
- Song clock is always the audio HW position (seek-baselined) — never a
  frame counter.
- The sweep/glide update runs every frame (bounded tile writes); full UI
  redraws still happen only when visible state changes.

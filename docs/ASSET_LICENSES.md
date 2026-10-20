# Asset & dependency licenses

- Code (this project): MIT (see LICENSE). Original visual identity; no Spotify logos/assets.
- libnds: devkitPro custom permissive license (`libnds_license.txt`, zlib-style). Required for NDS build; dynamically part of devkitPro install, not vendored.
- libfat: bundled with libnds, same permissive terms.
- Maxmod: not used (researched only).
- dr_mp3 (https://github.com/mackron/dr_libs): evaluated then rejected (float —
  too slow on FPU-less ARM9; see decoder table). No code used.
- Helix MP3 decoder (`third_party/helix/`, RealNetworks RPSL 1.0 / RCSL 1.0,
  vendored unmodified C core from ultraembedded/libhelix-mp3 + one marked
  plain-C fallback branch in `assembly.h` for host unit tests only).
  RPSL/RCSL/License texts kept alongside the code. No code copied from
  ctrmus/PandaFlow/ClouDS Music (reference only; ctrmus is GPL-3.0).
- Sample lyrics in `music/`: original placeholder text written for this project (CC0).
- Unscii-8 (`third_party/fonts/unscii-8.hex`, tools/make_fonts.py): public
  domain (author: Viznut; repo states CC-0 except Unifont-derived full builds,
  which are NOT used). Converted at build time to DS 4bpp UI tiles
  (`assets/font_ui.h`).
- Barlow Semi Condensed SemiBold (`third_party/fonts/barlow/`, SIL Open Font
  License 1.1 — `OFL.txt` kept alongside). `tools/make_lyric_font.py`
  refits the skeleton to an 8x16 4bpp grid and emits `assets/font_lyr.h`;
  the shipped glyphs are derived raster data, not the original font file.
- Reference repos (techniques studied, NO code/assets vendored):
  Woopsi (dormant C++ GUI framework — rejected: C++ ABI + own NitroFS/VRAM
  management conflicts with the maxmod/libnds-C engine), NightFoxLib
  (devkitPro support removed upstream — rejected as a dependency),
  TWiLight Menu++ (GPL-3.0 — studied theme/boxart layout + asset pipeline
  only, nothing copied), devkitPro/libnds (permissive, toolchain), knightfox
  nds_nflib (studied sprite/font/scroll examples only).

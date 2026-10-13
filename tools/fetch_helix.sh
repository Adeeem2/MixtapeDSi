#!/bin/sh
# Vendor Helix fixed-point MP3 decoder (RealNetworks RPSL/RCSL - see
# third_party/helix/*.txt and docs/ASSET_LICENSES.md) for NDS builds.
set -e
mkdir -p third_party/helix
if [ -f third_party/helix/mp3dec.h ]; then echo "helix already present"; exit 0; fi
URL="https://codeload.github.com/ultraembedded/libhelix-mp3/zip/refs/heads/master"
TMP="$(mktemp -d)/helix.zip"
if command -v curl >/dev/null; then curl -L "$URL" -o "$TMP"
elif command -v wget >/dev/null; then wget "$URL" -O "$TMP"
else echo "need curl or wget"; exit 1; fi
rm -rf third_party/helix_src && mkdir -p third_party/helix_src
unzip -q "$TMP" -d third_party/helix_src
SRC="third_party/helix_src/libhelix-mp3-master"
cp "$SRC/mp3dec.c" "$SRC/mp3tabs.c" third_party/helix/
cp "$SRC/pub/mp3dec.h" "$SRC/pub/mp3common.h" "$SRC/pub/statname.h" third_party/helix/
cp "$SRC/real/"*.c "$SRC/real/"*.h third_party/helix/
cp "$SRC/LICENSE.txt" "$SRC/RPSL.txt" "$SRC/RCSL.txt" third_party/helix/
echo "vendored helix (C core, no ADS asm)"

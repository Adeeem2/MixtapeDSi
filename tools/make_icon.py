#!/usr/bin/env python3
"""Generate icon.bmp (32x32 8-bit paletted) for the NDS banner: emerald EQ bars.
ndstool requires a palette, so we emit 8bpp with a 16-entry palette."""
import struct

W = H = 32
PAL = [  # B, G, R, 0
    (22, 18, 18), (36, 30, 30), (46, 204, 130), (80, 120, 27),
] + [(0, 0, 0)] * 12
BG, FRAME, FG, DIM = 0, 1, 2, 3

px = [[BG for _ in range(W)] for _ in range(H)]
for y in range(4, 28):
    for x in range(4, 28):
        if (x, y) in [(4, 4), (4, 27), (27, 4), (27, 27)]:
            continue
        px[y][x] = FRAME

bars = [10, 16, 20, 13, 17, 9]
for i, h in enumerate(bars):
    x0 = 6 + i * 4
    for x in (x0, x0 + 1):
        for k in range(h):
            y = 25 - k
            px[y][x] = FG if k < h - 3 else DIM

NPAL = 16
offset = 54 + NPAL * 4
row = W  # 8bpp, no padding needed at 32px
size = offset + row * H
with open("icon.bmp", "wb") as f:
    f.write(b"BM" + struct.pack("<IHHI", size, 0, 0, offset))
    f.write(struct.pack("<IiiHHIIiiII", 40, W, H, 1, 8, 0, 0, 0, 0, NPAL, 0))
    for b, g, r in PAL[:NPAL]:
        f.write(bytes((b, g, r, 0)))
    for y in range(H - 1, -1, -1):
        f.write(bytes(px[y]))
print("wrote icon.bmp (8-bit paletted)")

#!/usr/bin/env python3
"""Preview generated font headers as PNG (dev only, stdlib)."""
import re, struct, zlib


def chunk(typ, data):
    c = struct.pack(">I", len(data)) + typ + data
    return c + struct.pack(">I", zlib.crc32(typ + data) & 0xFFFFFFFF)


def nums(s):
    return [int(x) for x in s.replace("\n", "").split(",") if x.strip() != ""]


def sheet(path, name, out, tw, th, cols):
    src = open(path).read()
    tiles = nums(re.search(name + r"\[\d+\] = \{(.*?)\};", src, re.S).group(1))
    n = len(tiles) // (tw // 2 * th)
    rows = (n + cols - 1) // cols
    W, H = cols * tw, rows * th
    outb = bytearray()
    for y in range(H):
        outb.append(0)
        ty, py = divmod(y, th)
        for x in range(W):
            tx, q = divmod(x, tw)
            gi = ty * cols + tx
            v = 0
            if gi < n:
                byte = tiles[gi * (tw // 2 * th) + py * (tw // 2) + q // 2]
                v = (byte >> (0 if q % 2 == 0 else 4)) & 15
            g = 255 if v else 20
            outb += bytes((g, g, g))
    ihdr = struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0)
    open(out, "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) +
                          chunk(b"IDAT", zlib.compress(bytes(outb))) + chunk(b"IEND", b""))
    print(out, "ok")


sheet("assets/font_ui.h", "fontUiTiles", "preview_font_ui.png", 8, 8, 32)
sheet("assets/font_lyr.h", "fontLyrTiles", "preview_font_lyr.png", 8, 16, 32)

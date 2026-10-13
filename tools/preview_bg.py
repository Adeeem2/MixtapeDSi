#!/usr/bin/env python3
"""Render assets/bg_*.h backdrops to preview PNGs (dev only, stdlib)."""
import re, struct, zlib


def chunk(typ, data):
    c = struct.pack(">I", len(data)) + typ + data
    return c + struct.pack(">I", zlib.crc32(typ + data) & 0xFFFFFFFF)


def nums(s):
    return [int(x) for x in s.replace("\n", "").split(",") if x.strip() != ""]


def load(path, name):
    src = open(path).read()
    pal = nums(re.search(name + r"Pal\[\d+\] = \{([^}]*)\}", src).group(1))
    tiles = nums(re.search(name + r"Tiles\[\d+\] = \{(.*?)\};", src, re.S).group(1))
    amap = nums(re.search(name + r"Map\[\d+\] = \{(.*?)\};", src, re.S).group(1))
    rgb = [(((v) & 31) * 255 // 31, ((v >> 5) & 31) * 255 // 31, ((v >> 10) & 31) * 255 // 31) for v in pal]
    POFF = 128  # tiles already carry the offset; pal is full 256 entries
    get = lambda idx: rgb[idx] if 0 <= idx < len(rgb) else (0, 0, 0)
    W, H = 256, 192
    # straightforward scanline writer
    out = bytearray()
    for y in range(H):
        out.append(0)
        ty, py = divmod(y, 8)
        for x in range(W):
            tx, q = divmod(x, 8)
            out += bytes(get(tiles[amap[ty * 32 + tx] * 64 + py * 8 + q]))
    ihdr = struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0)
    png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) +
           chunk(b"IDAT", zlib.compress(bytes(out))) + chunk(b"IEND", b""))
    open(f"preview_{name}.png", "wb").write(png)
    print(f"preview_{name}.png ok")


load("assets/bg_top.h", "bg_top")
load("assets/bg_bot.h", "bg_bot")

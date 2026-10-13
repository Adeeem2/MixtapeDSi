#!/usr/bin/env python3
"""Preview ui_*.h tilesets as PNGs (dev only, stdlib)."""
import re, struct, zlib


def chunk(typ, data):
    c = struct.pack(">I", len(data)) + typ + data
    return c + struct.pack(">I", zlib.crc32(typ + data) & 0xFFFFFFFF)


def nums(s):
    return [int(x) for x in s.replace("\n", "").split(",") if x.strip() != ""]


def load(path, name, out):
    src = open(path).read()
    pal = nums(re.search(name + r"Pal\[\d+\] = \{([^}]*)\}", src).group(1))
    tiles = nums(re.search(name + r"Tiles\[\d+\] = \{(.*?)\};", src, re.S).group(1))
    amap = nums(re.search(name + r"Map\[\d+\] = \{(.*?)\};", src, re.S).group(1))
    rgb = [(((v) & 31) * 255 // 31, ((v >> 5) & 31) * 255 // 31, ((v >> 10) & 31) * 255 // 31) for v in pal]
    W, H = 256, 192
    outb = bytearray()
    for y in range(H):
        outb.append(0)
        ty, py = divmod(y, 8)
        for x in range(W):
            tx, q = divmod(x, 8)
            v = tiles[amap[ty * 32 + tx] * 64 + py * 8 + q]
            outb += bytes(rgb[v] if v < len(rgb) else (255, 0, 255))
    ihdr = struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0)
    png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) +
           chunk(b"IDAT", zlib.compress(bytes(outb))) + chunk(b"IEND", b""))
    open(out, "wb").write(png)
    print(out, "ok")


load("assets/ui_bot.h", "ui_bot", "preview_ui_bot.png")
load("assets/ui_top.h", "ui_top", "preview_ui_top.png")

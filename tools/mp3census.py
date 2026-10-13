#!/usr/bin/env python3
"""Census MPEG frame headers in an MP3 file."""
import sys, struct

BITRATE_MPEG1_L3 = [0,32,40,48,56,64,80,96,112,128,160,192,224,256,320]
SR_MPEG1 = [44100, 48000, 32000]
SR_MPEG2 = [22050, 24000, 16000]
SR_MPEG25 = [11025, 12000, 8000]

path = sys.argv[1]
data = open(path, "rb").read()
print(f"size={len(data)}")

pos = 0
if data[:3] == b"ID3":
    sz = ((data[6] & 0x7F) << 21) | ((data[7] & 0x7F) << 14) | ((data[8] & 0x7F) << 7) | (data[9] & 0x7F)
    print(f"ID3v2 size={sz} end={10+sz}")
    pos = 10 + sz

from collections import Counter
c = Counter()
n = 0
first = 20
bad = 0
rates = Counter()
while pos + 4 <= len(data) and n < 200000:
    if data[pos] == 0xFF and (data[pos+1] & 0xE0) == 0xE0:
        ver = (data[pos+1] >> 3) & 0x03
        lay = (data[pos+1] >> 1) & 0x03
        bri = (data[pos+2] >> 4) & 0x0F
        sri = (data[pos+2] >> 2) & 0x03
        pad = (data[pos+2] >> 1) & 0x01
        if ver == 1 or lay != 0x01 or bri in (0, 15) or sri == 3:
            bad += 1
            pos += 1
            continue
        vername = {3: "MPEG1", 2: "MPEG2", 0: "MPEG25"}.get(ver, "?")
        if ver == 3:
            rate = SR_MPEG1[sri]; br = BITRATE_MPEG1_L3[bri]
            flen = (144 * br * 1000 // rate) + pad
        elif ver == 2:
            rate = SR_MPEG2[sri]; br = BITRATE_MPEG1_L3[bri]
            flen = (72 * br * 1000 // rate) + pad
        else:
            rate = SR_MPEG25[sri]; br = BITRATE_MPEG1_L3[bri]
            flen = (72 * br * 1000 // rate) + pad
        c[(vername, br, rate)] += 1
        rates[rate] += 1
        if n < first:
            print(f"  frame@{pos}: {vername} {br}kbps {rate}Hz pad={pad} len={flen}")
        pos += flen if flen > 0 else 1
        n += 1
    else:
        bad += 1
        pos += 1
print(f"frames={n} top={(c.most_common(5))} rates={dict(rates)} garbage_bytes~={bad}")
# Xing?
ix = data.find(b"Xing", 0, 300000)
ix2 = data.find(b"Info", 0, 300000)
print(f"Xing@={ix} Info@={ix2}")

#!/usr/bin/env python3
"""Patch ROM unit code to 0 (DS-only) and fix header CRC16."""
import sys, struct

def crc16(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc & 0xFFFF

src, dst = sys.argv[1], sys.argv[2]
rom = bytearray(open(src, "rb").read())
print(f"unit code before: 0x{rom[0x12]:02X}")
rom[0x12] = 0x00
crc = crc16(bytes(rom[0:0x15E]))
struct.pack_into("<H", rom, 0x15E, crc)
open(dst, "wb").write(rom)
print(f"unit code after: 0x{rom[0x12]:02X}, header CRC=0x{crc:04X}")

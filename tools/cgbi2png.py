#!/usr/bin/env python3
"""cgbi2png.py in.png out.png : convert an Apple CgBI ("iPhone-optimised") PNG to a normal PNG (to inspect assets)."""
import sys, zlib, struct
d = open(sys.argv[1], "rb").read(); o = 8; idat = b""; ihdr = None; cgbi = False; pal = b""; trns = b""
while o < len(d):
    ln, = struct.unpack(">I", d[o:o + 4]); t = d[o + 4:o + 8]; c = d[o + 8:o + 8 + ln]; o += 12 + ln
    if t == b"CgBI": cgbi = True
    elif t == b"IHDR": ihdr = struct.unpack(">IIBBBBB", c)
    elif t == b"IDAT": idat += c
    elif t == b"PLTE": pal = c
    elif t == b"tRNS": trns = c
w, h, depth, ctype = ihdr[:4]
if not cgbi: open(sys.argv[2], "wb").write(d); sys.exit()
raw = zlib.decompressobj(-15).decompress(idat); ch = {6: 4, 2: 3, 0: 1, 4: 2, 3: 1}[ctype]; st = w * ch
rows = []; prev = bytearray(st)
for y in range(h):
    f = raw[y * (st + 1)]; line = bytearray(raw[y * (st + 1) + 1:(y + 1) * (st + 1)])
    for x in range(st):
        a = line[x - ch] if x >= ch else 0; b = prev[x]; c = prev[x - ch] if x >= ch else 0
        if f == 1: line[x] = (line[x] + a) & 255
        elif f == 2: line[x] = (line[x] + b) & 255
        elif f == 3: line[x] = (line[x] + (a + b) // 2) & 255
        elif f == 4:
            p = a + b - c; pa, pb, pc = abs(p - a), abs(p - b), abs(p - c); line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
    rows.append(line); prev = line
out = bytearray()
for line in rows:
    out.append(0)
    for x in range(w):
        if ctype == 6:
            b, g, r, a = line[x * 4:x * 4 + 4]
            if a and a < 255: r, g, b = min(255, r * 255 // a), min(255, g * 255 // a), min(255, b * 255 // a)
            out += bytes((r, g, b, a))
        elif ctype == 2: b, g, r = line[x * 3:x * 3 + 3]; out += bytes((r, g, b, 255))
        else: out += bytes((line[x],) * 3 + (255,))
def chunk(t, c): return struct.pack(">I", len(c)) + t + c + struct.pack(">I", zlib.crc32(t + c) & 0xffffffff)
ct = 6 if ctype in (6,) else 6
open(sys.argv[2], "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(bytes(out) if ctype == 6 else bytes(out), 6)) + chunk(b"IEND", b""))

#!/usr/bin/env python3
"""Decode compiled NIBArchive files.   nibdump.py <file.nib> [--tree]   |   nibdump.py --stats <dir>
Format (little endian): "NIBArchive", u32 ver, u32 ?, then (count,offset) pairs for objects, keys, values, classnames.
varint: 7 bits per byte, LSB group first, a set high bit marks the LAST byte.
object = varint class, varint firstValue, varint valueCount ; key = varint len + bytes ;
value = varint keyIndex, u8 type, payload (0:i8 1:i16 2:i32 3:i64 4:true 5:false 6:f32 7:f64 8:data(varint len) 9:nil 10:objref u32);
classname = varint len(incl NUL), varint extraCount, extraCount*u32, bytes."""
import struct, sys, os, collections

def varint(d, o):
    r = sh = 0
    while True:
        b = d[o]; o += 1; r |= (b & 0x7f) << sh; sh += 7
        if b & 0x80: return r, o

def parse(path):
    d = open(path, "rb").read(); assert d[:10] == b"NIBArchive"
    _, _, no, oo, nk, ko, nv, vo, nc, co = struct.unpack_from("<10I", d, 10)
    o = co; classes = []
    for _ in range(nc):
        ln, o = varint(d, o); ex, o = varint(d, o); o += 4 * ex; classes.append(d[o:o + ln - 1].decode()); o += ln
    o = ko; keys = []
    for _ in range(nk):
        ln, o = varint(d, o); keys.append(d[o:o + ln].decode()); o += ln
    o = vo; vals = []
    for _ in range(nv):
        k, o = varint(d, o); t = d[o]; o += 1
        if t == 0: v = struct.unpack_from("<b", d, o)[0]; o += 1
        elif t == 1: v = struct.unpack_from("<h", d, o)[0]; o += 2
        elif t == 2: v = struct.unpack_from("<i", d, o)[0]; o += 4
        elif t == 3: v = struct.unpack_from("<q", d, o)[0]; o += 8
        elif t == 4: v = True
        elif t == 5: v = False
        elif t == 6: v = struct.unpack_from("<f", d, o)[0]; o += 4
        elif t == 7: v = struct.unpack_from("<d", d, o)[0]; o += 8
        elif t == 8: ln, o = varint(d, o); v = d[o:o + ln]; o += ln
        elif t == 9: v = None
        elif t == 10: v = ("obj", struct.unpack_from("<I", d, o)[0]); o += 4
        else: raise ValueError(f"bad type {t} at {o}")
        vals.append((keys[k], t, v))
    o = oo; objs = []
    for _ in range(no):
        c, o = varint(d, o); f, o = varint(d, o); n, o = varint(d, o); objs.append((classes[c], vals[f:f + n]))
    return objs

def show(objs, i, depth=0, seen=None, maxd=6):
    seen = seen or set(); cls, vals = objs[i]; pad = "  " * depth
    print(f"{pad}#{i} {cls}")
    if i in seen or depth > maxd: return
    seen.add(i)
    for k, t, v in vals:
        if isinstance(v, tuple): print(f"{pad}  {k} ->"); show(objs, v[1], depth + 2, seen, maxd)
        else: print(f"{pad}  {k} = {v if not isinstance(v, bytes) or len(v) < 40 else v[:40].hex() + '...'}")

if __name__ == "__main__":
    if sys.argv[1] == "--stats":
        cc = collections.Counter(); kk = collections.Counter()
        for f in sorted(os.listdir(sys.argv[2])):
            if f.endswith(".nib"):
                for c, vals in parse(os.path.join(sys.argv[2], f)):
                    cc[c] += 1
                    for k, t, v in vals: kk[k] += 1
        print("CLASSES:", dict(cc.most_common())); print("KEYS:", dict(kk.most_common(80)))
    else: show(parse(sys.argv[1]), 0, maxd=8 if "--tree" in sys.argv else 3)

#!/usr/bin/env python3
"""Recover C++ classes + vtables from Itanium-ABI RTTI in the stripped binary.
usage: rtti.py <binary> <out.json>
  typeinfo object  = { vptr(bound to __cxxabiv1::*_type_info vtable), const char* mangled_name, [base typeinfo ...] }
  vtable           = { offset_to_top, typeinfo*, fn0, fn1, ... }
Output: {classname: {typeinfo, bases:[names], vtable, methods:[addr|0x1 flagged thumb]}}
Method slots are code pointers (odd = Thumb). Pure virtuals show as the import `___cxa_pure_virtual`.
"""
import sys, os, re, json, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ra_re import Binary

def demangle_simple(n):
    """'13PEventHandler' -> 'PEventHandler'; templates left mangled but readable"""
    m = re.match(r"^(\d+)(.*)$", n)
    if m:
        k = int(m.group(1)); return m.group(2)[:k] + (("<" + m.group(2)[k:] + ">") if m.group(2)[k:] else "")
    return n

def main(path, out):
    b = Binary(path)
    names = {}   # va -> mangled   (typeinfo name strings live in the first __const, not __cstring)
    for sec in b.f.sections:
        if sec.name not in ("__const", "__cstring") or sec.size == 0: continue
        sb = b.section_bytes(sec)
        for m in re.finditer(rb"(?<=\0)(\d+[A-Za-z_][A-Za-z0-9_]*|N\d[A-Za-z0-9_]+E)\0", b"\0" + sb):
            s_ = m.group(1).decode(); k = re.match(r"\d+", s_)
            if k and int(k.group()) <= len(s_) - len(k.group()):
                names[sec.virtual_address + m.start() - 1] = s_
    data_secs = [s for s in b.f.sections if s.name in ("__const", "__data", "__objc_const") and s.size]
    words = {}
    for s in data_secs:
        d = b.section_bytes(s)
        for o in range(0, len(d) - 3, 4):
            words[s.virtual_address + o] = struct.unpack_from("<I", d, o)[0]
    ti = {}   # typeinfo addr -> (name, [base typeinfo addrs])
    for a, v in words.items():
        if v in names and (a - 4) in words:
            bases = []
            nx = words.get(a + 4)
            if nx in names or nx is None: pass
            else: bases.append(nx)
            ti[a - 4] = (names[v], bases)
    byname = {n: a for a, (n, _) in ti.items()}
    res = {}
    for a, (n, bases) in ti.items():
        bn = [ti[x][0] for x in bases if x in ti]
        res[n] = dict(typeinfo=a, bases=bn, vtable=None, methods=[])
    for a, v in words.items():
        if v in ti and (a - 4) in words:     # vtable slot -1 -> typeinfo
            name = ti[v][0]
            vt = a + 4
            ms = []
            while vt in words:
                w = words[vt]
                if (b.in_text(w & ~1)) : ms.append(w)
                elif w in b.stubs: ms.append(w)
                elif w == 0 and not ms: pass
                else: break
                vt += 4
            if name in res and ms and res[name]["vtable"] is None:
                res[name]["vtable"] = a + 4; res[name]["methods"] = ms
    # clamp each vtable at the start of the next one (adjacent vtables otherwise run together)
    starts = sorted(r["vtable"] for r in res.values() if r["vtable"])
    for r in res.values():
        if r["vtable"]:
            nxt = next((x for x in starts if x > r["vtable"]), None)
            if nxt: r["methods"] = r["methods"][: max(0, (nxt - 8 - r["vtable"]) // 4)]
    for n, r in res.items():
        r["name"] = demangle_simple(n)
        r["bases"] = [demangle_simple(x) for x in r["bases"]]
    json.dump(res, open(out, "w"), indent=1)
    withvt = sum(1 for r in res.values() if r["vtable"])
    print(f"{len(res)} classes (RTTI), {withvt} with vtables, {sum(len(r['methods']) for r in res.values())} vtable slots -> {out}")

if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])

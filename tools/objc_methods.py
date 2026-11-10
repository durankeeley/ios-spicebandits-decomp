#!/usr/bin/env python3
"""Extract every Objective-C method (class, metaclass and category) from the Mach-O metadata.
usage: objc_methods.py <binary> <out.json>
Output: {"imp_addr(no thumb bit)": "-[Class sel]" / "+[Class sel]" / "-[Class(Cat) sel]"}
Objective-C 2.0 ABI, 32-bit: class_t{isa,super,cache,vtable,data}; class_ro_t{flags,start,size,ivarLayout,name,baseMethods,...};
method_list_t{entsize,count,{name,types,imp}...}; category_t{name,cls,instMethods,classMethods,...}.
IMPs of Thumb methods have bit 0 set."""
import sys, os, json, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ra_re import Binary

def main(path, out):
    b = Binary(path)
    segs = []
    for s in b.f.sections: segs.append((s.virtual_address, s.size, s))
    cache = {}
    def rd(a, n=4):
        for va, sz, s in segs:
            if va <= a < va + sz and s.offset:
                o = b.fat_off + s.offset + (a - va); return b.raw[o:o + n]
        return b"\0" * n
    def u32(a): return struct.unpack("<I", rd(a))[0]
    def cstr(a):
        if not a: return ""
        for va, sz, s in segs:
            if va <= a < va + sz and s.offset:
                o = b.fat_off + s.offset + (a - va); e = b.raw.index(b"\0", o); return b.raw[o:e].decode("latin1")
        return ""
    def methods(lst, cls, sign):
        res = {}
        if not lst: return res
        ent = u32(lst) & ~3; n = u32(lst + 4)
        for i in range(n):
            m = lst + 8 + i * ent
            imp = u32(m + 8)
            if imp: res[imp & ~1] = f"{sign}[{cls} {cstr(u32(m))}]"
        return res
    r = {}
    def sec(n):
        for s in b.f.sections:
            if s.name == n: return s
    cl = sec("__objc_classlist")
    names = {}
    if cl:
        for o in range(0, cl.size, 4):
            c = u32(cl.virtual_address + o); data = u32(c + 16) & ~3; meta = u32(c); mdata = u32(meta + 16) & ~3
            nm = cstr(u32(data + 16)); names[c] = nm
            r.update(methods(u32(data + 20), nm, "-")); r.update(methods(u32(mdata + 20), nm, "+"))
    ct = sec("__objc_catlist")
    if ct:
        for o in range(0, ct.size, 4):
            c = u32(ct.virtual_address + o); cat = cstr(u32(c)); cls = u32(c + 4)
            cn = names.get(cls) or (cstr(u32((u32(cls + 16) & ~3) + 16)) if cls and cls < 0x40000000 else "?")
            tag = f"{cn}({cat})"
            r.update(methods(u32(c + 8), tag, "-")); r.update(methods(u32(c + 12), tag, "+"))
    json.dump(r, open(out, "w"), indent=0)
    print(f"{len(r)} ObjC methods -> {out}")

if __name__ == "__main__": main(sys.argv[1], sys.argv[2])

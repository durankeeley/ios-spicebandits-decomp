#!/usr/bin/env python3
"""objcdis.py <tag> '<-[Class sel]>' : disassemble an Objective-C method, annotating selector loads (objc_msgSend arguments) with names."""
import sys, os, json, struct
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, f"{root}/tools")
from ra_disasm import Program
tag, want = sys.argv[1], sys.argv[2]
p = Program(f"{root}/work/{tag}/Payload/Tower.app/Tower").run(); b = p.b
meth = {int(k): v for k, v in json.load(open(f"{root}/work/{tag}/objc_methods.json")).items()}
addr = [a for a, n in meth.items() if n == want]
if not addr: sys.exit("no such method")
fn = p.functions(); m, body = fn[addr[0]]
def cstr_at(a):
    for s in b.f.sections:
        if s.virtual_address <= a < s.virtual_address + s.size and s.offset:
            o = b.fat_off + s.offset + a - s.virtual_address; return b.raw[o:b.raw.index(b"\0", o)].decode("latin1")
def rd(a):
    for s in b.f.sections:
        if s.virtual_address <= a < s.virtual_address + s.size and s.offset: return struct.unpack_from("<I", b.raw, b.fat_off + s.offset + a - s.virtual_address)[0]
sel = {}   # reg -> description of last PIC-computed address
def note(a, i):
    ops = i.op_str
    if i.mnemonic.startswith("movw"): sel[ops.split(",")[0].strip()] = ("lo", i.operands[1].imm & 0xffff, a)
    elif i.mnemonic.startswith("movt"):
        r = ops.split(",")[0].strip()
        if r in sel and sel[r][0] == "lo": sel[r] = ("val", (i.operands[1].imm & 0xffff) << 16 | sel[r][1], a)
    elif i.mnemonic.startswith("add") and "pc" in ops:
        r = ops.split(",")[0].strip()
        if r in sel and sel[r][0] == "val":
            t = (sel[r][1] + a + (4 if m else 8)) & 0xffffffff; v = rd(t); name = cstr_at(v) if v else None
            sel[r] = ("addr", t, a); return f"  ; &{t:#x}" + (f' = "{name}"' if name and name.isprintable() and len(name) < 60 else "")
    return ""
print(f"; {want} @ {addr[0]:#x} {'thumb' if m else 'arm'}")
for a, i in sorted(body.items()):
    n = note(a, i)
    if i.mnemonic.startswith("bl") and i.operands and i.operands[0].type == 2:
        t = i.operands[0].imm; n += "  ; " + (p.b.stubs.get(t) or meth.get(t & ~1) or f"sub_{t:x}")
    print(f"{a:08x}  {i.mnemonic:8s} {i.op_str}{n}")

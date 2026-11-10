#!/usr/bin/env python3
"""selxref.py <tag> <selector> [...] : which functions load this selector (ObjC selref xrefs), named via objc_methods.json"""
import sys, os, json, struct, bisect
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, f"{root}/tools")
from ra_disasm import Program
tag = sys.argv[1]; sels = sys.argv[2:]
p = Program(f"{root}/work/{tag}/Payload/Tower.app/Tower").run(); b = p.b
meth = {int(k): v for k, v in json.load(open(f"{root}/work/{tag}/objc_methods.json")).items()}; ks = sorted(meth)
def sec(n):
    for s in b.f.sections:
        if s.name == n: return s
def cstr_at(a):
    for s in b.f.sections:
        if s.virtual_address <= a < s.virtual_address + s.size:
            o = b.fat_off + s.offset + a - s.virtual_address; return b.raw[o:b.raw.index(b"\0", o)].decode("latin1")
sr = sec("__objc_selrefs"); targets = {}
for o in range(0, sr.size, 4):
    a = sr.virtual_address + o; v = struct.unpack_from("<I", b.raw, b.fat_off + sr.offset + o)[0]
    n = cstr_at(v)
    if n in sels: targets[a] = n
fn = p.functions(); hits = {}
for a, (m, i) in sorted(p.insns.items()):
    if i.mnemonic.startswith("ldr") and "[pc" in i.op_str and len(i.operands) > 1 and i.operands[1].type == 3 and i.operands[1].mem.index == 0:
        pc = (a + (4 if m else 8)) & ~3; lit = pc + i.operands[1].mem.disp
        if b.in_text(lit) and lit + 4 <= b.text_end:
            v = struct.unpack_from("<I", b.text, lit - b.text_va)[0]
            for off in range(1, 9):
                q = p.insns.get(a + off * (2 if m else 4)) or p.insns.get(a + 2 * off)
                if q and ((q[1].mnemonic.startswith("add") and "pc" in q[1].op_str) or (q[1].mnemonic == "ldr" and "[pc, r" in q[1].op_str)):
                    t = (v + q[1].address + (4 if q[0] else 8)) & 0xffffffff
                    if t in targets: hits.setdefault(targets[t], []).append(a)
                    break
# Thumb-2 / ARMv7 PIC: movw rX,#lo ; movt rX,#hi ; add rX, pc
for a, (m, i) in sorted(p.insns.items()):
    if i.mnemonic.startswith("movw") and len(i.operands) == 2:
        reg = i.op_str.split(",")[0].strip(); lo = i.operands[1].imm & 0xffff; step = 4 if m else 4
        nxt = p.insns.get(a + 4)
        if nxt and nxt[1].mnemonic.startswith("movt") and nxt[1].op_str.split(",")[0].strip() == reg:
            hi = nxt[1].operands[1].imm & 0xffff; val = (hi << 16) | lo
            for off in (8, 10, 12, 14, 16):
                q = p.insns.get(a + off)
                if q and q[1].mnemonic.startswith("add") and "pc" in q[1].op_str and q[1].op_str.split(",")[0].strip() == reg:
                    t = (val + q[1].address + (4 if q[0] else 8)) & 0xffffffff
                    if t in targets: hits.setdefault(targets[t], []).append(a)
                    break
for s, addrs in hits.items():
    names = set()
    for a in addrs:
        i = bisect.bisect_right(ks, a) - 1; names.add(meth[ks[i]] if i >= 0 else hex(a))
    print(s, "<-", sorted(names))

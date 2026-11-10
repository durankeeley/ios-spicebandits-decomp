#!/usr/bin/env python3
"""Disassemble one function with correct ARM/Thumb mode.
usage: disasm_fn.py <tag> <hex addr|ClassName::vfN>      e.g. disasm_fn.py v1.8.20 0x1639c"""
import sys, os, json
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(root, "tools"))
from ra_disasm import Program
tag, q = sys.argv[1], sys.argv[2]
p = Program(f"{root}/work/{tag}/Payload/Red Alert.app/Red Alert").run()
fn = p.functions()
if "::vf" in q:
    cls, k = q.split("::vf"); c = [r for r in json.load(open(f"{root}/work/{tag}/classes.json")).values() if r["name"] == cls][0]
    a = c["methods"][int(k)] & ~1
else:
    a = int(q, 16)
mode, body = fn[a]
print(f"; {q} @0x{a:x}  {'thumb' if mode else 'arm'}  {len(body)} insns")
for ad, i in sorted(body.items()):
    note = ""
    if i.operands and i.operands[0].type == 2 and i.operands[0].imm in p.b.stubs: note = "  ; " + p.b.stubs[i.operands[0].imm]
    elif i.operands and i.operands[0].type == 2 and i.mnemonic.startswith("bl") and i.operands[0].imm in p.entries: note = f"  ; sub_{i.operands[0].imm:x}"
    print(f"{ad:08x}  {i.mnemonic:8s} {i.op_str}{note}")

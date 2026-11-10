#!/usr/bin/env python3
"""Discover functions (mixed ARM/Thumb-2) and write work/<tag>/functions.json.
usage: find_functions.py <binary> <out.json>
Per function: mode(arm|thumb), size, callees, imported APIs (direct bl or address-taken via literal pool),
referenced C strings, how it was discovered, VFP/NEON use, whether it uses SjLj EH (has C++ try/catch or dtor cleanups).
"""
import sys, os, json, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ra_disasm import Program, ARM, THUMB
from capstone import arm_const as A

def main(path, out):
    p = Program(path).run()
    b = p.b
    funcs = {}
    for e, (mode, body) in p.functions().items():
        callees, imports, strings = set(), collections.Counter(), set()
        vfp = neon = 0
        end = e
        for a, i in sorted(body.items()):
            end = max(end, a + i.size)
            base = i.mnemonic.split(".")[0]
            if base in ("bl", "blx", "b") and i.operands and i.operands[0].type == A.ARM_OP_IMM:
                t = i.operands[0].imm
                if t in b.stubs: imports[b.stubs[t]] += 1
                elif t in p.entries and base != "b": callees.add(t)
            if base.startswith("v") and base not in ("vmov_",):
                vfp += 1
                if " q" in " " + i.op_str: neon += 1
            if base in ("ldr", "ldr.w") and "[pc" in i.op_str and len(i.operands) > 1 and i.operands[1].type == A.ARM_OP_MEM:
                pc = (a + (8 if mode == ARM else 4)) & ~3
                lit = pc + i.operands[1].mem.disp
                if b.in_text(lit) and lit + 4 <= b.text_end:
                    v = int.from_bytes(b.text[lit - b.text_va: lit - b.text_va + 4], "little")
                    if v in b.stubs: imports[b.stubs[v]] += 1
                    s = b.cstring_at(v)
                    if s and len(s) > 2 and s.isprintable(): strings.add(s)
        funcs[e] = dict(addr=e, mode="thumb" if mode == THUMB else "arm", size=sum(i.size for i in body.values()), span=end - e, ninsn=len(body),
                        callees=sorted(callees), imports=dict(imports), strings=sorted(strings)[:12],
                        vfp=vfp, neon=neon, via=p.seed_src.get(e, "?").split(":")[0].split("@")[0],
                        sjlj=("Unwind_SjLj_Register" in imports))
    json.dump(funcs, open(out, "w"), indent=0)
    cov = sum(i.size for _, i in p.insns.values())
    json.dump(dict(insns=len(p.insns), covered=cov, text=len(b.text), entries=len(p.entries),
                   thumb=sum(1 for m in p.entries.values() if m == THUMB), tables=len(p.tables)),
              open(out.replace("functions.json", "stats.json"), "w"))
    print(f"{len(funcs)} functions ({sum(1 for f in funcs.values() if f['mode']=='thumb')} thumb), "
          f"{cov*100//len(b.text)}% of __text decoded -> {out}")

if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])

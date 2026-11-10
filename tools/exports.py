#!/usr/bin/env python3
"""Extract the Mach-O exports trie (real, demangled C++ names) -> work/<tag>/exports.json {addr(no thumb bit): name}.
usage: exports.py <binary> <out.json>"""
import sys, json, subprocess
b, out = sys.argv[1], sys.argv[2]
t = subprocess.run(["llvm-objdump", "--macho", "--arch=armv7", "--exports-trie", b], capture_output=True, text=True).stdout
rows = [l.split() for l in t.splitlines() if l.strip().startswith("0x")]
mang = [r[1].lstrip("_") if not r[1].startswith("__Z") else r[1][1:] for r in rows]
dem = subprocess.run(["llvm-cxxfilt"], input="\n".join(mang), capture_output=True, text=True).stdout.splitlines()
res = {}
for r, d in zip(rows, dem):
    res[str(int(r[0], 16) & ~1)] = d
json.dump(res, open(out, "w"), indent=0)
print(f"{len(res)} exported symbols -> {out}")

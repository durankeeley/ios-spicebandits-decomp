#!/usr/bin/env python3
"""symbolize.py <tag> <hex addr>...   -> nearest exported symbol / discovered function for each address (thumb bit ignored)"""
import sys, os, json, bisect
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
tag = sys.argv[1]
ex = {int(k): v for k, v in json.load(open(f"{root}/work/{tag}/exports.json")).items()}
fn = sorted(int(k) for k in json.load(open(f"{root}/work/{tag}/functions.json")))
cl = json.load(open(f"{root}/work/{tag}/classes.json")) if os.path.exists(f"{root}/work/{tag}/classes.json") else {}
vs = {}
for r in cl.values():
    for k, m in enumerate(r["methods"]): vs.setdefault(m & ~1, f"{r['name']}::vf{k}")
for a in sys.argv[2:]:
    for tok in a.replace(",", " ").split():
        v = int(tok, 16) & ~1
        i = bisect.bisect_right(fn, v) - 1; f = fn[i] if i >= 0 else 0
        print(f"{v:08x}  fn {f:08x}+{v-f:<5x} {ex.get(f) or vs.get(f) or ''}")

#!/usr/bin/env python3
"""docs/IMPORTS.md: every imported API the game actually calls, with call-site counts and example callers.
usage: gen_imports.py <tag> [<tag>...]  (reads work/<tag>/functions.json)"""
import sys, os, json, collections
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
tags = sys.argv[1:]
L = ["# Imports used by the game (shim surface)\n",
     "Only imports that are *called* are listed (the Mach-O imports ~290 symbols; many are never referenced). "
     "`n` = static call sites, `fns` = functions containing them. Implement these in the host shim.\n"]
for tag in tags:
    fn = json.load(open(f"{root}/work/{tag}/functions.json"))
    use = collections.defaultdict(lambda: [0, []])
    for a, f in fn.items():
        for k, n in f["imports"].items():
            use[k][0] += n; use[k][1].append(int(a))
    L.append(f"\n## {tag} — {len(use)} distinct imports\n\n| symbol | n | fns | example callers |\n|---|---|---|---|")
    for k, (n, fs) in sorted(use.items(), key=lambda x: (x[0][:2] != "gl", x[0].lower())):
        L.append(f"| `{k}` | {n} | {len(fs)} | " + ", ".join(f"`0x{x:x}`" for x in sorted(fs)[:3]) + " |")
open(f"{root}/docs/IMPORTS.md", "w").write("\n".join(L) + "\n")
print("wrote docs/IMPORTS.md")

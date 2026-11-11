#!/usr/bin/env python3
"""seed_docs.py <docs-dir> : pre-mark every in-game tutorial as already shown (defaults.bin), so UI tests see the normal screens."""
import os, sys
d = sys.argv[1]; os.makedirs(d, exist_ok=True)
out = b""
for i in range(0, 64):
    k = f"TutorialType-{i}".encode(); out += b"s%d:" % len(k) + k + b"i1;"
open(os.path.join(d, "defaults.bin"), "wb").write(out)

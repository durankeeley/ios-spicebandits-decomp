#!/usr/bin/env python3
"""Image signature helper for the screenshot tests (no third-party deps).
  imgcheck.py sig   <shot.ppm>                 print a 32x24 grayscale signature
  imgcheck.py make  <shot.ppm> <golden.txt>    write a golden signature
  imgcheck.py cmp   <shot.ppm> <golden.txt> [max_mean_diff=14]   exit 0 if close
  imgcheck.py stats <shot.ppm>                 mean brightness / fraction of black pixels / red dominance
Goldens are tiny derived signatures, not copies of game art."""
import sys

def load(p):
    d = open(p, "rb").read(); parts = d.split(b"\n", 3)
    w, h = map(int, parts[1].split()); return w, h, parts[3]

def sig(p, gw=32, gh=24):
    w, h, px = load(p); out = []
    for gy in range(gh):
        for gx in range(gw):
            x0, x1, y0, y1 = gx * w // gw, (gx + 1) * w // gw, gy * h // gh, (gy + 1) * h // gh
            s = n = 0
            for y in range(y0, y1, 4):
                row = y * w * 3
                for x in range(x0, x1, 4):
                    o = row + x * 3; s += (px[o] * 30 + px[o + 1] * 59 + px[o + 2] * 11) // 100; n += 1
            out.append(s // max(n, 1))
    return out

def stats(p):
    w, h, px = load(p); n = tot = black = red = 0
    for i in range(0, len(px) - 2, 3 * 37):
        r, g, b = px[i], px[i + 1], px[i + 2]; n += 1; tot += (r + g + b) // 3
        if r + g + b < 24: black += 1
        if r > g + 30 and r > b + 30: red += 1
    return tot / n, black / n, red / n

if __name__ == "__main__":
    c = sys.argv[1]
    if c == "sig": print(" ".join(map(str, sig(sys.argv[2]))))
    elif c == "make": open(sys.argv[3], "w").write(" ".join(map(str, sig(sys.argv[2]))) + "\n")
    elif c == "cmp":
        g = list(map(int, open(sys.argv[3]).read().split())); s = sig(sys.argv[2]); thr = float(sys.argv[4]) if len(sys.argv) > 4 else 14
        d = sum(abs(a - b) for a, b in zip(g, s)) / len(g); print(f"mean abs diff {d:.2f} (limit {thr})"); sys.exit(0 if d <= thr else 1)
    elif c == "redmin":      # redmin <shot.ppm> <min fraction>
        sys.exit(0 if stats(sys.argv[2])[2] > float(sys.argv[3]) else 1)
    elif c == "stats":
        m, bl, rd = stats(sys.argv[2]); print(f"brightness={m:.1f} black={bl:.3f} red={rd:.3f}")

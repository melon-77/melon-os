#!/usr/bin/env python3
"""Draw the frames written by `lua5.1 tools/sim.lua frames DIR` as PNGs (318x212, scaled 2x), to look at without a calculator."""
import sys
from pathlib import Path
from PIL import Image, ImageDraw

SCALE = 2


def render(src: Path) -> Path:
    img = Image.new("RGB", (318 * SCALE, 212 * SCALE), (0, 0, 0))
    d = ImageDraw.Draw(img)
    color = (255, 255, 255)
    for line in src.read_text().splitlines():
        if not line:
            continue
        f = line.split("\t")
        op, a = f[0], f[1:]
        if op == "color":
            color = tuple(int(float(v)) for v in a)
        elif op == "text":
            x, y = float(a[0]), float(a[1])
            d.text((x * SCALE, y * SCALE), a[3], fill=color)
        else:
            n = [float(v) * SCALE for v in a]
            if op == "rect":
                d.rectangle([n[0], n[1], n[0] + max(n[2], 1) - 1, n[1] + max(n[3], 1) - 1], fill=color)
            elif op == "drect":
                d.rectangle([n[0], n[1], n[0] + n[2], n[1] + n[3]], outline=color)
            elif op == "arc":
                if n[2] > 0 and n[3] > 0:
                    d.ellipse([n[0], n[1], n[0] + n[2], n[1] + n[3]], fill=color)
            elif op == "line":
                d.line(n, fill=color, width=SCALE)
            elif op == "poly":
                d.polygon(list(zip(n[0::2], n[1::2])), fill=color)
    out = src.with_suffix(".png")
    img.save(out)
    return out


if __name__ == "__main__":
    for p in sorted(Path(sys.argv[1]).glob("*.txt")):
        print(render(p))

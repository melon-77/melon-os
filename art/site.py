#!/usr/bin/env python3
"""Draws the two pictures the website (site/) is made of.

  site/assets/melon-pixel.svg   the melon from `melonfetch`: the script's own awk drawing is run and each half-block
                                cell turned back into two square pixels, so the site shows the same melon as the terminal
  site/assets/net.svg           a seamless tile of netting (a jittered grid, like the rind), drawn with currentColor

Run from anywhere: python3 art/site.py   (needs awk and python3, nothing else)
"""
import os, re, random, subprocess

here = os.path.dirname(os.path.abspath(__file__))
root = os.path.dirname(here)
out = os.path.join(root, "site", "assets")
os.makedirs(out, exist_ok=True)

# --- the melon ---------------------------------------------------------------------------------------------------
src = open(os.path.join(root, "recipes/melon-base/files/usr/bin/melonfetch")).read()
m = re.search(r"art=\$\(awk -v E=\"\$E\" '(.*?)'\)\n", src, re.S)
awk = m.group(1)
ansi = subprocess.run(["awk", "-v", "E=\x1b", awk], capture_output=True, text=True, check=True).stdout.splitlines()

cell = re.compile(r"\x1b\[0(?:;38;2;(\d+;\d+;\d+)(?:;48;2;(\d+;\d+;\d+))?)?m(.)")
rows = []
for line in ansi:
    top, bot = [], []
    for fg, bg, ch in cell.findall(line):
        hexa = lambda c: "#%02x%02x%02x" % tuple(int(v) for v in c.split(";")) if c else None
        fg, bg = hexa(fg), hexa(bg)
        if ch == "▀":   top.append(fg); bot.append(bg)       # upper half block
        elif ch == "▄": top.append(None); bot.append(fg)     # lower half block
        else:                top.append(None); bot.append(None)
    rows += [top, bot]
H, W = len(rows), max(len(r) for r in rows)
rects = []
for y, r in enumerate(rows):                                       # merge runs of one colour in a row
    x = 0
    while x < len(r):
        c = r[x]
        if c is None: x += 1; continue
        e = x
        while e + 1 < len(r) and r[e + 1] == c: e += 1
        rects.append('<rect x="%d" y="%d" width="%d" height="1" fill="%s"/>' % (x, y, e - x + 1, c))
        x = e + 1
svg = ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" shape-rendering="crispEdges">%s</svg>\n'
       % (W, H, "".join(rects)))
open(os.path.join(out, "melon-pixel.svg"), "w").write(svg)

# --- the netting -------------------------------------------------------------------------------------------------
random.seed(77)
N, S = 9, 120                      # 9x9 cells of 120 px: a 1080 px tile
pts = {(i, j): ((i % N) * S + random.uniform(-0.34, 0.34) * S, (j % N) * S + random.uniform(-0.34, 0.34) * S)
       for i in range(N) for j in range(N)}
def p(i, j):                        # wrap around: the tile repeats without a seam
    x, y = pts[(i % N, j % N)]
    return x + (i // N) * N * S, y + (j // N) * N * S
paths = []
for j in range(N):
    for i in range(-1, N + 1):
        a, b = p(i, j), p(i + 1, j)
        paths.append("M%.0f %.0fL%.0f %.0f" % (a + b))
for i in range(N):
    for j in range(-1, N + 1):
        a, b = p(i, j), p(i, j + 1)
        paths.append("M%.0f %.0fL%.0f %.0f" % (a + b))
net = ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" width="%d" height="%d">'
       '<path d="%s" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round"/></svg>\n'
       % (N * S, N * S, N * S, N * S, "".join(paths)))
open(os.path.join(out, "net.svg"), "w").write(net)
print("melon-pixel.svg: %dx%d, %d rects; net.svg: %d bytes" % (W, H, len(rects), len(net)))

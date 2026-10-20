#!/usr/bin/env python3
"""The installer's icon (12 October 2026, the calmer look): melon's plain logo with a small accent-green badge holding
a down arrow ("install"). Replaces the smiling mascot.
  installer-icon.py <logo-512.png> <out-dir>   -> <size>x<size>/apps/melon-installer.png for 32, 48, 64, 128, 256"""
import os, sys
from PIL import Image, ImageDraw

LOGO, OUT = sys.argv[1:3]
S = 1024
im = Image.new('RGBA', (S, S), (0, 0, 0, 0))
logo = Image.open(LOGO).convert('RGBA').resize((S, S), Image.LANCZOS)
im.alpha_composite(logo)
d = ImageDraw.Draw(im)
cx, cy, r = int(S * 0.76), int(S * 0.76), int(S * 0.2)
d.ellipse((cx - r - 24, cy - r - 24, cx + r + 24, cy + r + 24), fill=(18, 23, 20, 255))      # a dark ring separates it
d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(111, 191, 74, 255))
w = int(r * 0.22)
d.line((cx, cy - r * 0.55, cx, cy + r * 0.35), fill=(11, 21, 12, 255), width=w)
d.polygon([(cx - r * 0.5, cy + r * 0.0), (cx + r * 0.5, cy + r * 0.0), (cx, cy + r * 0.58)], fill=(11, 21, 12, 255))
for s in (32, 48, 64, 128, 256):
    os.makedirs(f'{OUT}/{s}x{s}/apps', exist_ok=True)
    im.resize((s, s), Image.LANCZOS).save(f'{OUT}/{s}x{s}/apps/melon-installer.png', optimize=True)

#!/usr/bin/env python3
"""melon's default wallpaper (12 October 2026, the calmer look): a dark green-grey field with one soft glow of the
melon accent in a corner. No planet, no logo, no text: the branding is in the boot menu and the accent colours.
  wallpaper.py <out-dir>   -> 3840x2160.jpg, 2560x1440.jpg, 1920x1080.jpg, screenshot.jpg (the folder Plasma and
                              pcmanfm-qt read: usr/share/wallpapers/melon/contents/images, screenshot one level up)
Needs only numpy and Pillow (no fonts)."""
import os, sys
import numpy as np
from PIL import Image

OUT = sys.argv[1]; os.makedirs(OUT, exist_ok=True)
W, H = 3840, 2160
y, x = np.mgrid[0:H, 0:W].astype(np.float32)
u, v = x / W, y / H
# the field: #0c100e at the top left to #131a16 at the bottom right
top, bottom = np.array([12, 16, 14], np.float32), np.array([19, 26, 22], np.float32)
t = np.clip(0.55 * u + 0.45 * v, 0, 1)[..., None]
img = top * (1 - t) + bottom * t
# one wide, faint glow of the accent (#6fbf4a), mostly off the bottom right of the screen
d = np.sqrt(((u - 1.05) / 0.75) ** 2 + ((v - 1.15) / 0.75) ** 2)
glow = np.exp(-d ** 2 * 2.2)[..., None]
img = img + glow * (np.array([111, 191, 74], np.float32) - img) * 0.16
# a little film grain so the gradient doesn't band on 8-bit screens and in JPEG
rng = np.random.default_rng(12)
img = img + rng.normal(0, 0.9, (H, W, 1)).astype(np.float32)
im = Image.fromarray(np.clip(img + 0.5, 0, 255).astype(np.uint8), 'RGB')
for w, h in ((3840, 2160), (2560, 1440), (1920, 1080)):
    im.resize((w, h), Image.LANCZOS).save(f'{OUT}/{w}x{h}.jpg', quality=90, optimize=True, progressive=True)
im.resize((400, 225), Image.LANCZOS).save(f'{OUT}/screenshot.jpg', quality=88)

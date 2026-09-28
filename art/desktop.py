#!/usr/bin/env python3
"""Default desktop wallpaper and splash background: the melon planet, no logo and no text.
  desktop.py <out-dir>   -> wallpaper/<3840x2160,2560x1440,1920x1080>.jpg, screenshot.jpg, splash.jpg"""
import sys, os
from PIL import Image
import fx, planet

OUT = sys.argv[1]; os.makedirs(OUT, exist_ok=True)
F = fx.Fonts('fonts/x/usr/share/fonts/truetype/noto')
img = planet.planet(F, None, wordmark=False, cxf=0.92, cyf=1.32, rf=0.58, seeds=70, dim=0.9).convert('RGB')   # the GRUB composition: the planet peeks in at the bottom right
for w, h in ((3840, 2160), (2560, 1440), (1920, 1080)):
    img.resize((w, h), Image.LANCZOS).save(f'{OUT}/{w}x{h}.jpg', quality=92, optimize=True, progressive=True)
img.resize((400, 225), Image.LANCZOS).save(f'{OUT}/screenshot.jpg', quality=88)
img.resize((1920, 1080), Image.LANCZOS).save(f'{OUT}/splash.jpg', quality=90, optimize=True)

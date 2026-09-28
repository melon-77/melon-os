#!/usr/bin/env python3
"""GRUB backgrounds (1920x1080 PNG): the planet for everyone, the portal for gauntlet survivors.
GRUB draws its menu in light gray straight over the image, down the left and along the top, with help text
at the bottom, so both keep the left and the edges calm and put the drama on the right.
  grub.py <out-dir> <logo.png>"""
import sys
import numpy as np
from PIL import Image
import fx, planet, protos2

OUT, LOGO = sys.argv[1:3]
F = fx.Fonts('fonts/x/usr/share/fonts/truetype/noto')

def mark(img):
    """A small melon mark, top right, clear of the menu."""
    logo = Image.open(LOGO).convert('RGBA'); ls = int(0.065 * fx.H); logo = logo.resize((ls, ls), Image.LANCZOS)
    f = F('NotoSerifDisplay-Regular', 0.036 * fx.H); tw = f.getlength('melon')
    x = int(0.975 * fx.W - tw - ls - 22); y = int(0.955 * fx.H - ls / 2)   # bottom right, under GRUB's box
    img.paste(logo, (x, y), logo)
    fx.gradient_text(img, (x + ls + 22 + tw / 2, y + ls / 2), 'melon', f, (226, 236, 214), (150, 190, 130))

def save(img, name):
    img = img.resize((1920, 1080), Image.LANCZOS)
    img.save(f'{OUT}/{name}', optimize=True)          # 8-bit RGB, not interlaced: what GRUB's png module reads

import os; os.makedirs(OUT, exist_ok=True)
g = planet.planet(F, LOGO, wordmark=False, cxf=0.92, cyf=1.32, rf=0.58, seeds=70, dim=0.9)   # clear of the help text
mark(g); save(g, 'background.png')
s = protos2.portal(cxf=0.74, cyf=0.47, text='you went through the gauntlet.', text_at='grub', calm_left=0.75)
save(s, 'background-survivor.png')

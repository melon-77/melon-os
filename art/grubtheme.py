#!/usr/bin/env python3
"""The GRUB themes: melon (everyone) and melon-gold (gauntlet survivors). Each is a directory with theme.txt,
the background from grub.py, the menu panel and highlight as 9-slice PNGs, and the fonts as GRUB .pf2 files.
  grubtheme.py <themes-dir> <rewards-themes-dir> <background.png> <background-survivor.png> <grub-mkfont>
Needs grub-mkfont (grub-common) and the Noto/DejaVu fonts in fonts/x (see grub.py)."""
import os, subprocess, sys
from PIL import Image, ImageDraw

THEMES, REWARDS, BG, BG_GOLD, MKFONT = sys.argv[1:6]
NOTO = 'fonts/x/usr/share/fonts/truetype/noto'
MONO = '/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf'
# name in theme.txt -> (ttf, pixel size); only the characters a boot menu needs (ASCII, Latin-1, Latin Extended-A)
FONTS = {'Noto Sans Regular 24': (f'{NOTO}/NotoSans-Regular.ttf', 24),
         'Noto Sans Bold 24': (f'{NOTO}/NotoSans-Bold.ttf', 24),
         'Noto Sans Regular 18': (f'{NOTO}/NotoSans-Regular.ttf', 18),
         'Noto Sans Regular 16': (f'{NOTO}/NotoSans-Regular.ttf', 16),
         'DejaVu Sans Mono Regular 16': (MONO, 16)}

THEME = '''# melon's boot menu{extra_comment}
title-text: ""
desktop-image: "background.png"
desktop-color: "#0a0e0c"
terminal-font: "DejaVu Sans Mono Regular 16"
terminal-box: "menu_*.png"

+ boot_menu {{
  left = 7%
  top = 28%
  width = 38%
  height = 40%
  item_font = "Noto Sans Regular 24"
  selected_item_font = "Noto Sans Bold 24"
  item_color = "{item}"
  selected_item_color = "{sel_text}"
  item_height = 44
  item_padding = 18
  item_spacing = 16
  icon_width = 0
  icon_height = 0
  item_icon_space = 0
  menu_pixmap_style = "menu_*.png"
  selected_item_pixmap_style = "select_*.png"
  scrollbar = false
}}
+ label {{
  id = "__timeout__"
  left = 7%
  top = 70%
  width = 38%
  height = 28
  text = "Starting in %d seconds"
  font = "Noto Sans Regular 18"
  color = "{dim}"
}}
+ label {{
  left = 7%
  top = 93%
  width = 60%
  height = 24
  text = "Up/Down to choose  -  Enter to start  -  e to edit  -  c for a command line"
  font = "Noto Sans Regular 16"
  color = "{faint}"
}}
'''

def nine(d, prefix, fill, r, border=None):
    """A rounded box cut into the nine pieces GRUB stretches: <prefix>_nw.png ... <prefix>_c.png."""
    s = 4 * (2 * r + 1)     # drawn 4x and scaled down for smooth corners
    im = Image.new('RGBA', (s, s), (0, 0, 0, 0))
    ImageDraw.Draw(im).rounded_rectangle((0, 0, s - 1, s - 1), radius=4 * r, fill=fill,
                                         outline=border, width=4 * 2 if border else 0)
    im = im.resize((2 * r + 1, 2 * r + 1), Image.LANCZOS)
    parts = {'nw': (0, 0, r, r), 'n': (r, 0, r + 1, r), 'ne': (r + 1, 0, 2 * r + 1, r),
             'w': (0, r, r, r + 1), 'c': (r, r, r + 1, r + 1), 'e': (r + 1, r, 2 * r + 1, r + 1),
             'sw': (0, r + 1, r, 2 * r + 1), 's': (r, r + 1, r + 1, 2 * r + 1), 'se': (r + 1, r + 1, 2 * r + 1, 2 * r + 1)}
    for k, box in parts.items():
        im.crop(box).save(f'{d}/{prefix}_{k}.png')

def theme(d, bg, accent, item, sel_text, dim, faint, panel, extra_comment=''):
    os.makedirs(d, exist_ok=True)
    Image.open(bg).convert('RGB').save(f'{d}/background.png', optimize=True)   # 8-bit RGB, not interlaced
    nine(d, 'menu', panel, 14, border=(255, 255, 255, 28))
    nine(d, 'select', accent, 8)
    for name, (ttf, size) in FONTS.items():
        out = f'{d}/{name.lower().replace(" ", "-")}.pf2'
        subprocess.run([MKFONT, '-o', out, '-n', ' '.join(name.split()[:-2]), '-s', str(size),
                        '-r', '0x20-0x7E,0xA0-0x17F,0x2010-0x2027', ttf], check=True)
    with open(f'{d}/theme.txt', 'w') as f:
        f.write(THEME.format(item=item, sel_text=sel_text, dim=dim, faint=faint, extra_comment=extra_comment))

theme(f'{THEMES}/melon', BG, (111, 191, 74, 255), '#d6e8cc', '#0b150c', '#a8c2a8', '#7f9a82', (8, 14, 10, 150))
theme(f'{REWARDS}/melon-gold', BG_GOLD, (232, 182, 74, 255), '#f2e6c8', '#1a1204', '#c9b07a', '#9a8a66', (10, 8, 4, 165),
      extra_comment=', survivor edition (melon-update-grub picks it for gauntlet survivors)')

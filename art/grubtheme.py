#!/usr/bin/env python3
"""The GRUB themes: melon (everyone: melon-base, live ISOs and installed systems, BIOS and UEFI) and melon-gold (gauntlet
survivors). The calm design of 12 October 2026: no background picture, one dark colour, a small melon above a centred
menu, the selected entry in a thin accent outline, and one line of help at the bottom.
  grubtheme.py <themes-dir> <rewards-themes-dir> <logo.png> <grub-mkfont | directory with the .pf2 files>
Writes <themes-dir>/melon and <rewards-themes-dir>/melon-gold: theme.txt, logo.png, the 9-slice PNGs of the selected
entry (select_*.png) and of the command line's box (term_*.png), and the fonts as GRUB .pf2 files. With grub-mkfont
(grub-common) and the Noto/DejaVu fonts in fonts/x (see grub.py) the fonts are made fresh; given a directory, the .pf2
files already made are copied (they are in the repository: recipes/melon-base/files/usr/share/grub/themes/melon)."""
import os, shutil, subprocess, sys
from PIL import Image, ImageDraw

THEMES, REWARDS, LOGO, FONTSRC = sys.argv[1:5]
NOTO = 'fonts/x/usr/share/fonts/truetype/noto'
if not os.path.isdir(NOTO): NOTO = '/usr/share/fonts/truetype/noto'   # Ubuntu's fonts-noto-core
MONO = '/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf'
# name in theme.txt -> (ttf, pixel size); only the characters a boot menu needs (ASCII, Latin-1, Latin Extended-A)
FONTS = {'Noto Sans Regular 20': (f'{NOTO}/NotoSans-Regular.ttf', 20),
         'Noto Sans Regular 16': (f'{NOTO}/NotoSans-Regular.ttf', 16),
         'DejaVu Sans Mono Regular 16': (MONO, 16)}

# GRUB's theme positions take a percentage plus or minus pixels: everything is centred, so it reads the same at 1024x768
# (BIOS, VBE) and at a 4K panel's native mode (UEFI, GOP)
THEME = '''# melon's boot menu{extra_comment}
title-text: ""
desktop-color: "{bg}"
terminal-font: "DejaVu Sans Mono Regular 16"
terminal-box: "term_*.png"

+ image {{
  left = 50%-24
  top = 24%-24
  width = 48
  height = 48
  file = "logo.png"
}}
+ boot_menu {{
  left = 20%
  top = 32%
  width = 60%
  height = 46%
  item_font = "Noto Sans Regular 20"
  selected_item_font = "Noto Sans Regular 20"
  item_color = "{item}"
  selected_item_color = "{sel_text}"
  item_height = 40
  item_padding = 18
  item_spacing = 6
  icon_width = 0
  icon_height = 0
  item_icon_space = 0
  selected_item_pixmap_style = "select_*.png"
  scrollbar = false
}}
+ label {{
  id = "__timeout__"
  left = 20%
  top = 80%
  width = 60%
  height = 24
  align = "center"
  text = "Starting in %d s"
  font = "Noto Sans Regular 16"
  color = "{faint}"
}}
+ label {{
  left = 0
  top = 100%-56
  width = 100%
  height = 24
  align = "center"
  text = "Enter to start   ·   e to edit   ·   c for a command line"
  font = "Noto Sans Regular 16"
  color = "{faint}"
}}
'''

def nine(d, prefix, fill, r, border=None, bw=2):
    """A rounded box cut into the nine pieces GRUB stretches: <prefix>_nw.png ... <prefix>_c.png."""
    s = 4 * (2 * r + 1)     # drawn 4x and scaled down for smooth corners
    im = Image.new('RGBA', (s, s), (0, 0, 0, 0))
    ImageDraw.Draw(im).rounded_rectangle((0, 0, s - 1, s - 1), radius=4 * r, fill=fill,
                                         outline=border, width=4 * bw if border else 0)
    im = im.resize((2 * r + 1, 2 * r + 1), Image.LANCZOS)
    parts = {'nw': (0, 0, r, r), 'n': (r, 0, r + 1, r), 'ne': (r + 1, 0, 2 * r + 1, r),
             'w': (0, r, r, r + 1), 'c': (r, r, r + 1, r + 1), 'e': (r + 1, r, 2 * r + 1, r + 1),
             'sw': (0, r + 1, r, 2 * r + 1), 's': (r, r + 1, r + 1, 2 * r + 1), 'se': (r + 1, r + 1, 2 * r + 1, 2 * r + 1)}
    for k, box in parts.items():
        im.crop(box).save(f'{d}/{prefix}_{k}.png', optimize=True)   # 8-bit RGBA, not interlaced: what GRUB's png reads

def theme(d, bg, accent, item, sel_text, faint, sel_fill, extra_comment=''):
    if os.path.isdir(d): shutil.rmtree(d)
    os.makedirs(d)
    Image.open(LOGO).convert('RGBA').resize((48, 48), Image.LANCZOS).save(f'{d}/logo.png', optimize=True)
    nine(d, 'select', sel_fill, 8, border=accent, bw=2)
    nine(d, 'term', (20, 25, 22, 255), 6, border=(255, 255, 255, 30), bw=1)
    for name, (ttf, size) in FONTS.items():
        pf2 = f'{name.lower().replace(" ", "-")}.pf2'
        if os.path.isdir(FONTSRC):
            shutil.copy(f'{FONTSRC}/{pf2}', f'{d}/{pf2}')
        else:
            subprocess.run([FONTSRC, '-o', f'{d}/{pf2}', '-n', ' '.join(name.split()[:-2]), '-s', str(size),
                            '-r', '0x20-0x7E,0xA0-0x17F,0x2010-0x2027', ttf], check=True)
    with open(f'{d}/theme.txt', 'w') as f:
        f.write(THEME.format(bg=bg, item=item, sel_text=sel_text, faint=faint, extra_comment=extra_comment))

theme(f'{THEMES}/melon', '#121714', (111, 191, 74, 255), '#b9c4b5', '#f2f6ef', '#6f7d6c', (27, 36, 30, 255))
theme(f'{REWARDS}/melon-gold', '#14120c', (232, 182, 74, 255), '#c9bfa8', '#fbf4e2', '#7d745f', (36, 31, 20, 255),
      extra_comment=', survivor edition (melon-update-grub picks it for gauntlet survivors)')

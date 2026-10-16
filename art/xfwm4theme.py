#!/usr/bin/env python3
"""melon's window frames for XFCE (xfwm4 theme "melon", docs/xfce-theme.md).
  xfwm4theme.py <dir>        writes themerc and the PNG pieces into <dir> (recipes/melon-xfce/files/usr/share/themes/melon/xfwm4)
A title bar in the header colour, so it joins the toolbar under it; rounded top corners; a thin light outline; the title
centred; three small round buttons at the right (minimise, maximise, close), grey, the close one red under the pointer.
xfwm4 reads PNG pieces with alpha, which the rounded corners need; its compositor (on in melon-xfce) draws them."""
import os, sys
from PIL import Image, ImageDraw

OUT = sys.argv[1]
os.makedirs(OUT, exist_ok=True)
S = 8                       # drawn 8 times larger, then scaled down: smooth curves and diagonal lines
H = 36                      # title bar height
R = 10                      # corner radius
B = 3                       # side and bottom border
HEAD = {'active': (32, 40, 35, 255), 'inactive': (27, 34, 29, 255)}      # #202823, a step darker when unfocused
WIN = {'active': (27, 34, 29, 255), 'inactive': (24, 30, 26, 255)}       # #1b221d: the window under the frame
LINE = {'active': (255, 255, 255, 20), 'inactive': (255, 255, 255, 12)}  # the outline
TEXT = {'active': '#e6ece3', 'inactive': '#7d877b'}

def save(img, name):
    img.resize((img.width // S, img.height // S), Image.LANCZOS).save(f'{OUT}/{name}.png', optimize=True)

def corner(state, right):
    w = R + 1
    im = Image.new('RGBA', (w * S, H * S), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    # a rounded rectangle wider than the piece, cut at the piece's edge: the outline, then the header inside it
    box = (0, 0, (w + R) * S, (H + R) * S) if not right else (-R * S, 0, w * S - 1, (H + R) * S)
    d.rounded_rectangle(box, radius=R * S, fill=LINE[state])
    inner = (box[0] + S, box[1] + S, box[2] - S, box[3]) if not right else (box[0] + S, box[1] + S, box[2] - S, box[3])
    d.rounded_rectangle(inner, radius=(R - 1) * S, fill=HEAD[state])
    save(im, f'top-{"right" if right else "left"}-{state}')

def title(state):
    im = Image.new('RGBA', (S, H * S), HEAD[state]); d = ImageDraw.Draw(im)
    d.rectangle((0, 0, S, S - 1), fill=LINE[state])
    im = Image.alpha_composite(Image.new('RGBA', im.size, HEAD[state]), im)
    for i in range(1, 6): save(im, f'title-{i}-{state}')

def side(state, name):
    vertical = name in ('left', 'right')
    im = Image.new('RGBA', ((B * S, S) if vertical else (S, B * S)), WIN[state]); d = ImageDraw.Draw(im)
    if name == 'left': d.rectangle((0, 0, S - 1, S), fill=LINE[state])
    if name == 'right': d.rectangle((B * S - S, 0, B * S, S), fill=LINE[state])
    if name == 'bottom': d.rectangle((0, B * S - S, S, B * S), fill=LINE[state])
    im = Image.alpha_composite(Image.new('RGBA', im.size, WIN[state]), im)
    save(im, f'{name}-{state}')

def bottom_corner(state, right):
    im = Image.new('RGBA', (B * S, B * S), WIN[state]); d = ImageDraw.Draw(im)
    d.rectangle((0, B * S - S, B * S, B * S), fill=LINE[state])
    d.rectangle(((B - 1) * S, 0, B * S, B * S) if right else (0, 0, S - 1, B * S), fill=LINE[state])
    im = Image.alpha_composite(Image.new('RGBA', im.size, WIN[state]), im)
    save(im, f'bottom-{"right" if right else "left"}-{state}')

BW = 28                     # a button's piece: 28 px wide, as tall as the title bar
CIRCLE = {'active': (255, 255, 255, 34), 'prelight': (255, 255, 255, 54), 'pressed': (255, 255, 255, 74), 'inactive': (255, 255, 255, 16)}
GLYPH = {'active': (201, 210, 198, 255), 'prelight': (255, 255, 255, 255), 'pressed': (255, 255, 255, 255),
         'inactive': (139, 149, 137, 255)}
RED = {'prelight': (224, 93, 79, 255), 'pressed': (196, 72, 59, 255)}   # #e05d4f, darker when pressed

def button(kind, state):
    # opaque: the header colour with the circle and the glyph blended onto it (xfwm4 turns partial alpha into a 1-bit mask
    # when the compositor is off, which would cut the circle out)
    bg = HEAD['inactive' if state == 'inactive' else 'active']
    im = Image.new('RGBA', (BW * S, H * S), bg)
    top = Image.new('RGBA', (BW * S, S), LINE['inactive' if state == 'inactive' else 'active'])
    im.alpha_composite(top)                              # the outline along the top, as on the title pieces
    cx, cy, r = BW * S / 2, H * S / 2, 10 * S
    fill = RED.get(state) if kind == 'close' else None
    fill = fill or CIRCLE[state]
    if fill:
        layer = Image.new('RGBA', im.size, (0, 0, 0, 0))
        ImageDraw.Draw(layer).ellipse((cx - r, cy - r, cx + r, cy + r), fill=fill)
        im.alpha_composite(layer)
    layer = Image.new('RGBA', im.size, (0, 0, 0, 0)); d = ImageDraw.Draw(layer)
    g, w, a = GLYPH[state], int(1.75 * S), int(4.5 * S)         # colour, line width, half the glyph's size
    if kind == 'close':
        d.line((cx - a, cy - a, cx + a, cy + a), fill=g, width=w); d.line((cx - a, cy + a, cx + a, cy - a), fill=g, width=w)
    elif kind == 'hide':
        d.line((cx - a, cy, cx + a, cy), fill=g, width=w)
    elif kind == 'maximize':
        d.rectangle((cx - a, cy - a, cx + a, cy + a), outline=g, width=w)
    elif kind == 'maximize-toggled':     # two overlapping squares: "restore"
        o = int(1.5 * S)
        d.rectangle((cx - a + o, cy - a - o + S, cx + a + o - S, cy + a - o), outline=g, width=w)
        d.rectangle((cx - a, cy - a + o, cx + a - o, cy + a), fill=(0, 0, 0, 0), outline=g, width=w)
    im.alpha_composite(layer)
    save(im, f'{kind}-{state}')

for st in ('active', 'inactive'):
    corner(st, False); corner(st, True); title(st)
    for n in ('left', 'right', 'bottom'): side(st, n)
    bottom_corner(st, False); bottom_corner(st, True)
for k in ('close', 'hide', 'maximize', 'maximize-toggled'):
    for st in ('active', 'inactive', 'prelight', 'pressed'): button(k, st)

open(f'{OUT}/themerc', 'w').write(f'''# melon's window frames (art/xfwm4theme.py)
button_offset=6
button_spacing=0
full_width_title=true
title_alignment=center
title_horizontal_offset=0
title_vertical_offset_active=0
title_vertical_offset_inactive=0
title_shadow_active=false
title_shadow_inactive=false
active_text_color={TEXT['active']}
inactive_text_color={TEXT['inactive']}
show_app_icon=false
shadow_delta_height=2
shadow_delta_width=0
shadow_delta_x=0
shadow_delta_y=-4
shadow_opacity=55
''')

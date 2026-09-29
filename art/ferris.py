#!/usr/bin/env python3
"""The rust-free reward: "farewell, Ferris". Ferris the crab (Rust's unofficial mascot, which its author put in the
public domain) in a hospital bed at night, surrounded by family and friends, a melon among them. Drawn as SVG on a
1920x1080 canvas, rendered with cairosvg, finished with the fx studio's lens and a corner line like the portal's.
  ferris.py <out-dir> [font-dir]            -> <out-dir>/melon-farewell-ferris/{metadata.json,contents/...}
                                               and <out-dir>/melon-farewell-ferris.png (3840x2160, lossless)
  ferris.py --preview <file.png> [width]    -> just the drawing, no lens or caption
Needs Pillow, numpy and cairosvg; the font dir holds NotoSerifDisplay-Italic.ttf."""
import itertools, json, math, os, random, sys

CAPTION = 'you removed rust. ferris is surrounded by family now.'
FERRIS = '#f74c00'

_ids = itertools.count()
DEFS, OUT = [], []

def uid(p): return f'{p}{next(_ids)}'
def add(s): OUT.append(s)
def rgb(h): h = h.lstrip('#'); return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))
def hexc(c): return '#%02x%02x%02x' % tuple(max(0, min(255, int(round(v)))) for v in c)
def shade(h, f):
    """f < 1 darkens towards black, f > 1 lightens towards white"""
    r = rgb(h)
    return hexc([v * f for v in r]) if f <= 1 else hexc([v + (255 - v) * (f - 1) for v in r])
def num(v): return f'{v:.2f}'.rstrip('0').rstrip('.') if isinstance(v, float) else str(v)
def el(tag, **a):
    add(f'<{tag} ' + ' '.join(f'{k.rstrip("_").replace("_", "-")}="{num(v)}"' for k, v in a.items()) + '/>')
def P(pts, close=True): return 'M ' + ' L '.join(f'{x:.1f} {y:.1f}' for x, y in pts) + (' Z' if close else '')
def _stops(stops):
    return ''.join(f'<stop offset="{o}" stop-color="{c}" stop-opacity="{a}"/>' for o, c, a in stops)
def lin(stops, x1=0, y1=0, x2=0, y2=1, user=False):
    i = uid('l'); u = ' gradientUnits="userSpaceOnUse"' if user else ''
    DEFS.append(f'<linearGradient id="{i}" x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}"{u}>{_stops(stops)}</linearGradient>')
    return f'url(#{i})'
def rad(stops, cx=.5, cy=.5, r=.5, fx=None, fy=None, user=False):
    i = uid('r'); u = ' gradientUnits="userSpaceOnUse"' if user else ''
    f = f' fx="{fx}" fy="{fy}"' if fx is not None else ''
    DEFS.append(f'<radialGradient id="{i}" cx="{cx}" cy="{cy}" r="{r}"{f}{u}>{_stops(stops)}</radialGradient>')
    return f'url(#{i})'
def heart(x, y, r, col, op=1.0):
    add(f'<path d="M {x} {y + r} C {x - 0.75 * r} {y + 0.45 * r}, {x - 1.3 * r} {y - 0.1 * r}, {x - 1.2 * r} {y - 0.6 * r} '
        f'C {x - 1.1 * r} {y - 1.2 * r}, {x - 0.25 * r} {y - 1.25 * r}, {x} {y - 0.6 * r} '
        f'C {x + 0.25 * r} {y - 1.25 * r}, {x + 1.1 * r} {y - 1.2 * r}, {x + 1.2 * r} {y - 0.6 * r} '
        f'C {x + 1.3 * r} {y - 0.1 * r}, {x + 0.75 * r} {y + 0.45 * r}, {x} {y + r} Z" fill="{col}" fill-opacity="{op}"/>')

def glow(cx, cy, rx, ry, col, a):
    el('ellipse', cx=cx, cy=cy, rx=rx, ry=ry, fill=rad([(0, col, a), (0.5, col, a * 0.35), (1, col, 0)]))
def group(cx, cy, s=1.0, rot=0): add(f'<g transform="translate({cx:.1f} {cy:.1f}) rotate({rot}) scale({s})">')
def end(): add('</g>')

# ------------------------------------------------------------------ crabs
def body_pts(w=100, top=64, bottom=44, spikes=7, sp=12):
    """Ferris's outline: a wide shell with a row of rounded spikes along the top"""
    cs = [math.pi * (0.16 + 0.68 * k / (spikes - 1)) for k in range(spikes)]
    pts = []
    for i in range(181):
        th = math.pi * (1 - i / 180)
        b = sum(sp * math.exp(-((th - c) / 0.05) ** 2) for c in cs)
        pts.append(((w + 0.6 * b * abs(math.cos(th))) * math.cos(th), -top * math.sin(th) ** 0.85 - b * math.sin(th)))
    for i in range(1, 180):
        th = -math.pi * i / 180
        pts.append((w * math.cos(th), bottom * math.sin(-th) ** 0.9))
    return pts

def claw(x, y, r, ang, col, mouth=36):
    """a pincer: a disc with a notch, the notch pointing along ang (degrees, SVG orientation)"""
    a, m = math.radians(ang), math.radians(mouth)
    ax, ay = x + 0.2 * r * math.cos(a), y + 0.2 * r * math.sin(a)
    p1 = (x + r * math.cos(a + m), y + r * math.sin(a + m)); p2 = (x + r * math.cos(a - m), y + r * math.sin(a - m))
    d = f'M {ax:.1f} {ay:.1f} L {p1[0]:.1f} {p1[1]:.1f} A {r} {r} 0 1 1 {p2[0]:.1f} {p2[1]:.1f} Z'
    add(f'<path d="{d}" fill="{rad([(0, shade(col, 1.25), 1), (0.6, col, 1), (1, shade(col, 0.62), 1)], .38, .32, .75)}" '
        f'stroke="{shade(col, 0.45)}" stroke-width="2" stroke-opacity="0.6"/>')

def arm(sg, cx, cy, col, w=14):
    sx, sy = sg * 84, 6
    ex, ey = (sx + cx) / 2 + sg * 16, (sy + cy) / 2 + 12
    add(f'<path d="M {sx} {sy} Q {ex:.1f} {ey:.1f} {cx:.1f} {cy:.1f}" fill="none" stroke="{shade(col, 0.78)}" '
        f'stroke-width="{w}" stroke-linecap="round"/>')

def tear(x, y, s=1.0):
    add(f'<path d="M {x} {y - 9 * s} C {x + 6 * s} {y}, {x + 6 * s} {y + 7 * s}, {x} {y + 7 * s} '
        f'C {x - 6 * s} {y + 7 * s}, {x - 6 * s} {y}, {x} {y - 9 * s} Z" fill="#aee0ff" fill-opacity="0.9"/>')

def eyes(look=(0, 0), mood='sad', brow='#3b1a0c', brow_w=5):
    for sg in (-1, 1):
        ex, ey = sg * 30, -20
        el('ellipse', cx=ex, cy=ey, rx=15, ry=17, fill='#fbf7ef')
        px, py = ex + look[0] * 5, ey + look[1] * 5
        el('circle', cx=px, cy=py, r=9, fill='#15100c')
        el('circle', cx=px - 3, cy=py - 3.5, r=2.8, fill='#ffffff')
        if mood == 'sad':   # inner ends up
            add(f'<path d="M {ex + sg * 15} {ey - 22} L {ex - sg * 11} {ey - 30}" stroke="{brow}" '
                f'stroke-width="{brow_w}" stroke-linecap="round"/>')
    if mood == 'sad':
        add('<path d="M -13 27 Q 0 16 13 27" fill="none" stroke="#3b1a0c" stroke-width="4" stroke-linecap="round"/>')
    else:
        add('<path d="M -13 20 Q 0 32 13 20" fill="none" stroke="#3b1a0c" stroke-width="4" stroke-linecap="round"/>')

def crab(cx, base, s, col, look=(0, 0), mood='sad', claws=None, tears=(), before=None, after=None, legs=True):
    """A standing crab: feet on y = base. claws: [(side, x, y, angle, r)] in local units (body is 200 wide)."""
    cy = base - 88 * s
    el('ellipse', cx=cx, cy=base + 2, rx=118 * s, ry=15 * s, fill=rad([(0, '#000000', 0.55), (1, '#000000', 0)]))
    group(cx, cy, s)
    if before: before()
    if legs:
        for sg in (-1, 1):
            for k in range(3):
                pts = [(sg * (40 + 20 * k), 26 + 4 * k), (sg * (86 + 22 * k), 16 + 14 * k), (sg * (96 + 24 * k), 88)]
                add(f'<path d="{P(pts, False)}" fill="none" stroke="{shade(col, 0.62)}" stroke-width="11" '
                    f'stroke-linecap="round" stroke-linejoin="round"/>')
    claws = claws if claws is not None else [(-1, -150, -46, -125, 30), (1, 150, -46, -55, 30)]
    for sg, x, y, a, r in claws: arm(sg, x, y, col)
    add(f'<path d="{P(body_pts())}" fill="{rad([(0, shade(col, 1.3), 1), (0.55, col, 1), (1, shade(col, 0.58), 1)], .4, .28, .8)}" '
        f'stroke="{shade(col, 0.45)}" stroke-width="2.5" stroke-opacity="0.55"/>')
    eyes(look, mood)
    for (x, y) in tears: tear(x, y)
    for sg, x, y, a, r in claws: claw(x, y, r, a, col)
    if after: after()
    end()

# ------------------------------------------------------------------ the room
def wall_and_floor():
    el('rect', x=0, y=0, width=1920, height=1080, fill=lin([(0, '#08110e', 1), (0.8, '#13241d', 1), (1, '#16291f', 1)]))
    # the bumper rail along the wall, the medical gas panel over the bed
    el('rect', x=0, y=688, width=1920, height=16, fill='#1a2e26')
    el('rect', x=0, y=688, width=1920, height=2, fill='#2a443a')
    el('rect', x=800, y=392, width=400, height=34, rx=6, fill='#16271f', stroke='#223a30', stroke_width=2)
    for i, c in enumerate(('#5fbf7a', '#e9efe9', '#5fa8d6', '#e9efe9', '#d6c25f')):
        el('circle', cx=840 + i * 80, cy=409, r=6, fill=c, fill_opacity=0.55)
    # the reading lamp over the bed
    el('rect', x=950, y=338, width=100, height=26, rx=7, fill='#2b3934')
    el('rect', x=956, y=360, width=88, height=5, rx=2, fill='#ffe8c2')
    # floor: tiles in perspective towards the bed
    el('rect', x=0, y=860, width=1920, height=220, fill=lin([(0, '#0d1814', 1), (1, '#060b09', 1)]))
    for y in (878, 902, 934, 978, 1040):
        el('line', x1=0, y1=y, x2=1920, y2=y, stroke='#16241e', stroke_width=2)
    for x0 in range(-1400, 3400, 170):
        el('line', x1=x0, y1=860, x2=1000 + (x0 - 1000) * (1080 - 560) / 300, y2=1080, stroke='#16241e', stroke_width=2)
    el('rect', x=0, y=852, width=1920, height=12, fill='#1b2c25')

def window():
    el('rect', x=1466, y=112, width=338, height=372, rx=4, fill='#1f2f29')
    el('rect', x=1484, y=130, width=302, height=336, fill=lin([(0, '#07172a', 1), (1, '#18394a', 1)]))
    rs = random.Random(7)
    for _ in range(34):
        el('circle', cx=rs.uniform(1490, 1780), cy=rs.uniform(136, 380), r=rs.uniform(0.8, 1.9), fill='#ffffff',
           fill_opacity=rs.uniform(0.35, 0.9))
    glow(1712, 206, 130, 130, '#dbe9ff', 0.28)
    el('circle', cx=1712, cy=206, r=34, fill='#f2ead0')
    el('circle', cx=1727, cy=196, r=31, fill='#0b1c2e')
    # the city asleep
    x, sky = 1484, []
    rs = random.Random(3)
    while x < 1786:
        w = rs.uniform(18, 42); h = rs.uniform(30, 110); sky += [(x, 466 - h), (x + w, 466 - h)]; x += w
    add(f'<path d="{P([(1484, 466)] + sky + [(1786, 466)])}" fill="#08121b"/>')
    for _ in range(40):
        bx, by = rs.uniform(1490, 1780), rs.uniform(390, 460)
        el('rect', x=bx, y=by, width=3, height=4, fill='#f6d27a', fill_opacity=rs.uniform(0.25, 0.8))
    el('rect', x=1631, y=130, width=8, height=336, fill='#1f2f29')
    el('rect', x=1484, y=294, width=302, height=8, fill='#1f2f29')
    el('rect', x=1452, y=480, width=366, height=16, rx=3, fill='#2a3b34')

def curtain():
    el('line', x1=0, y1=66, x2=440, y2=66, stroke='#6f7d77', stroke_width=5)
    xs = list(range(0, 262, 6))
    hem = [(x, 868 + 10 * math.sin(x / 20)) for x in xs]
    edge = [(258 + 6 * math.sin(y / 60), y) for y in range(868, 70, -12)]
    add(f'<path d="{P([(0, 72)] + hem + edge + [(258, 72)])}" '
        f'fill="{lin([(0, "#1d362d", 1), (1, "#2c4d41", 1)], 0, 0, 1, 0)}"/>')
    for i in range(7):   # folds
        x = 18 + i * 36
        add(f'<path d="M {x} 74 C {x + 8} 300, {x - 8} 600, {x + 4} 868" fill="none" stroke="#0d1a15" '
            f'stroke-width="10" stroke-opacity="0.45"/>')
        add(f'<path d="M {x + 16} 74 C {x + 22} 300, {x + 8} 600, {x + 20} 868" fill="none" stroke="#4d7a69" '
            f'stroke-width="6" stroke-opacity="0.22"/>')
    for x in range(12, 262, 30): el('circle', cx=x, cy=70, r=5, fill='#8a9791')

def photo():
    """on the wall: the family at the beach, in happier days"""
    el('rect', x=1250, y=176, width=132, height=108, rx=3, fill='#5a4630')
    el('rect', x=1260, y=186, width=112, height=88, fill='#e9e1cc')
    el('rect', x=1268, y=194, width=96, height=72, fill=lin([(0, '#8fd0ee', 1), (0.55, '#bfe6f3', 1)]))
    el('rect', x=1268, y=230, width=96, height=14, fill='#3a88b8')
    el('rect', x=1268, y=244, width=96, height=22, fill='#e8cf8e')
    el('circle', cx=1348, cy=207, r=6, fill='#ffe07a')
    for x, s, c in ((1290, 0.11, FERRIS), (1312, 0.085, '#e4492b'), (1330, 0.06, '#ff7c35')):
        crab(x, 262, s, c, mood='happy', look=(0, 0), legs=False)

def monitor():
    el('rect', x=662, y=312, width=44, height=14, fill='#2b3833')
    glow(566, 324, 190, 130, '#5ee38a', 0.10)
    el('rect', x=468, y=248, width=198, height=152, rx=12, fill='#25302c', stroke='#35433d', stroke_width=2)
    el('rect', x=480, y=260, width=174, height=128, rx=4, fill='#03100a')
    for y in range(272, 388, 16): el('line', x1=480, y1=y, x2=654, y2=y, stroke='#0b2718', stroke_width=1)
    # the beats fade into a line
    base, pts, x = 330, [(484, 330)], 484
    for bx, a in ((506, 46), (552, 32), (592, 19), (624, 8)):
        pts += [(bx - 12, base), (bx - 8, base - 0.12 * a), (bx - 5, base), (bx - 2, base + 0.14 * a), (bx, base - a),
                (bx + 3, base + 0.3 * a), (bx + 6, base), (bx + 12, base - 0.16 * a), (bx + 18, base)]
    pts += [(650, base)]
    d = P(pts, False)
    add(f'<path d="{d}" fill="none" stroke="#62e38c" stroke-width="7" stroke-opacity="0.18" stroke-linejoin="round"/>')
    add(f'<path d="{d}" fill="none" stroke="#7cf0a0" stroke-width="2.2" stroke-linejoin="round"/>')
    add('<text x="488" y="282" font-family="Noto Sans" font-size="13" fill="#62e38c" fill-opacity="0.8">HR</text>')
    add('<text x="646" y="290" font-family="Noto Sans" font-weight="bold" font-size="24" fill="#7cf0a0" '
        'text-anchor="end">12</text>')
    add('<text x="488" y="380" font-family="Noto Sans" font-size="12" fill="#63cfe0" fill-opacity="0.85">SpO2  81</text>')
    heart(640, 373, 6, '#e36262')

def iv_stand():
    el('line', x1=700, y1=204, x2=700, y2=942, stroke='#8e9a95', stroke_width=5)
    el('line', x1=676, y1=212, x2=724, y2=212, stroke='#8e9a95', stroke_width=4, stroke_linecap='round')
    for dx, dy in ((-40, 16), (40, 16), (-18, 24), (18, 24)):
        el('line', x1=700, y1=942, x2=700 + dx, y2=942 + dy, stroke='#8e9a95', stroke_width=5, stroke_linecap='round')
        el('circle', cx=700 + dx, cy=946 + dy, r=5, fill='#1b1f1e')
    el('rect', x=674, y=222, width=52, height=96, rx=12, fill='#dff3f7', fill_opacity=0.18, stroke='#dff3f7',
       stroke_opacity=0.55, stroke_width=1.5)
    el('rect', x=676, y=262, width=48, height=54, rx=10, fill='#dff3f7', fill_opacity=0.2)
    el('rect', x=684, y=236, width=32, height=18, rx=2, fill='#ffffff', fill_opacity=0.45)
    el('rect', x=694, y=318, width=12, height=24, rx=3, fill='#dff3f7', fill_opacity=0.35)

# ------------------------------------------------------------------ the bed, and Ferris in it
FC, FY, FS = 1000, 658, 1.25    # Ferris: centre and scale

def bed_back():
    el('ellipse', cx=1000, cy=958, rx=440, ry=40, fill=rad([(0, '#000000', 0.6), (1, '#000000', 0)]))
    el('rect', x=722, y=870, width=556, height=46, fill='#1c2522')
    for x in (732, 1268):
        el('rect', x=x - 7, y=870, width=14, height=78, fill='#56625d')
        el('circle', cx=x, cy=952, r=14, fill='#121615'); el('circle', cx=x, cy=952, r=5, fill='#56625d')
    # headboard with its posts
    el('rect', x=776, y=468, width=448, height=190, rx=22, fill=lin([(0, '#8b9893', 1), (1, '#56625d', 1)]))
    el('rect', x=798, y=490, width=404, height=150, rx=14, fill='#9fb0a9', fill_opacity=0.25)
    for x in (764, 1212):
        el('rect', x=x, y=452, width=24, height=250, rx=10, fill=lin([(0, '#a3b0ab', 1), (1, '#5f6b66', 1)]))
    # mattress and sheet
    add(f'<path d="{P([(794, 612), (1206, 612), (1296, 800), (704, 800)])}" '
        f'fill="{lin([(0, "#c9d4cf", 1), (1, "#e6eeea", 1)])}"/>')
    # pillow
    add(f'<path d="M 842 586 C 842 548, 900 544, 1000 548 C 1100 544, 1158 548, 1158 586 C 1164 626, 1110 640, 1000 636 '
        f'C 890 640, 836 626, 842 586 Z" fill="{lin([(0, "#f6f9f7", 1), (1, "#c3cfca", 1)])}"/>')

def ferris_body():
    group(FC, FY, FS)
    add(f'<path d="{P(body_pts())}" fill="{rad([(0, "#ff9a55", 1), (0.5, FERRIS, 1), (1, "#9e3000", 1)], .42, .3, .8)}" '
        f'stroke="#6e2200" stroke-width="2.5" stroke-opacity="0.5"/>')
    for sg in (-1, 1):   # sleepy eyes: lids most of the way down
        ex, ey = sg * 34, -18
        add(f'<path d="M {ex - 14} {ey + 1} Q {ex} {ey + 6} {ex + 14} {ey + 1} A 14 14 0 0 1 {ex - 14} {ey + 1} Z" '
            f'fill="#15100c"/>')
        el('circle', cx=ex - 5, cy=ey + 8, r=2.4, fill='#ffffff', fill_opacity=0.8)
        add(f'<path d="M {ex - 16} {ey + 1} Q {ex} {ey + 6} {ex + 16} {ey + 1}" fill="none" stroke="#6e2200" '
            f'stroke-width="2.8" stroke-linecap="round"/>')
    tear(-44, 6, 0.8)
    add('<path d="M -11 17 Q 0 23 11 17" fill="none" stroke="#4a1600" stroke-width="3.2" stroke-linecap="round"/>')
    end()

def blanket():
    top = [(x, 706 - 9 * math.exp(-((x - 1000) / 140) ** 2)) for x in range(758, 1243, 6)]
    add(f'<path d="{P(top + [(1300, 804), (700, 804)])}" fill="{lin([(0, "#8fb9a9", 1), (1, "#5f8a7b", 1)])}"/>')
    band = top + [(x, y + 16) for x, y in reversed(top)]
    add(f'<path d="{P(band)}" fill="{lin([(0, "#f1f6f3", 1), (1, "#c9d6d0", 1)])}"/>')
    for x0, x1, c in ((820, 760, '#4e7466'), (900, 870, '#4e7466'), (1100, 1140, '#4e7466'), (1180, 1250, '#4e7466')):
        add(f'<path d="M {x0} 728 Q {(x0 + x1) / 2 + 10} 760 {x1} 800" fill="none" stroke="{c}" stroke-width="5" '
            f'stroke-opacity="0.45" stroke-linecap="round"/>')

def ferris_claws():
    group(FC, FY, FS)
    for sg in (-1, 1): arm(sg, sg * 118, 66, FERRIS, 15)
    claw(-118, 66, 29, -40, FERRIS)
    claw(118, 66, 29, -140, FERRIS)
    # the hospital bracelet and the IV's tape
    add('<rect x="84" y="36" width="22" height="9" rx="2" fill="#f4f6f5" transform="rotate(38 95 40)"/>')
    add('<rect x="-104" y="34" width="14" height="12" rx="2" fill="#f4f6f5" fill-opacity="0.9"/>')
    end()

def iv_tube():
    tx, ty = FC - 97 * FS, FY + 40 * FS
    add(f'<path d="M 700 342 C 700 520, {tx - 70:.0f} {ty - 60:.0f}, {tx:.0f} {ty:.0f}" fill="none" stroke="#e4f5f8" '
        f'stroke-opacity="0.55" stroke-width="2.4"/>')

def bed_front():
    for sg in (-1, 1):   # side rails
        x0, x1 = 1000 + sg * 206, 1000 + sg * 282
        for dy in (0, 40):
            el('line', x1=x0, y1=626 + dy, x2=x1, y2=740 + dy, stroke='#a9b4af', stroke_width=7, stroke_linecap='round')
        for t in (0.08, 0.5, 0.92):
            x, y = x0 + (x1 - x0) * t, 626 + 114 * t
            el('line', x1=x, y1=y, x2=x, y2=y + 40, stroke='#8b9792', stroke_width=5)
    el('rect', x=690, y=792, width=620, height=82, rx=14, fill=lin([(0, '#7c8984', 1), (1, '#4d5955', 1)]))
    el('rect', x=698, y=797, width=604, height=4, rx=2, fill='#b6c2bd', fill_opacity=0.6)
    for x in (678, 1298):
        el('rect', x=x, y=774, width=24, height=116, rx=10, fill=lin([(0, '#a3b0ab', 1), (1, '#5f6b66', 1)]))
    # the chart
    el('rect', x=956, y=802, width=88, height=104, rx=5, fill='#9b7a4c')
    el('rect', x=964, y=816, width=72, height=84, fill='#f3efe4')
    el('rect', x=982, y=796, width=36, height=16, rx=3, fill='#c7cecb')
    add('<text x="1000" y="833" font-family="Noto Sans" font-weight="bold" font-size="12" fill="#2d2d2d" '
        'text-anchor="middle">FERRIS</text>')
    for y in (842, 850, 858): el('line', x1=970, y1=y, x2=1030, y2=y, stroke='#9aa0a0', stroke_width=1.5)
    add(f'<path d="{P([(970, 868), (984, 872), (996, 870), (1008, 880), (1020, 886), (1030, 893)], False)}" '
        f'fill="none" stroke="#d24a4a" stroke-width="2"/>')

def baby():
    """the youngest, on the blanket, holding on to Ferris's claw"""
    crab(1196, 790, 0.3, '#ff8a48', look=(-1, -0.2), tears=((-30, 4),),
         claws=[(-1, -150, -10, 180, 30), (1, 140, -40, -60, 30)])

# ------------------------------------------------------------------ everyone else
def table():
    el('rect', x=1322, y=744, width=124, height=14, rx=3, fill='#7a6446')
    for x in (1330, 1428): el('rect', x=x, y=758, width=10, height=172, fill='#5f4e37')
    el('rect', x=1330, y=840, width=108, height=8, fill='#5f4e37')
    # flowers, a card
    for x0, x1, y1 in ((1374, 1352, 650), (1380, 1382, 636), (1386, 1410, 648), (1378, 1366, 628), (1384, 1398, 626)):
        add(f'<path d="M {x0} 740 Q {x0} 700 {x1} {y1}" fill="none" stroke="#5d9a4a" stroke-width="3"/>')
    for x, y, c in ((1352, 650, '#e87aa0'), (1382, 634, '#f2d15c'), (1410, 648, '#f0f0f0'), (1366, 626, '#e87aa0'),
                    (1398, 624, '#f2d15c')):
        for k in range(5):
            a = k * 2 * math.pi / 5
            el('circle', cx=x + 7 * math.cos(a), cy=y + 7 * math.sin(a), r=6, fill=c)
        el('circle', cx=x, cy=y, r=4, fill='#b36a2a')
    el('rect', x=1362, y=690, width=36, height=54, rx=8, fill='#bfe6f0', fill_opacity=0.35, stroke='#dff3f7',
       stroke_opacity=0.5)
    add(f'<path d="{P([(1406, 744), (1420, 712), (1440, 744)])}" fill="#f3efe4"/>')
    heart(1423, 732, 5, '#d24a4a')

def balloon():
    add('<path d="M 1434 752 C 1420 680, 1446 600, 1414 520" fill="none" stroke="#d8d8d8" stroke-width="1.5" '
        'stroke-opacity="0.7"/>')
    group(1410, 478, 1.0, -14)
    add(f'<path d="M 0 40 C -30 18, -58 -4, -52 -30 C -46 -54, -12 -58, 0 -30 C 12 -58, 46 -54, 52 -30 '
        f'C 58 -4, 30 18, 0 40 Z" fill="{rad([(0, "#f07a84", 1), (0.6, "#d8424f", 1), (1, "#8f2530", 1)], .35, .3, .8)}"/>')
    add('<path d="M -30 -20 Q -10 -6 8 -24" fill="none" stroke="#8f2530" stroke-width="2" stroke-opacity="0.6"/>')
    add('<text x="0" y="-2" font-family="Noto Sans" font-weight="bold" font-size="14" fill="#fff4f4" '
        'text-anchor="middle">get well</text>')
    end()

def grandpa():
    def glasses():
        for sg in (-1, 1): el('circle', cx=sg * 30, cy=-20, r=20, fill='none', stroke='#d9d2b4', stroke_width=3.5)
        el('line', x1=-10, y1=-22, x2=10, y2=-22, stroke='#d9d2b4', stroke_width=3.5)
    def cane():
        add('<path d="M -176 90 L -166 6 Q -162 -14 -144 -10" fill="none" stroke="#6b4a2a" stroke-width="9" '
            'stroke-linecap="round"/>')
    crab(334, 996, 0.95, '#c3643e', look=(1, -0.4), before=None,
         claws=[(-1, -150, -4, 200, 28), (1, 140, -30, -40, 28)], tears=((42, 4),),
         after=lambda: (glasses(), cane(), [add(f'<path d="M {sg * 44} -46 L {sg * 16} -52" stroke="#eeeeee" '
                                               f'stroke-width="8" stroke-linecap="round"/>') for sg in (-1, 1)]))

def partner():
    def hanky():
        add(f'<path d="{P([(-78, -52), (-38, -60), (-24, -14), (-66, -4)])}" fill="#f4f1ea" stroke="#c9c4b8" '
            f'stroke-width="1.5"/>')
    crab(566, 986, 1.0, '#e0482c', look=(1, -0.5), tears=((40, 2), (46, 22)),
         claws=[(-1, -64, -30, -30, 28), (1, 150, -58, -15, 30)], after=hanky)

def kid():
    def drawing():
        el('rect', x=-60, y=-8, width=120, height=86, rx=3, fill='#fbfaf5', stroke='#d8d4c8', stroke_width=2)
        add('<path d="M 0 60 C -26 40, -40 26, -32 12 C -24 0, -8 4, 0 18 C 8 4, 24 0, 32 12 C 40 26, 26 40, 0 60 Z" '
            'fill="#e2453a" fill-opacity="0.85"/>')
        add('<path d="M -48 70 L -40 56 L -30 70 M 34 70 L 42 58 L 50 70" fill="none" stroke="#f74c00" '
            'stroke-width="4" stroke-linecap="round"/>')
    crab(458, 1022, 0.55, '#ff7a33', look=(0.8, -0.8), tears=((-34, 4), (34, 4)),
         claws=[(-1, -70, 10, 20, 30), (1, 70, 10, 160, 30)], after=drawing)

def melon_friend():
    cx, cy, r = 1508, 890, 74
    el('ellipse', cx=cx, cy=968, rx=100, ry=13, fill=rad([(0, '#000000', 0.55), (1, '#000000', 0)]))
    for sg in (-1, 1):
        el('line', x1=cx + sg * 26, y1=cy + 60, x2=cx + sg * 32, y2=964, stroke='#2f5a22', stroke_width=10,
           stroke_linecap='round')
    # the arm with the flowers, reaching towards the bed
    el('line', x1=cx - 60, y1=cy + 10, x2=cx - 106, y2=cy - 10, stroke='#3f7a2c', stroke_width=9, stroke_linecap='round')
    for x0, y0 in ((-112, -40), (-126, -28), (-100, -52), (-120, -54)):
        el('line', x1=cx - 106, y1=cy - 10, x2=cx + x0, y2=cy + y0, stroke='#5d9a4a', stroke_width=3)
    for (x, y, c) in ((-112, -40, '#f2d15c'), (-126, -28, '#e87aa0'), (-100, -52, '#e87aa0'), (-120, -54, '#f0f0f0')):
        for k in range(5):
            a = k * 2 * math.pi / 5
            el('circle', cx=cx + x + 6 * math.cos(a), cy=cy + y + 6 * math.sin(a), r=5, fill=c)
    el('line', x1=cx + 62, y1=cy + 16, x2=cx + 90, y2=cy + 44, stroke='#3f7a2c', stroke_width=9, stroke_linecap='round')
    el('circle', cx=cx, cy=cy, r=r, fill=rad([(0, '#a9dc7c', 1), (0.55, '#6fbf4a', 1), (1, '#2f6a22', 1)], .38, .3, .8))
    rs = random.Random(11)
    clip = uid('c')
    DEFS.append(f'<clipPath id="{clip}"><circle cx="{cx}" cy="{cy}" r="{r - 2}"/></clipPath>')
    add(f'<g clip-path="url(#{clip})">')
    for _ in range(46):   # the net, like the logo's
        x, y = rs.uniform(cx - r, cx + r), rs.uniform(cy - r, cy + r)
        add(f'<path d="M {x:.0f} {y:.0f} l {rs.uniform(-16, 16):.0f} {rs.uniform(-16, 16):.0f} l {rs.uniform(-16, 16):.0f} '
            f'{rs.uniform(-16, 16):.0f}" fill="none" stroke="#d9f0b8" stroke-width="1.6" stroke-opacity="0.45"/>')
    for dx in (-40, 0, 40):
        add(f'<path d="M {cx + dx} {cy - r} Q {cx + dx * 1.5} {cy} {cx + dx} {cy + r}" fill="none" stroke="#3c7a2a" '
            f'stroke-width="3" stroke-opacity="0.5"/>')
    end()
    add(f'<path d="M {cx + 4} {cy - r + 2} Q {cx + 8} {cy - r - 20} {cx + 20} {cy - r - 30}" fill="none" stroke="#5c4a2a" '
        f'stroke-width="6" stroke-linecap="round"/>')
    add(f'<path d="M {cx + 18} {cy - r - 28} C {cx + 40} {cy - r - 60}, {cx + 70} {cy - r - 40}, {cx + 64} {cy - r - 22} '
        f'C {cx + 50} {cy - r - 10}, {cx + 30} {cy - r - 16}, {cx + 18} {cy - r - 28} Z" fill="#3f8a34"/>')
    group(cx, cy + 14, 0.9)
    eyes((-1, -0.4), 'sad', brow='#1f3a16')
    tear(-36, 4); tear(36, 4)
    end()

def blue_friend():
    crab(1702, 944, 0.82, '#4a86cf', look=(-1, -0.3), tears=((-40, 2), (-36, 22), (40, 2)),
         claws=[(-1, -44, -26, -150, 28), (1, 150, 30, 40, 28)])

# ------------------------------------------------------------------ light
def light():
    add(f'<path d="{P([(956, 366), (1044, 366), (1250, 780), (750, 780)])}" '
        f'fill="{lin([(0, "#ffdca0", 0.16), (1, "#ffdca0", 0)])}"/>')
    glow(1000, 640, 520, 340, '#ffcf8a', 0.14)
    add(f'<path d="{P([(1484, 466), (1786, 466), (1600, 1080), (1120, 1080)])}" '
        f'fill="{lin([(0, "#bfe0ff", 0.08), (1, "#bfe0ff", 0.01)])}"/>')
    glow(1000, 1000, 520, 60, '#ffcf8a', 0.05)

def svg():
    wall_and_floor(); window(); curtain(); photo(); monitor(); iv_stand()
    bed_back(); ferris_body(); blanket(); ferris_claws(); iv_tube(); baby(); bed_front()
    table(); balloon(); grandpa(); partner(); kid(); melon_friend(); blue_friend(); light()
    return ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1920 1080" width="1920" height="1080">'
            f'<defs>{"".join(DEFS)}</defs>{"".join(OUT)}</svg>')

def render(width, height):
    import io, cairosvg
    from PIL import Image
    png = cairosvg.svg2png(bytestring=svg().encode(), output_width=width, output_height=height)
    return Image.open(io.BytesIO(png)).convert('RGB')

if __name__ == '__main__':
    if sys.argv[1] == '--preview':
        render(int(sys.argv[3]) if len(sys.argv) > 3 else 1920, (int(sys.argv[3]) if len(sys.argv) > 3 else 1920) * 9 // 16)\
            .save(sys.argv[2]); sys.exit(0)
    import numpy as np
    import fx
    from fx import W, H
    out = sys.argv[1]
    F = fx.Fonts(sys.argv[2] if len(sys.argv) > 2 else 'fonts/x/usr/share/fonts/truetype/noto')
    img = fx.lens(render(W, H), np.random.RandomState(5))
    f = F('NotoSerifDisplay-Italic', 0.026 * H)   # the portal's corner line (protos2.corner_line), a little lower
    w = f.getlength(CAPTION) + len(CAPTION) * 0.003 * H
    fx.gradient_text(img, (0.962 * W - w / 2, 0.93 * H), CAPTION, f, (236, 214, 160), (190, 150, 86),
                     spacing=int(0.003 * H), shadow=8)
    fx.save(img, out, 'melon-farewell-ferris')
    img.save(f'{out}/melon-farewell-ferris.png', optimize=True)   # the lossless master, next to the package
    with open(f'{out}/melon-farewell-ferris/metadata.json', 'w') as m:
        json.dump({'KPlugin': {'Id': 'melon-farewell-ferris', 'Name': 'farewell, Ferris', 'Authors': [{'Name': 'melon'}],
                               'License': 'CC-BY-SA-4.0'}}, m)
        m.write('\n')

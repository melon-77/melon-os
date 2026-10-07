#!/usr/bin/env python3
"""melon games: the icon of the launcher for melon's own games. A gamepad grown from cantaloupe rind (the netting of
the melon logo), a stem, tendril and leaf on top, a d-pad cut into the orange flesh and four melon seeds for buttons.
  games.py <out.svg> [full|small] [green|gold]
'small' drops the netting and thickens every line, for 16 to 32 px."""
import sys, math
import numpy as np

CX = 256.0
rng = np.random.default_rng(7)

def f(v): return f'{v:.2f}'.rstrip('0').rstrip('.')

def smooth(pts, closed=True, k=1.0):
    """Catmull-Rom through pts as a cubic Bezier path."""
    n = len(pts); P = [np.array(p, float) for p in pts]
    s = f'M{f(P[0][0])},{f(P[0][1])}'
    for i in (range(n) if closed else range(n - 1)):
        p0 = P[(i - 1) % n] if closed or i > 0 else P[i]
        p1, p2 = P[i], P[(i + 1) % n]
        p3 = P[(i + 2) % n] if closed or i + 2 < n else P[(i + 1) % n]
        c1 = p1 + (p2 - p0) / 6 * k; c2 = p2 - (p3 - p1) / 6 * k
        s += f' C{f(c1[0])},{f(c1[1])} {f(c2[0])},{f(c2[1])} {f(p2[0])},{f(p2[1])}'
    return s + (' Z' if closed else '')

def xf(pts, tx, ty, rot, sc, flip=False):
    c, s = math.cos(math.radians(rot)), math.sin(math.radians(rot))
    out = []
    for x, y in pts:
        if flip: x = -x
        x, y = x * sc, y * sc
        out.append((tx + x * c - y * s, ty + x * s + y * c))
    return out

PAL = {
    'green': dict(line='#0f2513', rind=('#8bca5e', '#5a9d45', '#2f6431'), net='#dcefb8', shade='#0b2210',
                  leaf=('#1d4a24', '#4b9a3f'), vein='#8ccf68', stem='#7a9a44', tendril='#6fbf4a',
                  flesh=('#f7c47a', '#f29a4c'), seed=('#fbecc6', '#b9772e'), pill='#2d5e2c'),
    'gold':  dict(line='#2e2106', rind=('#f6d470', '#c8922c', '#6e4c10'), net='#fbe6a8', shade='#2a1c03',
                  leaf=('#7a5410', '#d9a93c'), vein='#ffe08a', stem='#c79a3a', tendril='#f0c75a',
                  flesh=('#f7c47a', '#f29a4c'), seed=('#fff8e6', '#a8741f'), pill='#7a5a16'),
}

# ---------------------------------------------------------------- the gamepad: a wide body with two grips
HALF = [(256, 214), (318, 210), (378, 216), (424, 236), (452, 276), (468, 334), (474, 392), (464, 436), (436, 452),
        (404, 440), (378, 404), (350, 378), (300, 372)]
BODY = HALF + [(256, 370)] + [(2 * CX - x, y) for x, y in reversed(HALF[1:])]
body_d = smooth(BODY, k=0.95)
BOX = (34, 196, 478, 470)

def netting():
    """Voronoi cells over the body, smaller toward the rim so the body reads as rounded."""
    def inside(p):   # even-odd ray cast against the body's polygon
        c = False
        for (x1, y1), (x2, y2) in zip(BODY, BODY[1:] + BODY[:1]):
            if (y1 > p[1]) != (y2 > p[1]) and p[0] < x1 + (p[1] - y1) * (x2 - x1) / (y2 - y1): c = not c
        return c
    pts = []
    while len(pts) < 330:
        p = (rng.uniform(BOX[0], BOX[2]), rng.uniform(BOX[1], BOX[3]))
        # keep fewer points in the middle: big cells there, small ones at the edges
        if not inside(p) and rng.random() > 0.15: continue
        dmid = math.hypot((p[0] - CX) / 220, (p[1] - 320) / 120)
        if rng.random() > 0.35 + 0.65 * min(1, dmid): continue
        pts.append(p)
    pts = np.array(pts)
    cells = []
    box = [(BOX[0] - 20, BOX[1] - 20), (BOX[2] + 20, BOX[1] - 20), (BOX[2] + 20, BOX[3] + 20), (BOX[0] - 20, BOX[3] + 20)]
    for i, p in enumerate(pts):
        poly = box
        for j, q in enumerate(pts):
            if i == j or np.hypot(*(p - q)) > 90: continue
            m = (p + q) / 2; nrm = q - p; new = []
            for k in range(len(poly)):
                a, b = np.array(poly[k]), np.array(poly[(k + 1) % len(poly)])
                da, db = np.dot(a - m, nrm), np.dot(b - m, nrm)
                if da <= 0: new.append(tuple(a))
                if da * db < 0: t = da / (da - db); new.append(tuple(a + t * (b - a)))
            poly = new
            if len(poly) < 3: break
        if len(poly) >= 3:
            c = np.mean(poly, axis=0)
            gap = 1.9
            cells.append([tuple(c + (np.array(v) - c) * max(0.3, 1 - gap / max(4, np.linalg.norm(np.array(v) - c)))) for v in poly])
    return cells

# ---------------------------------------------------------------- vine leaf (points up, base at origin, length 1)
def leaf_pts():
    pts = []
    for d in np.arange(-176, 177, 4):
        t = math.radians(d)
        lob = max(math.exp(-((d - c) / 24.0) ** 2) * h for c, h in ((0, 1.0), (58, 0.86), (-58, 0.86), (116, 0.62), (-116, 0.62)))
        r = 0.27 + 0.25 * lob + 0.012 * math.sin(math.radians(d) * 22)
        if abs(d) > 150: r *= 0.55 + 0.45 * (180 - abs(d)) / 30
        pts.append((r * math.sin(t), -0.5 - r * math.cos(t)))
    return pts
LEAF_VEINS = [[(0, -0.08), (0, -0.96)], [(0, -0.30), (0.40, -0.76)], [(0, -0.30), (-0.40, -0.76)],
              [(0, -0.22), (0.44, -0.30)], [(0, -0.22), (-0.44, -0.30)]]

def leaf(P, tx, ty, rot, sc, lw, veins=True):
    body = smooth(xf(leaf_pts(), tx, ty, rot, sc), k=0.9)
    tip = xf([(0, -1)], tx, ty, rot, sc)[0]
    out = [f'<linearGradient id="leafg" gradientUnits="userSpaceOnUse" x1="{f(tx)}" y1="{f(ty)}" x2="{f(tip[0])}" y2="{f(tip[1])}">'
           f'<stop offset="0" stop-color="{P["leaf"][0]}"/><stop offset="1" stop-color="{P["leaf"][1]}"/></linearGradient>',
           f'<clipPath id="leafc"><path d="{body}"/></clipPath>',
           f'<path d="{body}" fill="url(#leafg)" stroke="{P["line"]}" stroke-width="{f(lw)}" stroke-linejoin="round"/>']
    if veins:
        out.append('<g clip-path="url(#leafc)">')
        for v in LEAF_VEINS:
            a, b = xf(v, tx, ty, rot, sc)
            out.append(f'<path d="M{f(a[0])},{f(a[1])} L{f(b[0])},{f(b[1])}" stroke="{P["vein"]}" stroke-width="{f(sc * 0.022)}" '
                       f'stroke-linecap="round" opacity="0.75"/>')
        out.append('</g>')
    return '\n'.join(out)

# ---------------------------------------------------------------- the controls
def dpad(P, cx, cy, arm, w, lw):
    """a plus cut into the rind, showing the flesh"""
    a, h = arm, w / 2
    pts = [(-h, -a), (h, -a), (h, -h), (a, -h), (a, h), (h, h), (h, a), (-h, a), (-h, h), (-a, h), (-a, -h), (-h, -h)]
    r = w * 0.22   # rounded arm ends
    d = f'M{f(cx - h)},{f(cy - a + r)} Q{f(cx - h)},{f(cy - a)} {f(cx - h + r)},{f(cy - a)} L{f(cx + h - r)},{f(cy - a)} ' \
        f'Q{f(cx + h)},{f(cy - a)} {f(cx + h)},{f(cy - a + r)} L{f(cx + h)},{f(cy - h)} L{f(cx + a - r)},{f(cy - h)} ' \
        f'Q{f(cx + a)},{f(cy - h)} {f(cx + a)},{f(cy - h + r)} L{f(cx + a)},{f(cy + h - r)} Q{f(cx + a)},{f(cy + h)} {f(cx + a - r)},{f(cy + h)} ' \
        f'L{f(cx + h)},{f(cy + h)} L{f(cx + h)},{f(cy + a - r)} Q{f(cx + h)},{f(cy + a)} {f(cx + h - r)},{f(cy + a)} ' \
        f'L{f(cx - h + r)},{f(cy + a)} Q{f(cx - h)},{f(cy + a)} {f(cx - h)},{f(cy + a - r)} L{f(cx - h)},{f(cy + h)} ' \
        f'L{f(cx - a + r)},{f(cy + h)} Q{f(cx - a)},{f(cy + h)} {f(cx - a)},{f(cy + h - r)} L{f(cx - a)},{f(cy - h + r)} ' \
        f'Q{f(cx - a)},{f(cy - h)} {f(cx - a + r)},{f(cy - h)} L{f(cx - h)},{f(cy - h)} Z'
    return (f'<path d="{d}" fill="url(#flesh)" stroke="{P["line"]}" stroke-width="{f(lw)}" stroke-linejoin="round"/>'
            f'<circle cx="{f(cx)}" cy="{f(cy)}" r="{f(w * 0.2)}" fill="{P["flesh"][1]}" opacity="0.8"/>')

def seeds(P, cx, cy, rad, sx, sy, lw, socket):
    """four melon seeds in a diamond, lying in a round of flesh, each pointing at the centre"""
    out = []
    if socket:
        out.append(f'<circle cx="{f(cx)}" cy="{f(cy)}" r="{f(socket)}" fill="url(#fleshr)" stroke="{P["line"]}" stroke-width="{f(lw)}"/>')
    for ang in (0, 90, 180, 270):
        x, y = cx + rad * math.sin(math.radians(ang)), cy - rad * math.cos(math.radians(ang))
        # a seed: pointed toward the centre, round at the outside
        pts = [(0, -sy), (sx * 0.75, -sy * 0.35), (sx, sy * 0.35), (0, sy), (-sx, sy * 0.35), (-sx * 0.75, -sy * 0.35)]
        pts = xf(pts, x, y, ang + 180, 1)
        out.append(f'<path d="{smooth(pts, k=1.0)}" fill="url(#seed)" stroke="{P["seed"][1]}" stroke-width="{f(lw * 0.6)}"/>')
        out.append(f'<path d="{smooth(pts, k=1.0)}" fill="none" stroke="{P["line"]}" stroke-width="{f(lw * 0.45)}" opacity="0.6"/>')
    return '\n'.join(out)

# ---------------------------------------------------------------- assemble
def build(mode='full', pal='green'):
    P = PAL[pal]; small = mode == 'small'
    lw = 20 if small else 5
    parts = []
    # stem, tendril and leaf growing out of the top of the pad
    sw = 26 if small else 15
    parts.append(f'<path d="M256,222 C252,198 256,176 270,160" fill="none" stroke="{P["line"]}" stroke-width="{sw}" stroke-linecap="round"/>'
                 f'<path d="M256,222 C252,198 256,176 270,160" fill="none" stroke="{P["stem"]}" stroke-width="{sw - 6 - (6 if small else 0)}" stroke-linecap="round"/>')
    if small:
        parts.append(leaf(P, 262, 190, 58, 150, lw, veins=False))
    else:
        parts.append(leaf(P, 262, 186, 58, 122, 3.5))
        parts.append(f'<path d="M252,202 C228,196 214,176 226,164 C236,154 251,163 243,174 C237,180 229,174 233,170" fill="none" '
                     f'stroke="{P["line"]}" stroke-width="8" stroke-linecap="round"/>'
                     f'<path d="M252,202 C228,196 214,176 226,164 C236,154 251,163 243,174 C237,180 229,174 233,170" fill="none" '
                     f'stroke="{P["tendril"]}" stroke-width="4.5" stroke-linecap="round"/>')
    # the body
    if small:
        parts.append(f'<path d="{body_d}" fill="url(#rind)"/>')
    else:
        parts.append(f'<path d="{body_d}" fill="{P["net"]}"/>')
        parts.append(f'<g clip-path="url(#bodyclip)" fill="url(#rind)">' +
                     ''.join(f'<path d="{smooth(c, k=0.35)}"/>' for c in netting()) + '</g>')
    parts.append(f'<path d="{body_d}" fill="url(#shade)"/><path d="{body_d}" fill="url(#shine)"/>')
    parts.append(f'<path d="{body_d}" fill="none" stroke="{P["line"]}" stroke-width="{lw}" stroke-linejoin="round"/>')
    # the controls
    if small:
        parts.append(dpad(P, 152, 300, 62, 42, 12))
        parts.append(seeds(P, 356, 300, 38, 15, 22, 12, 74))
    else:
        parts.append(dpad(P, 156, 300, 50, 33, 4.5))
        parts.append(seeds(P, 358, 300, 34, 11, 17, 5, 58))
        for x in (234, 278):   # select and start, two small rind pills
            parts.append(f'<rect x="{x - 15}" y="335" width="30" height="12" rx="6" fill="{P["pill"]}" '
                         f'stroke="{P["line"]}" stroke-width="3.5"/>')
    defs = (f'<clipPath id="bodyclip"><path d="{body_d}"/></clipPath>'
            f'<radialGradient id="rind" cx="0.38" cy="0.22" r="0.85"><stop offset="0" stop-color="{P["rind"][0]}"/>'
            f'<stop offset="0.55" stop-color="{P["rind"][1]}"/><stop offset="1" stop-color="{P["rind"][2]}"/></radialGradient>'
            f'<radialGradient id="shade" cx="0.45" cy="0.30" r="0.75"><stop offset="0.6" stop-color="#000" stop-opacity="0"/>'
            f'<stop offset="1" stop-color="{P["shade"]}" stop-opacity="0.5"/></radialGradient>'
            '<radialGradient id="shine" cx="0.32" cy="0.16" r="0.36"><stop offset="0" stop-color="#fff" stop-opacity="0.26"/>'
            '<stop offset="1" stop-color="#fff" stop-opacity="0"/></radialGradient>'
            f'<linearGradient id="flesh" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="{P["flesh"][0]}"/>'
            f'<stop offset="1" stop-color="{P["flesh"][1]}"/></linearGradient>'
            f'<radialGradient id="fleshr" cx="0.5" cy="0.5" r="0.5"><stop offset="0" stop-color="{P["flesh"][1]}"/>'
            f'<stop offset="0.75" stop-color="{P["flesh"][0]}"/><stop offset="0.88" stop-color="#e9efb0"/>'
            f'<stop offset="1" stop-color="#b6d98a"/></radialGradient>'
            f'<linearGradient id="seed" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="{P["seed"][0]}"/>'
            f'<stop offset="1" stop-color="#f1d9a0"/></linearGradient>')
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512" width="512" height="512">'
            f'<defs>{defs}</defs>\n<g transform="translate(0,-16)">\n' + '\n'.join(parts) + '\n</g>\n</svg>\n')

if __name__ == '__main__':
    open(sys.argv[1], 'w').write(build(sys.argv[2] if len(sys.argv) > 2 else 'full', sys.argv[3] if len(sys.argv) > 3 else 'green'))

#!/usr/bin/env python3
"""melon logo: a serene cantaloupe face in a wreath of vine leaves (vector, SVG).
  logo.py <out.svg>"""
import sys, math
import numpy as np

CX = 256.0
rng = np.random.default_rng(11)

# ---------------------------------------------------------------- helpers
def f(v): return f'{v:.2f}'.rstrip('0').rstrip('.')

def smooth(pts, closed=True, k=1.0):
    """Catmull-Rom through pts as a cubic Bezier path."""
    n = len(pts); P = [np.array(p, float) for p in pts]
    s = f'M{f(P[0][0])},{f(P[0][1])}'
    rng_ = range(n) if closed else range(n - 1)
    for i in rng_:
        p0 = P[(i - 1) % n] if closed or i > 0 else P[i]
        p1, p2 = P[i], P[(i + 1) % n]
        p3 = P[(i + 2) % n] if closed or i + 2 < n else P[(i + 1) % n]
        c1 = p1 + (p2 - p0) / 6 * k; c2 = p2 - (p3 - p1) / 6 * k
        s += f' C{f(c1[0])},{f(c1[1])} {f(c2[0])},{f(c2[1])} {f(p2[0])},{f(p2[1])}'
    return s + (' Z' if closed else '')

def mirror(pts): return [(2 * CX - x, y) for x, y in pts]

def xf(pts, tx, ty, rot, sc, flip=False):
    c, s = math.cos(math.radians(rot)), math.sin(math.radians(rot))
    out = []
    for x, y in pts:
        if flip: x = -x
        x, y = x * sc, y * sc
        out.append((tx + x * c - y * s, ty + x * s + y * c))
    return out

# ---------------------------------------------------------------- the face: an egg of netted rind
FACE = [(256, 132), (320, 142), (360, 190), (366, 250), (350, 312), (308, 356), (256, 370),
        (204, 356), (162, 312), (146, 250), (152, 190), (192, 142)]
face_d = smooth(FACE)

def netting():
    """Voronoi cells of points spread over a sphere's front, projected onto the face: small cells near the rim."""
    pts = []
    while len(pts) < 460:
        v = rng.normal(size=3); v /= np.linalg.norm(v)
        if v[2] < 0.05: continue
        pts.append((CX + v[0] * 112, 250 + v[1] * 136))
    pts = np.array(pts)
    cells = []
    box = [(130, 100), (382, 100), (382, 400), (130, 400)]
    for i, p in enumerate(pts):
        poly = box
        for j, q in enumerate(pts):
            if i == j or np.hypot(*(p - q)) > 60: continue
            m = (p + q) / 2; nrm = q - p
            new = []
            for k in range(len(poly)):
                a, b = np.array(poly[k]), np.array(poly[(k + 1) % len(poly)])
                da, db = np.dot(a - m, nrm), np.dot(b - m, nrm)
                if da <= 0: new.append(tuple(a))
                if da * db < 0:
                    t = da / (da - db); new.append(tuple(a + t * (b - a)))
            poly = new
            if len(poly) < 3: break
        if len(poly) >= 3:
            c = np.mean(poly, axis=0)
            # inset toward the centroid: the gaps are the netting
            d = np.hypot(*(np.array(p) - np.array([CX, 250])) / [112, 136])
            gap = 1.7 * (1 - 0.6 * d)
            ins = [tuple(c + (np.array(v) - c) * max(0.3, 1 - gap / max(4, np.linalg.norm(np.array(v) - c)))) for v in poly]
            cells.append(ins)
    return cells

# ---------------------------------------------------------------- vine leaf (points up, base at origin, length 1)
def leaf_pts():
    pts = []
    for d in np.arange(-176, 177, 4):
        t = math.radians(d)
        lob = max(math.exp(-((d - c) / 24.0) ** 2) * h for c, h in ((0, 1.0), (58, 0.86), (-58, 0.86), (116, 0.62), (-116, 0.62)))
        r = 0.27 + 0.25 * lob + 0.012 * math.sin(math.radians(d) * 22)
        if abs(d) > 150: r *= 0.55 + 0.45 * (180 - abs(d)) / 30          # heart-shaped notch at the stem
        pts.append((r * math.sin(t), -0.5 - r * math.cos(t)))
    return pts
LEAF_VEINS = [[(0, -0.08), (0, -0.96)], [(0, -0.30), (0.40, -0.76)], [(0, -0.30), (-0.40, -0.76)],
              [(0, -0.22), (0.44, -0.30)], [(0, -0.22), (-0.44, -0.30)]]

def leaf(tx, ty, rot, sc, dark='#1f4d25', light='#3f8a3a', vein='#8ccf68', flip=False, gid=[0]):
    gid[0] += 1; g = f'lg{gid[0]}'
    body = smooth(xf(leaf_pts(), tx, ty, rot, sc, flip), k=0.9)
    tip = xf([(0, -1)], tx, ty, rot, sc, flip)[0]
    out = [f'<linearGradient id="{g}" gradientUnits="userSpaceOnUse" x1="{f(tx)}" y1="{f(ty)}" x2="{f(tip[0])}" y2="{f(tip[1])}">'
           f'<stop offset="0" stop-color="{dark}"/><stop offset="1" stop-color="{light}"/></linearGradient>',
           f'<clipPath id="{g}c"><path d="{body}"/></clipPath>',
           f'<path d="{body}" fill="url(#{g})" stroke="#0f2513" stroke-width="3" stroke-linejoin="round"/>', f'<g clip-path="url(#{g}c)">']
    for v in LEAF_VEINS:
        a, b = xf(v, tx, ty, rot, sc, flip)
        out.append(f'<path d="M{f(a[0])},{f(a[1])} L{f(b[0])},{f(b[1])}" stroke="{vein}" stroke-width="{f(max(1.2, sc * 0.022))}" '
                   f'stroke-linecap="round" opacity="0.75"/>')
    out.append('</g>')
    return '\n'.join(out)

def wedge(x, y, rot, sc, flip=False):
    """a cantaloupe wedge seen from the side: pale green rind below, orange flesh, seeds along the cut"""
    arc = lambda r, a0, a1, n=16, dy=0.0: [(math.cos(math.radians(a)) * r, math.sin(math.radians(a)) * r + dy) for a in np.linspace(a0, a1, n)]
    rind = arc(1.0, 10, 170) + arc(0.84, 170, 10)
    flesh = arc(0.84, 10, 170) + [(-0.70, 0.14), (0, 0.08), (0.70, 0.14)]
    T = lambda pts: xf([(px, py - 0.3) for px, py in pts], x, y, rot, sc, flip)
    out = [f'<path d="{smooth(T(rind), k=0.4)}" fill="url(#rindw)" stroke="#0f2513" stroke-width="3" stroke-linejoin="round"/>',
           f'<path d="{smooth(T(flesh), k=0.4)}" fill="url(#flesh)" stroke="#0f2513" stroke-width="3" stroke-linejoin="round"/>']
    for k, px in enumerate(np.linspace(-0.5, 0.5, 6)):
        sx, sy = T([(px, 0.22 + 0.03 * (k % 2))])[0]
        out.append(f'<ellipse cx="{f(sx)}" cy="{f(sy)}" rx="{f(sc*0.035)}" ry="{f(sc*0.07)}" fill="#fbecc6" stroke="#b9772e" '
                   f'stroke-width="1" transform="rotate({f(rot + (15 if k % 2 else -10))} {f(sx)} {f(sy)})"/>')
    return '\n'.join(out)

# ---------------------------------------------------------------- the features (friendly: happy closed eyes, a smile)
def features():
    D = '#16341c'
    s = []
    for right in (True, False):
        side = (lambda p: p) if right else mirror
        brow = side([(282, 214), (298, 207), (316, 208), (328, 214)])
        eye = side([(280, 250), (292, 239), (310, 237), (326, 247)])          # a closed eye smiling upward
        s.append(f'<path d="{smooth(brow, closed=False)}" fill="none" stroke="{D}" stroke-width="5" stroke-linecap="round"/>')
        s.append(f'<path d="{smooth(eye, closed=False)}" fill="none" stroke="{D}" stroke-width="6" stroke-linecap="round"/>')
        cx = 312 if right else 2 * CX - 312
        s.append(f'<ellipse cx="{cx}" cy="282" rx="19" ry="12" fill="#f29a4c" opacity="0.45"/>')   # rosy cheeks
        must = side([(256, 304), (268, 299), (282, 300), (293, 304), (303, 300), (306, 291), (300, 286), (299, 293),
                     (294, 297), (284, 310), (270, 314), (256, 311)])
        s.append(f'<path d="{smooth(must, k=0.8)}" fill="{D}"/>')
    s.append(f'<path d="M248,266 C246,282 250,290 256,291 C262,290 266,282 264,266" fill="#6fae52" stroke="{D}" stroke-width="3.5" '
             f'stroke-linejoin="round"/>')                                                # a small rounded nose
    s.append(f'<path d="M236,326 C246,342 266,342 276,326" fill="#b8423a" stroke="{D}" stroke-width="4" stroke-linejoin="round"/>')
    return '\n'.join(s)

# ---------------------------------------------------------------- assemble
def build():
    parts = []
    # the leafy mane: leaves fan out from behind the head, bigger at the top, smaller toward the chin
    fc = (256, 262)
    for ang, sc, dist in ((0, 150, 96), (34, 142, 102), (68, 136, 108), (102, 128, 110), (136, 112, 104)):
        for sgn in ((1,) if ang == 0 else (1, -1)):
            a = math.radians(ang * sgn)
            bx, by = fc[0] + math.sin(a) * dist * 0.55, fc[1] - math.cos(a) * dist * 0.55
            dark, light = (('#1d4a24', '#3b8537') if ang in (0, 68, 136) else ('#18401f', '#34793a'))
            parts.append(leaf(bx, by, ang * sgn, sc, dark=dark, light=light, flip=sgn < 0))
    # the chin leaf, pointing down, then the wedges tucked in at the lower sides
    parts.append(leaf(256, 352, 180, 84))
    parts.append(wedge(162, 392, 32, 64)); parts.append(wedge(2 * CX - 162, 392, -32, 64, flip=True))
    # the face
    cells = netting()
    parts.append(f'<path d="{face_d}" fill="#dcefb8"/>')
    cell_s = ''.join(f'<path d="{smooth(c, k=0.35)}"/>' for c in cells)
    parts.append(f'<g clip-path="url(#faceclip)" fill="url(#rind)">{cell_s}</g>')
    parts.append(f'<path d="{face_d}" fill="url(#shade)"/>')
    parts.append(f'<path d="{face_d}" fill="url(#calm)"/>')
    parts.append(f'<path d="{face_d}" fill="url(#shine)"/>')
    parts.append(features())
    parts.append(f'<path d="{face_d}" fill="none" stroke="#0f2513" stroke-width="4.5"/>')
    defs = (f'<clipPath id="faceclip"><path d="{face_d}"/></clipPath>'
            '<radialGradient id="rind" cx="0.40" cy="0.34" r="0.78"><stop offset="0" stop-color="#8bca5e"/>'
            '<stop offset="0.55" stop-color="#5a9d45"/><stop offset="1" stop-color="#2f6431"/></radialGradient>'
            '<radialGradient id="shade" cx="0.42" cy="0.36" r="0.70"><stop offset="0.6" stop-color="#000" stop-opacity="0"/>'
            '<stop offset="1" stop-color="#0b2210" stop-opacity="0.5"/></radialGradient>'
            '<radialGradient id="calm" cx="0.5" cy="0.56" r="0.42"><stop offset="0" stop-color="#6cab4f" stop-opacity="0.8"/>'
            '<stop offset="0.7" stop-color="#6cab4f" stop-opacity="0.35"/><stop offset="1" stop-color="#6cab4f" stop-opacity="0"/></radialGradient>'
            '<radialGradient id="shine" cx="0.36" cy="0.26" r="0.30"><stop offset="0" stop-color="#fff" stop-opacity="0.22"/>'
            '<stop offset="1" stop-color="#fff" stop-opacity="0"/></radialGradient>'
            '<linearGradient id="flesh" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#f7c47a"/>'
            '<stop offset="1" stop-color="#f29a4c"/></linearGradient>'
            '<linearGradient id="rindw" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#cfe6a4"/>'
            '<stop offset="1" stop-color="#6ea64c"/></linearGradient>')
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512" width="512" height="512">'
            f'<defs>{defs}</defs>\n' + '\n'.join(parts) + '\n</svg>\n')

def build_melon():
    """the melon from the wallpaper planet: a netted sphere with its ribs, a curled stem and one leaf"""
    global FACE, face_d
    R, cy = 150, 300
    circ = [(CX + R * math.sin(math.radians(a)), cy - R * math.cos(math.radians(a))) for a in range(0, 360, 20)]
    face_d = smooth(circ)
    parts = []
    # stem, tendril and leaf above the melon
    parts.append('<path d="M256,158 C253,140 257,124 268,112" fill="none" stroke="#0f2513" stroke-width="15" stroke-linecap="round"/>'
                 '<path d="M256,158 C253,140 257,124 268,112" fill="none" stroke="#7a9a44" stroke-width="9" stroke-linecap="round"/>')
    parts.append(leaf(260, 138, 58, 112, dark='#1d4a24', light='#4b9a3f'))
    parts.append('<path d="M252,146 C230,140 216,122 228,110 C238,100 252,110 244,120 C238,126 230,120 234,116" fill="none" '
                 'stroke="#6fbf4a" stroke-width="4.5" stroke-linecap="round"/>')
    # the melon: netting cells projected on the sphere, clipped to the circle, then ribs, shading, shine and outline
    pts = []
    while len(pts) < 520:
        v = rng.normal(size=3); v /= np.linalg.norm(v)
        if v[2] < 0.05: continue
        pts.append((CX + v[0] * R, cy + v[1] * R))
    global netting_pts
    cells = netting_sphere(np.array(pts), R, cy)
    parts.append(f'<path d="{face_d}" fill="#dcefb8"/>')
    parts.append(f'<g clip-path="url(#faceclip)" fill="url(#rind)">' + ''.join(f'<path d="{smooth(c, k=0.35)}"/>' for c in cells) + '</g>')
    for dx in (-0.62, 0, 0.62):
        x0 = CX + dx * R
        parts.append(f'<path d="M{f(x0)},{f(cy - R * math.cos(math.asin(min(0.99, abs(dx)))))} '
                     f'Q{f(CX + dx * R * 1.35)},{cy} {f(x0)},{f(cy + R * math.cos(math.asin(min(0.99, abs(dx)))))}" '
                     f'fill="none" stroke="#2d5e2c" stroke-width="7" opacity="0.55" clip-path="url(#faceclip)"/>')
    parts.append(f'<path d="{face_d}" fill="url(#shade)"/><path d="{face_d}" fill="url(#shine)"/>')
    parts.append(f'<path d="{face_d}" fill="none" stroke="#0f2513" stroke-width="5"/>')
    defs = (f'<clipPath id="faceclip"><path d="{face_d}"/></clipPath>'
            '<radialGradient id="rind" cx="0.38" cy="0.32" r="0.80"><stop offset="0" stop-color="#8bca5e"/>'
            '<stop offset="0.55" stop-color="#5a9d45"/><stop offset="1" stop-color="#2f6431"/></radialGradient>'
            '<radialGradient id="shade" cx="0.40" cy="0.34" r="0.70"><stop offset="0.55" stop-color="#000" stop-opacity="0"/>'
            '<stop offset="1" stop-color="#0b2210" stop-opacity="0.55"/></radialGradient>'
            '<radialGradient id="shine" cx="0.34" cy="0.26" r="0.32"><stop offset="0" stop-color="#fff" stop-opacity="0.28"/>'
            '<stop offset="1" stop-color="#fff" stop-opacity="0"/></radialGradient>')
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512" width="512" height="512">'
            f'<defs>{defs}</defs>\n' + '\n'.join(parts) + '\n</svg>\n')

def netting_sphere(pts, R, cy):
    cells = []
    box = [(CX - R - 10, cy - R - 10), (CX + R + 10, cy - R - 10), (CX + R + 10, cy + R + 10), (CX - R - 10, cy + R + 10)]
    for i, p in enumerate(pts):
        poly = box
        for j, q in enumerate(pts):
            if i == j or np.hypot(*(p - q)) > 70: continue
            m = (p + q) / 2; nrm = q - p; new = []
            for k in range(len(poly)):
                a, b = np.array(poly[k]), np.array(poly[(k + 1) % len(poly)])
                da, db = np.dot(a - m, nrm), np.dot(b - m, nrm)
                if da <= 0: new.append(tuple(a))
                if da * db < 0: t = da / (da - db); new.append(tuple(a + t * (b - a)))
            poly = new
            if len(poly) < 3: break
        if len(poly) >= 3:
            c = np.mean(poly, axis=0); d = np.hypot(*(np.array(p) - np.array([CX, cy]))) / R
            gap = 1.9 * (1 - 0.6 * d)
            cells.append([tuple(c + (np.array(v) - c) * max(0.3, 1 - gap / max(4, np.linalg.norm(np.array(v) - c)))) for v in poly])
    return cells

mode = sys.argv[2] if len(sys.argv) > 2 else 'face'
open(sys.argv[1], 'w').write(build() if mode == 'face' else build_melon())

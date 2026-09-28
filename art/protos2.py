#!/usr/bin/env python3
"""Title-free reward concepts on the fx studio.  protos2.py <out-dir> <logo.png> <font-dir> <rain|portal>"""
import math, sys
import numpy as np
from PIL import Image, ImageDraw
import fx
from fx import W, H, GOLD, xx, yy, blur, gold, over, mix

OUT, LOGO, FONTDIR, WHICH = (sys.argv[1:5] + [None] * 4)[:4] if __name__ == '__main__' else (None, None, None, None)
F = fx.Fonts(FONTDIR or 'fonts/x/usr/share/fonts/truetype/noto')

def corner_line(img, text, y=0.905):
    """One quiet line, bottom right, above where the Plasma panel sits."""
    f = F('NotoSerifDisplay-Italic', 0.026 * H)
    m = fx.mask_from_text(text, f, (0, 0), size=(1, 1))  # measure below instead
    w = f.getlength(text) + len(text) * 0.003 * H
    fx.gradient_text(img, (0.962 * W - w / 2, y * H), text, f, (236, 214, 160), (190, 150, 86), spacing=int(0.003 * H), shadow=8)

def hash2(i, j, s):
    return np.modf(np.sin(i * 127.1 + j * 311.7 + s) * 43758.5453)[0] % 1.0

def net_wrap(u, v, width, wrap_u, seed=0.0):
    """Cantaloupe netting (Worley F2-F1 ridges); the u axis wraps every wrap_u cells (for a tunnel)."""
    iu, iv = np.floor(u), np.floor(v)
    f1 = np.full(u.shape, 9.0, np.float32); f2 = np.full(u.shape, 9.0, np.float32)
    for di in (-1, 0, 1):
        for dj in (-1, 0, 1):
            ci, cj = iu + di, iv + dj; cw = np.mod(ci, wrap_u)
            px = 0.15 + 0.7 * hash2(cw, cj, seed); py = 0.15 + 0.7 * hash2(cw, cj, seed + 17.3)
            d = np.sqrt((ci + px - u) ** 2 + (cj + py - v) ** 2).astype(np.float32)
            f2 = np.where(d < f1, f1, np.minimum(f2, d)); f1 = np.minimum(f1, d)
    return np.clip(1 - (f2 - f1) / width, 0, 1) ** 1.5

# ------------------------------------------------------------------ 1. golden rain
def rain():
    rs = np.random.default_rng(31)
    hdr = mix([0.010, 0.008, 0.006], [0.10, 0.065, 0.025], np.exp(-np.hypot(xx - 0.45 * W, yy + 0.2 * H) / (0.9 * H)))
    # soft beams from above
    ang = np.arctan2(yy + 0.3 * H, xx - 0.45 * W)
    beam = np.clip(np.sin(ang * 23) + 0.6 * np.sin(ang * 41 + 1), 0, None) ** 2 * np.exp(-(yy / H) * 1.4)
    hdr += beam[..., None] * GOLD * 0.10
    tilt = math.radians(8)                                    # the rain falls slightly slanted
    melons = []
    for i in range(64):
        depth = rs.uniform(0, 1) ** 1.3                       # 0 = far, 1 = near
        size = (0.016 + 0.13 * depth ** 2.6) * H
        x, y = rs.uniform(-0.05, 1.05) * W, rs.uniform(-0.1, 1.05) * H
        if x > 0.66 * W and y > 0.78 * H: continue              # keep the corner line clear
        melons.append((depth, x, y, size, rs.uniform(0, math.pi), rs.uniform(0, 2 * math.pi), rs.uniform(0.35, 0.8)))
    melons.sort()
    for depth, x, y, size, rot, spin, tip in melons:
        a_, b_ = size * 1.45, size * 0.92                     # watermelon: oblong
        streak = int(10 + 150 * depth ** 1.3)
        dof = 1.5 + 14 * max(0, depth - 0.85) / 0.15 + 4 * max(0, 0.25 - depth) / 0.25
        rad = int(a_ * 1.05) + 4; px_ = rad + int(3 * dof) + 4; py_ = rad + streak + int(3 * dof) + 4   # room for blur and streak
        x0c, y0c, x1c, y1c = max(0, int(x) - px_), max(0, int(y) - py_), min(W, int(x) + px_), min(H, int(y) + py_)
        if x1c <= x0c or y1c <= y0c: continue
        py, px = np.mgrid[y0c:y1c, x0c:x1c].astype(np.float32)
        dx, dy = px - x, py - y
        u = (dx * math.cos(rot) + dy * math.sin(rot)) / a_
        v = (-dx * math.sin(rot) + dy * math.cos(rot)) / b_
        d2 = u * u + v * v
        inside = np.clip((1 - np.sqrt(np.clip(d2, 0, 4))) * b_ / 1.3, 0, 1)
        if not inside.any(): continue
        nzl = np.sqrt(np.clip(1 - d2, 0, 1))
        # stripes run pole to pole: angle around the long axis, with the melon turned by `spin`
        phi = np.arctan2(v, nzl + 1e-4) + spin
        wob = 0.12 * np.sin(u * 11 + spin) + 0.07 * np.sin(u * 23 - 2 * spin)   # jagged edges, pole to pole
        stripe = 0.5 + 0.5 * np.sin(phi * 6 + wob * 6)
        stripe = np.clip((stripe - 0.35) * 3, 0, 1)
        lx, ly = u * (b_ / a_), v
        bx = lx * math.cos(rot) - ly * math.sin(rot); by = lx * math.sin(rot) + ly * math.cos(rot)
        nl = np.sqrt(bx ** 2 + by ** 2 + nzl ** 2) + 1e-6
        tint = mix(np.array([0.55, 0.36, 0.12]), GOLD, stripe)     # darker gold stripes, polished gold between
        col = gold(bx / nl, by / nl, nzl / nl, ao=0.7 + 0.3 * stripe, gloss=stripe * 0.8, fill=0.5, tint=GOLD)
        col = col * (tint / GOLD)
        # depth: far melons fade into the haze
        haze = (1 - depth) * 0.55
        col = col * (1 - haze) + np.array([0.10, 0.065, 0.025]) * haze
        # motion blur along the fall direction + depth of field, applied to this sprite
        C = col * inside[..., None]; A = inside
        k = max(1, streak // 2)
        C = fx._box(C, k, 0); A = fx._box(A, k, 0)
        if dof > 2:
            C = blur(C, dof); A = blur(A, dof)
        sl = hdr[y0c:y1c, x0c:x1c]
        hdr[y0c:y1c, x0c:x1c] = sl * (1 - np.clip(A, 0, 1)[..., None]) + C
    img = fx.to_img(fx.develop(hdr, exposure=1.1, streak=0.9))
    img = fx.bokeh(img, rs, top=H, layers=((90, 2, 7, 1.2, 160), (14, 30, 80, 9, 34)))
    corner_line(img, 'you survived the gauntlet.')
    return fx.lens(img, rs)

# ------------------------------------------------------------------ 2. the other side: a tunnel through the netting
def portal(cxf=0.60, cyf=0.44, text='you went through the gauntlet. this is the other side.', text_at='corner', calm_left=0.0):
    rs = np.random.default_rng(47)
    cx, cy = cxf * W, cyf * H
    dx, dy = xx - cx, yy - cy
    r = np.hypot(dx, dy) / H + 1e-4
    ang = np.arctan2(dy, dx)
    z = 0.32 / r                                              # depth down the tunnel
    N = 26
    u = (ang / (2 * np.pi) + 0.5) * N + 1.1 * z               # twist as it recedes
    v = z * 3.2
    ridge = net_wrap(u, v, 0.11, N, 2.0)
    fine = net_wrap(u * 2, v * 2, 0.08, 2 * N, 7.0) * 0.35
    ridge = np.maximum(ridge, fine)
    glow = np.clip(0.035 / r, 0, 8)                           # the light at the end
    fog = np.exp(-z * 0.09)
    emerald = np.array([0.012, 0.07, 0.04])
    walls = (emerald * (0.15 + 0.6 * glow * fog)[..., None]
             + (GOLD * (ridge * (0.10 + 1.1 * glow * fog))[..., None]))
    # a shimmer that runs along the ridges
    walls += (GOLD * (ridge * np.clip(np.sin(v * 1.3 - u * (4 * np.pi / N)), 0, 1) ** 8 * 0.9 * fog)[..., None])
    hdr = walls
    hdr += (np.exp(-r / 0.02) * 9 + np.exp(-r / 0.07) * 1.4)[..., None] * np.array([1.0, 0.9, 0.7])
    # warp streaks: particles rushing past towards the viewer
    lay = Image.new('F', (W, H), 0.0); d = ImageDraw.Draw(lay)
    for _ in range(420):
        a = rs.uniform(-math.pi, math.pi); r0 = rs.uniform(0.04, 0.9) * H; ln = r0 * rs.uniform(0.08, 0.35)
        br = rs.uniform(0.3, 1.0) * (r0 / H) ** 0.5
        d.line([(cx + math.cos(a) * r0, cy + math.sin(a) * r0), (cx + math.cos(a) * (r0 + ln), cy + math.sin(a) * (r0 + ln))],
               fill=float(br), width=int(1 + 3 * r0 / H))
    streaks = blur(np.asarray(lay, np.float32), 1.5)
    hdr += streaks[..., None] * np.array([1.0, 0.85, 0.55]) * 1.4
    hdr = hdr * (1 - 0.35 * np.clip((r - 0.55) / 0.5, 0, 1))[..., None]
    ty = {'corner': 0.905, 'top': 0.075, 'grub': 0.962}[text_at]
    corner = np.exp(-(((xx - 0.86 * W) / (0.22 * W)) ** 2 + ((yy - ty * H) / (0.07 * H)) ** 2))
    hdr = hdr * (1 - 0.8 * corner)[..., None]                   # a quiet pocket for the corner line
    if calm_left:                                                # room for a boot menu on the left
        hdr = hdr * (1 - calm_left * np.clip(1 - xx / (0.62 * W), 0, 1) ** 1.2)[..., None]
    img = fx.to_img(fx.develop(hdr, exposure=1.0, streak=1.3, sat=1.25))
    # stronger dispersion than the other designs: this one is meant to feel unreal
    arr = np.asarray(img, np.float32)
    def scale(ch, s):
        im = Image.fromarray(ch, 'F'); w2, h2 = int(W * s), int(H * s)
        im = im.resize((w2, h2), Image.BICUBIC); l, t = (w2 - W) // 2, (h2 - H) // 2
        return np.asarray(im.crop((l, t, l + W, t + H)))
    arr = np.dstack([scale(arr[..., 0], 1.006), arr[..., 1], scale(arr[..., 2], 0.994)])
    img = fx.to_img(arr)
    corner_line(img, text, y=ty)
    return fx.lens(img, rs)

if __name__ == '__main__':
    fx.save({'rain': rain, 'portal': portal}[WHICH](), OUT, 'concept-' + WHICH)

"""The melon planet: a netted cantaloupe rising over the horizon (default desktop wallpaper, GRUB background)."""
import math
import numpy as np
from PIL import Image, ImageDraw, ImageFilter
import fx
from fx import W, H, xx, yy, mix

def hash2(i, j, s):
    return np.modf(np.sin(i * 127.1 + j * 311.7 + s) * 43758.5453)[0] % 1.0

def net(u, v, width, seed=0.0):
    """Cantaloupe netting: bright ridges along Worley cell borders, gently warped."""
    u = u + 0.18 * np.sin(v * 1.7 + seed); v = v + 0.18 * np.sin(u * 1.3 - seed)
    iu, iv = np.floor(u), np.floor(v)
    f1 = np.full(u.shape, 9.0, np.float32); f2 = np.full(u.shape, 9.0, np.float32)
    for di in (-1, 0, 1):
        for dj in (-1, 0, 1):
            ci, cj = iu + di, iv + dj
            px = 0.15 + 0.7 * hash2(ci, cj, seed); py = 0.15 + 0.7 * hash2(ci, cj, seed + 17.3)
            d = np.sqrt((ci + px - u) ** 2 + (cj + py - v) ** 2).astype(np.float32)
            f2 = np.where(d < f1, f1, np.minimum(f2, d)); f1 = np.minimum(f1, d)
    return np.clip(1 - (f2 - f1) / width, 0, 1) ** 1.6

GREEN = dict(bg0=[10, 14, 12], bg1=[24, 44, 28], far=[111, 191, 74], atm=[111, 191, 74], base0=[22, 48, 26], base1=[58, 104, 46],
             net0=[60, 96, 52], net1=[196, 232, 150], rim=[140, 215, 100], seed=(236, 226, 180), halo=(111, 191, 74, 60))

def planet(F, logo_path, wordmark=True, cxf=0.735, cyf=1.12, rf=0.70, seeds=140, P=GREEN, dim=1.0):
    rng = np.random.default_rng(7)
    cx, cy, r = cxf * W, cyf * H, rf * H
    d = np.sqrt((xx - cx) ** 2 + (yy - cy) ** 2) / r
    glow = np.exp(-np.clip(d - 1, 0, None) * 2.2)
    img = mix(np.array(P['bg0']), np.array(P['bg1']), glow * 0.85)
    img += (net(xx / 260, yy / 260, 0.05, 3.1) * 0.035 * (1 - glow))[..., None] * np.array(P['far'])
    atm = np.exp(-np.clip(d - 1, 0, None) * 14) * (d > 1)
    img = mix(img, P['atm'], atm * 0.55)
    inside = d < 1
    nx = (xx - cx) / r; ny = (yy - cy) / r
    nz = np.sqrt(np.clip(1 - nx ** 2 - ny ** 2, 0, 1))
    lon = np.arctan2(nx, nz + 1e-6); lat = np.arcsin(np.clip(ny, -1, 1))
    K = 15.0
    ridges = np.maximum(net(lon * K, lat * K, 0.085, 1.0), 0.45 * net(lon * K * 2.4, lat * K * 2.4, 0.07, 5.0))
    L = np.array([-0.55, -0.62, 0.56]); L /= np.linalg.norm(L)
    lam = np.clip(nx * L[0] + ny * L[1] + nz * L[2], 0, 1)
    suture = 1 - 0.35 * np.exp(-(np.sin(lon * 5.0) ** 2) * 60)
    base = mix(P['base0'], P['base1'], lam) * suture[..., None]
    netc = mix(P['net0'], P['net1'], lam)
    sph = mix(base, netc, ridges * (0.55 + 0.45 * lam))
    sph = mix(sph, P['rim'], (1 - nz) ** 3 * 0.8)
    sph = sph + (np.clip(lam, 0, 1) ** 40 * 0.25)[..., None] * 255
    edge = np.clip((1 - d) * r / 2.0, 0, 1)
    img = mix(img, sph, np.where(inside, edge, 0)) * dim
    out = fx.to_img(img)
    SS = 2
    lay = Image.new('RGBA', (W * SS, H * SS), (0, 0, 0, 0)); dr = ImageDraw.Draw(lay)
    for _ in range(seeds):
        x, y = rng.uniform(0, 0.62) * W, rng.uniform(0.02, 0.8) * H
        s = rng.uniform(4, 13); a = rng.uniform(0, math.pi); al = int(rng.uniform(18, 70) * dim)
        pts = [((x + s * 1.8 * math.cos(t) * math.cos(a) - s * math.sin(t) * math.sin(a)) * SS,
                (y + s * 1.8 * math.cos(t) * math.sin(a) + s * math.sin(t) * math.cos(a)) * SS) for t in np.linspace(0, 2 * math.pi, 20)]
        dr.polygon(pts, fill=P['seed'] + (al,))
    lay = lay.resize((W, H), Image.LANCZOS); out.paste(lay, (0, 0), lay)
    if wordmark:
        logo = Image.open(logo_path).convert('RGBA'); ls = int(0.2 * H); logo = logo.resize((ls, ls), Image.LANCZOS)
        lx, ly = int(0.085 * W), int(0.285 * H)
        halo = Image.new('RGBA', out.size, (0, 0, 0, 0)); hd = ImageDraw.Draw(halo)
        hd.ellipse([lx - ls * 0.15, ly - ls * 0.15, lx + ls * 1.15, ly + ls * 1.15], fill=P['halo'])
        halo = halo.filter(ImageFilter.GaussianBlur(ls * 0.2)); out.paste(halo, (0, 0), halo)
        out.paste(logo, (lx, ly), logo)
        tx = lx + ls + int(0.02 * W)
        f1 = F('NotoSans-Bold', 0.135 * H); f2 = F('NotoSans-Bold', 0.034 * H)
        fx.gradient_text(out, (tx + f1.getlength('melon') / 2, ly + ls * 0.40), 'melon', f1, (240, 248, 232), (170, 214, 150), shadow=18)
        sp = int(0.022 * H); wl = sum(f2.getlength(c) for c in 'LINUX') + sp * 4
        fx.gradient_text(out, (tx + 8 + wl / 2, ly + ls * 0.40 + 0.105 * H), 'LINUX', f2, (129, 203, 96), (111, 191, 74), spacing=sp)
    return out

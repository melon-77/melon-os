"""fx: a small HDR "studio" for melon wallpapers.
Gold material from an analytic studio environment, backdrop (guilloché, god rays), mirror floor,
bloom, anamorphic streaks, ACES tone mapping, hexagonal bokeh, typography, lens finish."""
import math
import numpy as np
from PIL import Image, ImageDraw, ImageFont

W, H = 3840, 2160
GOLD = np.array([1.0, 0.72, 0.30], np.float32)
yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)

class Fonts:
    def __init__(self, d):
        self.d = d
    def __call__(self, name, size):
        return ImageFont.truetype(f'{self.d}/{name}.ttf', int(size))

# ------------------------------------------------------------------ math helpers
def mix(a, b, t):
    t = t[..., None] if np.ndim(t) else t
    return np.asarray(a, np.float32) * (1 - t) + np.asarray(b, np.float32) * t

def _box(a, k, axis):
    if k < 1: return a
    pad = [(0, 0)] * a.ndim; pad[axis] = (k + 1, k)
    c = np.cumsum(np.pad(a, pad, mode='edge'), axis=axis, dtype=np.float32)
    n = a.shape[axis]
    return (np.take(c, np.arange(2 * k + 1, 2 * k + 1 + n), axis=axis) - np.take(c, np.arange(0, n), axis=axis)) / (2 * k + 1)

def blur(a, radius):
    k = max(1, int(round(radius * 0.87))); out = a.astype(np.float32)
    for _ in range(3): out = _box(_box(out, k, 0), k, 1)
    return out

def wide_blur(a, fx, fy):
    h, w = a.shape[:2]; chans = a if a.ndim == 3 else a[..., None]
    out = [np.asarray(Image.fromarray(chans[..., c].astype(np.float32), 'F')
                      .resize((max(1, w // fx), max(1, h // fy)), Image.BILINEAR).resize((w, h), Image.BICUBIC))
           for c in range(chans.shape[2])]
    return np.dstack(out) if a.ndim == 3 else out[0]

def aces(x):
    return np.clip((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0, 1)

def mask_from_text(text, font, center, spacing=0, size=(W, H)):
    m = Image.new('L', size, 0); d = ImageDraw.Draw(m)
    if spacing:
        ws = [font.getlength(c) for c in text]; x = center[0] - (sum(ws) + spacing * (len(text) - 1)) / 2
        for c, w in zip(text, ws): d.text((x, center[1]), c, font=font, fill=255, anchor='lm'); x += w + spacing
    else:
        d.text(center, text, font=font, fill=255, anchor='mm')
    return np.asarray(m, np.float32) / 255

def normals_from_height(h, strength):
    gy, gx = np.gradient(h * strength)
    nx, ny, nz = -gx, -gy, np.ones_like(h)
    l = np.sqrt(nx * nx + ny * ny + 1); return nx / l, ny / l, nz / l

# ------------------------------------------------------------------ the gold material
def gold(bx, by, bz, ao=1.0, gloss=0.0, fill=0.0, cool=0.35, tint=GOLD):
    """Polished gold lit by a studio: softbox above, strip lights left/right (the right one cool), floor bounce,
    optional front fill for surfaces facing the camera. gloss adds specular sparkle (ridges, bevels)."""
    Rx, Ry = 2 * bz * bx, 2 * bz * by
    warm = (0.08 + 1.7 * np.exp(-((Ry + 0.78) / 0.28) ** 2) * np.exp(-(Rx / 0.9) ** 2)
            + 1.3 * np.exp(-(((Rx + 0.72) / 0.2) ** 2 + ((Ry + 0.05) / 0.45) ** 2))
            + 0.7 * np.clip(Ry, 0, 1) ** 2
            + fill * np.exp(-((Rx + 0.15) ** 2 + (Ry + 0.25) ** 2) / 0.35))
    coolv = cool * np.exp(-(((Rx - 0.85) / 0.16) ** 2 + ((Ry - 0.05) / 0.5) ** 2))
    L = np.array([-0.5, -0.7, 0.5]); L /= np.linalg.norm(L)
    lam = np.clip(bx * L[0] + by * L[1] + bz * L[2], 0, 1)
    Hv = L + np.array([0, 0, 1.0]); Hv /= np.linalg.norm(Hv)
    spec = np.clip(bx * Hv[0] + by * Hv[1] + bz * Hv[2], 0, 1) ** 120
    fres = 0.7 + 0.3 * (1 - bz) ** 5
    col = (tint * (0.05 + 0.2 * lam)[..., None] + tint * (warm * fres * ao)[..., None]
           + np.array([0.35, 0.75, 0.80]) * tint.mean() * (coolv * fres * ao)[..., None]
           + np.array([1.0, 0.92, 0.75]) * (spec * (0.6 + 3.0 * gloss))[..., None] * 2.0)
    return col.astype(np.float32)

def over(hdr, col, alpha):
    a = alpha[..., None]; return hdr * (1 - a) + col * a

# ------------------------------------------------------------------ scene pieces
def backdrop(cx, cy, r, rays=0.42, guilloche=0.16, corona=1.1, seed=0):
    rr = np.hypot(xx - cx, yy - cy); ang = np.arctan2(yy - cy, xx - cx)
    hdr = mix([0.012, 0.009, 0.006], [0.20, 0.12, 0.04], np.exp(-rr / (0.45 * H)) ** 1.4)
    if guilloche:
        g = Image.new('L', (W, H), 0); gd = ImageDraw.Draw(g); th = np.linspace(0, 2 * np.pi, 2400)
        for i in range(46):
            ph = i * 2 * np.pi / 46
            for R0, amp, k in ((0.40 * H, 0.03 * H, 18), (0.52 * H, 0.035 * H, 26), (0.64 * H, 0.03 * H, 34)):
                rad = R0 + amp * np.sin(k * th + ph) + 0.25 * amp * np.sin(3 * k * th - ph)
                gd.line(list(zip(cx + rad * np.cos(th), cy + rad * np.sin(th))), fill=60, width=2)
        hdr += (np.asarray(g, np.float32) / 255 * np.exp(-np.clip(rr - 0.45 * H, 0, None) / (0.2 * H)))[..., None] * GOLD * guilloche
    if rays:
        ray = np.zeros_like(rr)
        for k, a, p in [(7, .5, 0), (11, .4, 1.3), (19, .3, 2.1), (31, .22, .7), (53, .15, 4.0), (89, .1, 2.6)]:
            ray += a * np.sin(ang * k + p + seed + 0.7 * np.sin(ang * 3 + p))
        ray = np.clip(ray, 0, None) ** 1.8 * np.exp(-np.clip(rr - r, 0, None) / (0.40 * H)) * (rr > r * 0.95)
        hdr += ray[..., None] * GOLD * rays
    if corona:
        hdr += (np.exp(-np.clip(rr - r, 0, None) / (0.03 * H)) * (rr > r))[..., None] * GOLD * corona
    return hdr

def mirror_floor(hdr, hz, cx, strength=0.55, pool=0.18):
    hzi = int(hz); n = H - hzi
    src = hdr[max(0, hzi - n):hzi][::-1]; refl = np.zeros((n, W, 3), np.float32); refl[:len(src)] = src
    t = np.clip(np.arange(n) / (0.2 * H), 0, 1)[:, None, None]
    refl = blur(refl, 2) * (1 - t) + blur(refl, 14) * t
    fade = np.exp(-np.arange(n) / (0.13 * H))[:, None, None]
    pl = np.exp(-(((xx[hzi:] - cx) / (0.30 * W)) ** 2 + ((yy[hzi:] - hz) / (0.07 * H)) ** 2))
    hdr[hzi:] = np.array([0.006, 0.005, 0.004]) + refl * fade * strength + pl[..., None] * GOLD * pool
    hdr[hzi - 1:hzi + 2] += np.exp(-np.abs(xx[0] - cx) / (0.28 * W))[None, :, None] * GOLD * 0.9
    return hdr

def develop(hdr, exposure=1.0, bloom=1.0, streak=1.0, sat=1.18):
    bright = np.clip(hdr - 0.85, 0, None)
    hdr = hdr + (blur(bright, 6) * 0.45 + blur(bright, 24) * 0.35 + wide_blur(bright, 32, 32) * 0.6) * bloom
    hdr = hdr + wide_blur(bright, 96, 1) * np.array([0.9, 0.85, 1.0]) * 1.4 * streak
    ldr = aces(hdr * exposure) * 255
    lum = ldr.mean(axis=2, keepdims=True); return lum + (ldr - lum) * sat

def bokeh(img, rs, avoid=None, top=H, layers=((80, 3, 9, 1.5, 170), (16, 26, 70, 7, 38))):
    from PIL import ImageFilter
    for count, smin, smax, bl, amax in layers:
        lay = Image.new('RGBA', (W, H), (0, 0, 0, 0)); bd = ImageDraw.Draw(lay)
        for _ in range(count):
            x, y = rs.uniform(0, W), rs.uniform(0, top); s = rs.uniform(smin, smax)
            if avoid and math.hypot(x - avoid[0], y - avoid[1]) < avoid[2]: continue
            rot = rs.uniform(0, math.pi / 3)
            hexp = [(x + s * math.cos(rot + i * math.pi / 3), y + s * math.sin(rot + i * math.pi / 3)) for i in range(6)]
            g = int(rs.uniform(170, 215)); al = int(rs.uniform(25, amax))
            bd.polygon(hexp, fill=(255, g, 110, al), outline=(255, g + 30, 170, min(255, int(al * 1.8))))
        lay = lay.filter(ImageFilter.GaussianBlur(bl)); img.paste(lay, (0, 0), lay)
    return img

def gradient_text(img, xy, text, font, top, bottom, spacing=0, shadow=None):
    from PIL import ImageFilter
    m = mask_from_text(text, font, xy, spacing, img.size)
    mask = Image.fromarray((m * 255).astype(np.uint8))
    box = mask.getbbox()
    if not box: return
    if shadow:
        sh = mask.filter(ImageFilter.GaussianBlur(shadow))
        img.paste(Image.new('RGB', img.size, (0, 0, 0)), (0, int(shadow * 0.6)), sh.point(lambda p: int(p * 0.9)))
    t = np.clip((np.arange(img.size[1]) - box[1]) / max(1, box[3] - box[1]), 0, 1)
    col = mix(top, bottom, t)[:, None, :].repeat(img.size[0], 1)
    img.paste(Image.fromarray(np.clip(col, 0, 255).astype(np.uint8)), (0, 0), mask)

def titles(img, F, title, subtitle, y=0.845):
    cx = W / 2
    gradient_text(img, (cx, y * H), title, F('NotoSerifDisplay-Regular', 0.062 * H), (255, 247, 214), (201, 140, 48),
                  spacing=int(0.03 * H), shadow=12)
    sub = F('NotoSerifDisplay-Italic', 0.024 * H)
    gradient_text(img, (cx, (y + 0.06) * H), subtitle, sub, (214, 178, 112), (170, 132, 70), spacing=int(0.004 * H))
    sw = sub.getlength(subtitle) + len(subtitle) * 0.004 * H
    hl = Image.new('RGBA', (W, H), (0, 0, 0, 0)); hd = ImageDraw.Draw(hl); yl = (y + 0.06) * H
    for s in (-1, 1):
        x0 = cx + s * (sw / 2 + 0.02 * W); x1 = x0 + s * 0.09 * W
        hd.line([(x0, yl), (x1, yl)], fill=(214, 170, 90, 200), width=2)
        hd.polygon([(x0 + s * 6, yl), (x0 + s * 16, yl - 6), (x0 + s * 26, yl), (x0 + s * 16, yl + 6)], fill=(236, 196, 110, 230))
    img.paste(hl, (0, 0), hl)

def brand(img, F, logo_path):
    logo = Image.open(logo_path).convert('RGBA'); ls = int(0.07 * H); logo = logo.resize((ls, ls), Image.LANCZOS)
    img.paste(logo, (int(0.035 * W), int(0.05 * H)), logo)
    gradient_text(img, (int(0.035 * W) + ls + 26 + F('NotoSerifDisplay-Regular', 0.04 * H).getlength('melon') / 2, int(0.05 * H) + ls / 2),
                  'melon', F('NotoSerifDisplay-Regular', 0.04 * H), (242, 228, 192), (198, 166, 104))

def lens(img, rs):
    arr = np.asarray(img, np.float32)
    im = Image.fromarray(arr[..., 0], 'F'); w2, h2 = int(W * 1.0016), int(H * 1.0016)
    im = im.resize((w2, h2), Image.BICUBIC); l, t = (w2 - W) // 2, (h2 - H) // 2
    arr = np.dstack([np.asarray(im.crop((l, t, l + W, t + H))), arr[..., 1], arr[..., 2]])
    vig = 1 - 0.6 * np.clip(np.hypot((xx - W / 2) / (W * 0.6), (yy - H * 0.45) / (H * 0.68)) - 0.38, 0, 1) ** 1.5
    arr = arr * vig[..., None] + rs.normal(0, 2.0, (H, W, 1))
    return Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8))

def to_img(a):
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))

def save(img, out_dir, name):
    import os
    d = f'{out_dir}/{name}/contents/images'; os.makedirs(d, exist_ok=True)
    for w, h in ((3840, 2160), (2560, 1440), (1920, 1080)):
        img.resize((w, h), Image.LANCZOS).save(f'{d}/{w}x{h}.jpg', quality=92, optimize=True, progressive=True)
    img.resize((400, 225), Image.LANCZOS).save(f'{out_dir}/{name}/contents/screenshot.jpg', quality=88)

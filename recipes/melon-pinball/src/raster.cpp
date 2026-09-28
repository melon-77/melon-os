#include "raster.hpp"

#include <algorithm>

namespace pb {

void Canvas::clear(RGBA c) {
  for (size_t i = 0; i < px.size(); i += 4) {
    px[i] = c.r * c.a; px[i + 1] = c.g * c.a; px[i + 2] = c.b * c.a; px[i + 3] = c.a;
  }
}

void Canvas::fillPoly(const std::vector<V2> &pts, const Shader &sh) {
  if (pts.size() < 3) return;
  double y0 = 1e9, y1 = -1e9;
  for (auto &p : pts) { y0 = std::min(y0, p.y); y1 = std::max(y1, p.y); }
  int iy0 = std::max(0, (int)std::floor(y0)), iy1 = std::min(h - 1, (int)std::ceil(y1));
  std::vector<double> xs;
  std::vector<float> cov(w);
  // two sub-scanlines per row with exact span ends, accumulated per pixel: smooth edges before the 2x downsample
  for (int y = iy0; y <= iy1; y++) {
    int lo = w, hi = -1;
    for (int sub = 0; sub < 2; sub++) {
      double sy = y + 0.25 + 0.5 * sub;
      xs.clear();
      size_t n = pts.size();
      for (size_t i = 0; i < n; i++) {
        V2 a = pts[i], b = pts[(i + 1) % n];
        if ((a.y <= sy && b.y > sy) || (b.y <= sy && a.y > sy)) xs.push_back(a.x + (sy - a.y) / (b.y - a.y) * (b.x - a.x));
      }
      std::sort(xs.begin(), xs.end());
      for (size_t i = 0; i + 1 < xs.size(); i += 2) {
        double xa = std::max(0.0, xs[i]), xb = std::min((double)w, xs[i + 1]);
        if (xb <= xa) continue;
        int ia = (int)std::floor(xa), ib = std::min(w - 1, (int)std::floor(xb - 1e-9));
        for (int x = ia; x <= ib; x++) {
          double c = std::min(xb, x + 1.0) - std::max(xa, (double)x);
          if (c > 0) cov[x] += (float)c * 0.5f;
        }
        lo = std::min(lo, ia);
        hi = std::max(hi, ib);
      }
    }
    for (int x = lo; x <= hi; x++) {
      if (cov[x] > 0) blend(x, y, sh(x + 0.5f, y + 0.5f), std::min(1.f, cov[x]));
      cov[x] = 0;
    }
  }
}

// Coverage-weighted shading of an implicit shape: `dist` < 0 inside, in pixels.
template <class D>
static void fillSdf(Canvas &cv, double x0, double y0, double x1, double y1, D dist, const Shader &sh) {
  int ix0 = std::max(0, (int)std::floor(x0) - 1), iy0 = std::max(0, (int)std::floor(y0) - 1);
  int ix1 = std::min(cv.w - 1, (int)std::ceil(x1) + 1), iy1 = std::min(cv.h - 1, (int)std::ceil(y1) + 1);
  for (int y = iy0; y <= iy1; y++)
    for (int x = ix0; x <= ix1; x++) {
      float d = (float)dist(x + 0.5, y + 0.5);
      float cov = clamp01(0.5f - d);
      if (cov > 0) cv.blend(x, y, sh(x + 0.5f, y + 0.5f), cov);
    }
}

void Canvas::fillCircle(V2 c, double r, const Shader &sh) {
  fillSdf(*this, c.x - r, c.y - r, c.x + r, c.y + r,
          [&](double x, double y) { return std::sqrt((x - c.x) * (x - c.x) + (y - c.y) * (y - c.y)) - r; }, sh);
}

void Canvas::fillCapsule(V2 a, V2 b, double r, const Shader &sh) {
  fillSdf(*this, std::min(a.x, b.x) - r, std::min(a.y, b.y) - r, std::max(a.x, b.x) + r, std::max(a.y, b.y) + r,
          [&](double x, double y) { return len(V2(x, y) - closestOnSeg(V2(x, y), a, b)) - r; }, sh);
}

void Canvas::ring(V2 c, double r0, double r1, RGBA col) {
  fillSdf(*this, c.x - r1, c.y - r1, c.x + r1, c.y + r1,
          [&](double x, double y) {
            double d = std::sqrt((x - c.x) * (x - c.x) + (y - c.y) * (y - c.y));
            return std::max(r0 - d, d - r1);
          },
          [col](float, float) { return col; });
}

void Canvas::glow(V2 c, double r, RGBA col) {
  int ix0 = std::max(0, (int)(c.x - r)), iy0 = std::max(0, (int)(c.y - r));
  int ix1 = std::min(w - 1, (int)(c.x + r)), iy1 = std::min(h - 1, (int)(c.y + r));
  for (int y = iy0; y <= iy1; y++)
    for (int x = ix0; x <= ix1; x++) {
      double d = std::sqrt((x + 0.5 - c.x) * (x + 0.5 - c.x) + (y + 0.5 - c.y) * (y + 0.5 - c.y)) / r;
      if (d >= 1) continue;
      float k = (float)((1 - d) * (1 - d));
      add(x, y, RGBA(col.r, col.g, col.b, col.a * k));
    }
}

void Canvas::composite(const Canvas &src, int dx, int dy, float op) {
  for (int y = 0; y < src.h; y++) {
    int ty = y + dy;
    if (ty < 0 || ty >= h) continue;
    for (int x = 0; x < src.w; x++) {
      int tx = x + dx;
      if (tx < 0 || tx >= w) continue;
      const float *s = &src.px[((size_t)y * src.w + x) * 4];
      float a = s[3] * op;
      if (a <= 0) continue;
      float *p = &px[((size_t)ty * w + tx) * 4];
      float ia = 1 - a;
      p[0] = s[0] * op + p[0] * ia;
      p[1] = s[1] * op + p[1] * ia;
      p[2] = s[2] * op + p[2] * ia;
      p[3] = a + p[3] * ia;
    }
  }
}

static void boxPass(std::vector<float> &a, std::vector<float> &tmp, int w, int h, int r, bool horiz) {
  int n = horiz ? w : h, lines = horiz ? h : w;
  float inv = 1.f / (2 * r + 1);
  for (int l = 0; l < lines; l++) {
    for (int c = 0; c < 4; c++) {
      auto at = [&](int i) -> float & {
        i = std::clamp(i, 0, n - 1);
        return horiz ? a[((size_t)l * w + i) * 4 + c] : a[((size_t)i * w + l) * 4 + c];
      };
      float acc = 0;
      for (int i = -r; i <= r; i++) acc += at(i);
      for (int i = 0; i < n; i++) {
        tmp[i] = acc * inv;
        acc += at(i + r + 1) - at(i - r);
      }
      for (int i = 0; i < n; i++) at(i) = tmp[i];
    }
  }
}

void Canvas::blur(int r) {
  if (r <= 0) return;
  std::vector<float> tmp(std::max(w, h));
  for (int k = 0; k < 3; k++) {
    boxPass(px, tmp, w, h, r, true);
    boxPass(px, tmp, w, h, r, false);
  }
}

void Canvas::blitSurface(SDL_Surface *s, int dx, int dy, RGBA tint, bool center) {
  if (!s) return;
  SDL_Surface *c = SDL_ConvertSurface(s, SDL_PIXELFORMAT_ARGB8888);
  if (!c) return;
  if (center) { dx -= c->w / 2; dy -= c->h / 2; }
  for (int y = 0; y < c->h; y++) {
    const Uint32 *row = (const Uint32 *)((const Uint8 *)c->pixels + (size_t)y * c->pitch);
    for (int x = 0; x < c->w; x++) {
      Uint32 v = row[x];
      float a = ((v >> 24) & 255) / 255.f;
      if (a <= 0) continue;
      RGBA col(((v >> 16) & 255) / 255.f * tint.r, ((v >> 8) & 255) / 255.f * tint.g, (v & 255) / 255.f * tint.b, tint.a);
      blend(dx + x, dy + y, col, a);
    }
  }
  SDL_DestroySurface(c);
}

void Canvas::blitRotated(SDL_Surface *s, V2 c, double ang, double sc, RGBA tint) {
  if (!s) return;
  SDL_Surface *cs = SDL_ConvertSurface(s, SDL_PIXELFORMAT_ARGB8888);
  if (!cs) return;
  double hw = cs->w * sc / 2, hh = cs->h * sc / 2, R = std::sqrt(hw * hw + hh * hh) + 2;
  double ca = std::cos(ang), sa = std::sin(ang);
  auto alphaAt = [&](int x, int y) -> float {
    if (x < 0 || y < 0 || x >= cs->w || y >= cs->h) return 0;
    Uint32 v = ((const Uint32 *)((const Uint8 *)cs->pixels + (size_t)y * cs->pitch))[x];
    return ((v >> 24) & 255) / 255.f;
  };
  for (int y = (int)(c.y - R); y <= (int)(c.y + R); y++)
    for (int x = (int)(c.x - R); x <= (int)(c.x + R); x++) {
      double dx = x + 0.5 - c.x, dy = y + 0.5 - c.y;
      double u = (dx * ca + dy * sa) / sc + cs->w / 2.0 - 0.5, v = (-dx * sa + dy * ca) / sc + cs->h / 2.0 - 0.5;
      int iu = (int)std::floor(u), iv = (int)std::floor(v);
      float fu = (float)(u - iu), fv = (float)(v - iv);
      float a = alphaAt(iu, iv) * (1 - fu) * (1 - fv) + alphaAt(iu + 1, iv) * fu * (1 - fv) +
                alphaAt(iu, iv + 1) * (1 - fu) * fv + alphaAt(iu + 1, iv + 1) * fu * fv;
      if (a > 0) blend(x, y, tint, a);
    }
  SDL_DestroySurface(cs);
}

Canvas Canvas::downsample2() const {
  Canvas o(w / 2, h / 2);
  for (int y = 0; y < o.h; y++)
    for (int x = 0; x < o.w; x++)
      for (int c = 0; c < 4; c++) {
        size_t i0 = ((size_t)(2 * y) * w + 2 * x) * 4 + c, i1 = ((size_t)(2 * y + 1) * w + 2 * x) * 4 + c;
        o.px[((size_t)y * o.w + x) * 4 + c] = 0.25f * (px[i0] + px[i0 + 4] + px[i1] + px[i1 + 4]);
      }
  return o;
}

SDL_Surface *Canvas::toSurface() const {
  SDL_Surface *s = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_ARGB8888);
  if (!s) return nullptr;
  for (int y = 0; y < h; y++) {
    Uint32 *row = (Uint32 *)((Uint8 *)s->pixels + (size_t)y * s->pitch);
    for (int x = 0; x < w; x++) {
      const float *p = &px[((size_t)y * w + x) * 4];
      float a = clamp01(p[3]);
      float ia = a > 1e-5f ? 1.f / a : 0;
      auto ch = [&](float v) { return (Uint32)(clamp01(v * ia) * 255.f + 0.5f); };
      row[x] = ((Uint32)(a * 255.f + 0.5f) << 24) | (ch(p[0]) << 16) | (ch(p[1]) << 8) | ch(p[2]);
    }
  }
  return s;
}

float hash2(float i, float j, float s) {
  float v = std::sin(i * 127.1f + j * 311.7f + s) * 43758.5453f;
  return v - std::floor(v);
}

float noise2(float x, float y) {
  float ix = std::floor(x), iy = std::floor(y), fx = x - ix, fy = y - iy;
  float a = hash2(ix, iy, 0), b = hash2(ix + 1, iy, 0), c = hash2(ix, iy + 1, 0), d = hash2(ix + 1, iy + 1, 0);
  fx = fx * fx * (3 - 2 * fx);
  fy = fy * fy * (3 - 2 * fy);
  return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
}

float melonNet(float u, float v, float width, float seed) {
  u += 0.18f * std::sin(v * 1.7f + seed);
  v += 0.18f * std::sin(u * 1.3f - seed);
  float iu = std::floor(u), iv = std::floor(v);
  float f1 = 9, f2 = 9;
  for (int di = -1; di <= 1; di++)
    for (int dj = -1; dj <= 1; dj++) {
      float ci = iu + di, cj = iv + dj;
      float px = 0.15f + 0.7f * hash2(ci, cj, seed), py = 0.15f + 0.7f * hash2(ci, cj, seed + 17.3f);
      float d = std::sqrt((ci + px - u) * (ci + px - u) + (cj + py - v) * (cj + py - v));
      if (d < f1) { f2 = f1; f1 = d; } else if (d < f2) f2 = d;
    }
  return std::pow(clamp01(1 - (f2 - f1) / width), 1.6f);
}

}  // namespace pb

// A small software painter for the table art: float RGBA canvases, filled shapes with per-pixel
// shading callbacks, soft shadows, and the cantaloupe netting from melon's own artwork (art/planet.py).
#pragma once
#include <SDL3/SDL.h>

#include <cmath>
#include <functional>
#include <vector>

#include "physics.hpp"

namespace pb {

struct RGBA {
  float r = 0, g = 0, b = 0, a = 0;
  RGBA() = default;
  RGBA(float r_, float g_, float b_, float a_ = 1) : r(r_), g(g_), b(b_), a(a_) {}
};
inline RGBA hex(unsigned c, float a = 1) { return {((c >> 16) & 255) / 255.f, ((c >> 8) & 255) / 255.f, (c & 255) / 255.f, a}; }
inline RGBA mixc(RGBA x, RGBA y, float t) {
  return {x.r + (y.r - x.r) * t, x.g + (y.g - x.g) * t, x.b + (y.b - x.b) * t, x.a + (y.a - x.a) * t};
}
inline RGBA scale(RGBA c, float s) { return {c.r * s, c.g * s, c.b * s, c.a}; }
inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
inline float smooth(float e0, float e1, float x) { float t = clamp01((x - e0) / (e1 - e0)); return t * t * (3 - 2 * t); }

// shading callback: pixel position in canvas pixels -> colour (straight alpha)
using Shader = std::function<RGBA(float x, float y)>;

class Canvas {
public:
  int w = 0, h = 0;
  std::vector<float> px;    // premultiplied RGBA

  Canvas() = default;
  Canvas(int w_, int h_) : w(w_), h(h_), px((size_t)w_ * h_ * 4, 0.f) {}
  void clear(RGBA c = {});

  // blend a straight-alpha colour with coverage `cov` onto pixel (x, y)
  void blend(int x, int y, RGBA c, float cov = 1) {
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    float a = c.a * cov;
    if (a <= 0) return;
    float *p = &px[((size_t)y * w + x) * 4];
    float ia = 1 - a;
    p[0] = c.r * a + p[0] * ia;
    p[1] = c.g * a + p[1] * ia;
    p[2] = c.b * a + p[2] * ia;
    p[3] = a + p[3] * ia;
  }
  void add(int x, int y, RGBA c) {           // additive light (glows)
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    float *p = &px[((size_t)y * w + x) * 4];
    p[0] += c.r * c.a; p[1] += c.g * c.a; p[2] += c.b * c.a;
  }

  // polygon (even-odd) in canvas pixels, with a shader or a flat colour
  void fillPoly(const std::vector<V2> &pts, const Shader &sh);
  void fillPoly(const std::vector<V2> &pts, RGBA c) { fillPoly(pts, [c](float, float) { return c; }); }
  // shader over a disc / capsule / ring (anti-aliased by a 1 px smooth edge)
  void fillCircle(V2 c, double r, const Shader &sh);
  void fillCircle(V2 c, double r, RGBA col) { fillCircle(c, r, [col](float, float) { return col; }); }
  void fillCapsule(V2 a, V2 b, double r, const Shader &sh);
  void fillCapsule(V2 a, V2 b, double r, RGBA col) { fillCapsule(a, b, r, [col](float, float) { return col; }); }
  void ring(V2 c, double r0, double r1, RGBA col);
  void glow(V2 c, double r, RGBA col);      // additive radial glow
  void composite(const Canvas &src, int dx, int dy, float opacity = 1);   // src over this
  void blur(int radius);                     // box blur x3 (approximately gaussian)
  void blitSurface(SDL_Surface *s, int dx, int dy, RGBA tint, bool center = true);  // text etc. (alpha from s)
  // centred at c, rotated by `ang` radians and scaled, bilinear (for labels along lanes)
  void blitRotated(SDL_Surface *s, V2 c, double ang, double scale, RGBA tint);

  Canvas downsample2() const;
  SDL_Surface *toSurface() const;            // ARGB8888, straight alpha
};

// cantaloupe netting: 1 on the net ridges, 0 in the cells (Worley F2-F1 edges)
float melonNet(float u, float v, float width, float seed = 0);
float hash2(float i, float j, float s);
float noise2(float x, float y);                // value noise 0..1

}  // namespace pb

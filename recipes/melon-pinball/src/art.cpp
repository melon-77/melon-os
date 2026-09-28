#include "art.hpp"

#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <string>

#include "raster.hpp"

namespace pb {

// ---------------------------------------------------------------- palette (melon's own: MelonDark, the wallpapers)
static const RGBA kSpace0 = hex(0x0a0e0c), kSpace1 = hex(0x14261a);
static const RGBA kGreen = hex(0x6fbf4a), kOrange = hex(0xf0a35e), kCream = hex(0xece2b4), kGold = hex(0xffd24a);
static const RGBA kRind0 = hex(0x16301a), kRind1 = hex(0x3a682e), kNet0 = hex(0x3c6034), kNet1 = hex(0xc4e896);

// A painter maps table metres to canvas pixels (with an optional origin for small sprite canvases).
struct Painter {
  Canvas &cv;
  double S;                 // px per metre on this canvas
  V2 org;                   // canvas pixel (0,0) is at this table position (m)
  V2 P(V2 m, double z = 0) const { return V2((m.x - org.x) * S, (m.y - org.y - z * kHeightK) * S); }
  double L(double m) const { return m * S; }
  V2 toM(float x, float y, double z = 0) const { return V2(x / S + org.x, y / S + org.y + z * kHeightK); }
};

static std::vector<V2> mapPts(const Painter &p, const std::vector<V2> &pts, double z) {
  std::vector<V2> o;
  for (auto &q : pts) o.push_back(p.P(q, z));
  return o;
}

static double signedArea(const std::vector<V2> &pts) {
  double a = 0;
  for (size_t i = 0; i < pts.size(); i++) a += cross(pts[i], pts[(i + 1) % pts.size()]);
  return a / 2;
}

// light comes from the top left, a little in front
static const V2 kLight = norm(V2(-0.55, -0.83));

// A raised polygon: sides stacked from the floor to height z, then the top, with lit and shaded edges.
static void extrudePoly(const Painter &p, const std::vector<V2> &pts, double z, RGBA side, const Shader &top) {
  int steps = std::max(1, (int)(p.L(z * kHeightK)));
  for (int k = 0; k < steps; k++) {
    double f = (double)k / steps;
    p.cv.fillPoly(mapPts(p, pts, z * f), scale(side, (float)(0.55 + 0.45 * f)));
  }
  auto tp = mapPts(p, pts, z);
  p.cv.fillPoly(tp, top);
  double sgn = signedArea(pts) > 0 ? 1 : -1;
  for (size_t i = 0; i < pts.size(); i++) {
    V2 a = pts[i], b = pts[(i + 1) % pts.size()];
    V2 n = perp(norm(b - a)) * -sgn;              // outward normal (y down)
    float l = (float)dot(n, kLight);
    RGBA c = l > 0 ? RGBA(1, 1, 0.9f, 0.45f * l) : RGBA(0, 0, 0, -0.5f * l);
    p.cv.fillCapsule(p.P(a, z), p.P(b, z), std::max(0.8, p.L(0.0006)), c);
  }
}

static void extrudeCircle(const Painter &p, V2 c, double r, double z, RGBA side, const Shader &top) {
  int steps = std::max(1, (int)(p.L(z * kHeightK)));
  for (int k = 0; k < steps; k++) {
    double f = (double)k / steps;
    p.cv.fillCircle(p.P(c, z * f), p.L(r), scale(side, (float)(0.5 + 0.5 * f)));
  }
  p.cv.fillCircle(p.P(c, z), p.L(r), top);
}

static void extrudeCapsule(const Painter &p, V2 a, V2 b, double r, double z, RGBA side, const Shader &top) {
  int steps = std::max(1, (int)(p.L(z * kHeightK)));
  for (int k = 0; k < steps; k++) {
    double f = (double)k / steps;
    p.cv.fillCapsule(p.P(a, z * f), p.P(b, z * f), p.L(r), scale(side, (float)(0.5 + 0.5 * f)));
  }
  p.cv.fillCapsule(p.P(a, z), p.P(b, z), p.L(r), top);
}

// chrome: bright along the middle of a rail, dark at its edges, a green reflection of the table below
static Shader chrome(const Painter &p, V2 a, V2 b, double r, double z) {
  V2 A = p.P(a, z), B = p.P(b, z);
  double R = p.L(r);
  return [A, B, R](float x, float y) {
    V2 q(x, y);
    V2 c = closestOnSeg(q, A, B);
    V2 d = q - c;
    double t = R > 0 ? std::clamp(len(d) / R, 0.0, 1.0) : 0;
    double side = dot(norm(d), kLight);
    float k = (float)(0.35 + 0.65 * (1 - t * t) + 0.25 * -side * t);
    RGBA base = mixc(hex(0x2c3a30), hex(0xe8f0e8), clamp01(k));
    if (t > 0.7 && side > 0) base = mixc(base, hex(0x6fbf4a), 0.25f);
    return base;
  };
}

static Shader sphere(const Painter &p, V2 c, double r, double z, RGBA dark, RGBA light) {
  V2 C = p.P(c, z);
  double R = p.L(r);
  return [C, R, dark, light](float x, float y) {
    double nx = (x - C.x) / R, ny = (y - C.y) / R;
    double nz = std::sqrt(std::max(0.0, 1 - nx * nx - ny * ny));
    double l = std::clamp(-0.5 * nx - 0.6 * ny + 0.62 * nz, 0.0, 1.0);
    RGBA col = mixc(dark, light, (float)l);
    double sp = std::pow(std::max(0.0, -0.45 * nx - 0.55 * ny + 0.7 * nz), 24);
    return mixc(col, RGBA(1, 1, 1, 1), (float)sp * 0.8f);
  };
}

// ---------------------------------------------------------------- the playfield floor
static RGBA floorAt(double x, double y, const Table &t, bool golden) {
  // deep space, greener towards the player
  float gy = (float)std::clamp(y / 1.066, 0.0, 1.0);
  RGBA c = mixc(kSpace0, hex(0x1b3524), 0.25f + 0.75f * gy);
  // nebulae: soft clouds of green and melon orange
  {
    float n = 0, amp = 0.5f, fq = 5.0f;
    for (int o = 0; o < 5; o++) { n += amp * noise2((float)(x * fq) + 3.1f, (float)(y * fq) - 1.7f); amp *= 0.5f; fq *= 2.1f; }
    float m = 0, amp2 = 0.5f, fq2 = 4.0f;
    for (int o = 0; o < 4; o++) { m += amp2 * noise2((float)(x * fq2) - 7.3f, (float)(y * fq2) + 5.2f); amp2 *= 0.5f; fq2 *= 2.3f; }
    c = mixc(c, hex(0x2f6b3a), smooth(0.45f, 0.85f, n) * 0.55f);
    c = mixc(c, hex(0x7a4a26), smooth(0.55f, 0.9f, m) * 0.35f);
    c = mixc(c, hex(0x9be07a), smooth(0.72f, 0.95f, n) * 0.25f);
  }
  // the faint cantaloupe net over everything, and a soft noise
  float net = melonNet((float)(x / 0.034), (float)(y / 0.034), 0.05f, 3.1f);
  c = mixc(c, kGreen, net * 0.07f);
  c = scale(c, 0.92f + 0.16f * noise2((float)(x * 90), (float)(y * 90)));
  // seeds for stars
  {
    const double cell = 0.011;
    double ci = std::floor(x / cell), cj = std::floor(y / cell);
    for (int di = -1; di <= 1; di++)
      for (int dj = -1; dj <= 1; dj++) {
        float i = (float)(ci + di), j = (float)(cj + dj);
        if (hash2(i, j, 9.1f) > 0.33f) continue;
        double sx = (i + 0.2 + 0.6 * hash2(i, j, 1.3f)) * cell, sy = (j + 0.2 + 0.6 * hash2(i, j, 7.7f)) * cell;
        double ang = hash2(i, j, 4.4f) * kPi;
        double rr = 0.0005 + 0.0011 * hash2(i, j, 2.2f);
        V2 d = rot(V2(x - sx, y - sy), -ang);
        double e = (d.x * d.x) / (rr * rr * 2.2) + (d.y * d.y) / (rr * rr * 0.8);
        if (e < 1.6) {
          float k = (float)std::clamp(1.6 - e, 0.0, 1.0) * (0.35f + 0.65f * hash2(i, j, 5.5f));
          c = mixc(c, kCream, k);
        }
      }
  }
  // the melon planet rising behind the flippers (the wallpaper's planet)
  {
    const double cx = 0.235, cy = 1.30, r = 0.42;
    double nx = (x - cx) / r, ny = (y - cy) / r, d = std::sqrt(nx * nx + ny * ny);
    if (d < 1) {
      double nz = std::sqrt(1 - d * d);
      double lon = std::atan2(nx, nz + 1e-6), lat = std::asin(std::clamp(ny, -1.0, 1.0));
      float n = melonNet((float)(lon * 5.5), (float)(lat * 5.5), 0.06f, 1.7f);
      float light = clamp01((float)(0.55 - 0.45 * nx - 0.35 * ny + 0.4 * nz));
      RGBA base = mixc(kRind0, kRind1, light);
      RGBA netc = mixc(kNet0, kNet1, light);
      RGBA pc = mixc(base, netc, n);
      pc = mixc(pc, hex(0x8cd772), (float)std::pow(d, 6) * 0.6f);   // bright limb
      c = mixc(c, pc, 0.85f);
    } else {
      float atm = (float)std::exp(-(d - 1) * 16);
      c = mixc(c, kGreen, atm * 0.55f);
    }
  }
  // the gauntlet portal: golden net pulled into the hole (the survivor wallpaper)
  {
    V2 q = V2(x, y) - t.portal;
    double r = len(q);
    if (r < 0.105) {
      double a = std::atan2(q.y, q.x);
      float n = melonNet((float)(a * 3.4), (float)(std::log(std::max(r, 0.004)) * 5.5), 0.07f, 8.2f);
      float fall = (float)std::clamp(1 - r / 0.105, 0.0, 1.0);
      c = mixc(c, kGold, n * fall * 0.75f);
      c = mixc(c, hex(0xfff2c0), (float)std::pow(fall, 5) * 0.5f);
    }
  }
  // lanes: the shooter lane and the loop are darker grooves with a centre line
  if (x > 0.470) {
    c = scale(c, 0.7f);
    if (std::fabs(x - 0.487) < 0.0012 && y > 0.33) c = mixc(c, kOrange, 0.25f);
  }
  if (x < 0.050 && y > 0.33 && y < 0.58) c = scale(c, 0.78f);
  if (golden) c = mixc(c, kGold, 0.03f);
  return c;
}

static void text(Canvas &cv, Text &tx, const Painter &p, const std::string &s, V2 at, double sizeM, double ang, RGBA col,
                 FontFace face = FONT_SANS) {
  float px = (float)p.L(sizeM);
  SDL_Surface *surf = tx.render(s, std::max(6.f, px), face);
  if (!surf) return;
  // the font is rendered at its pixel size; scale to the wanted height
  double sc = px / std::max(1.0, (double)surf->h) * 1.25;
  cv.blitRotated(surf, p.P(at), ang, sc, col);
  SDL_DestroySurface(surf);
}

// ---------------------------------------------------------------- lamps
static void drawLamp(const Painter &p, Text &tx, const Lamp &l, int idx, bool lit) {
  RGBA col = hex(l.color);
  RGBA off = scale(col, 0.34f);
  off.a = 1;
  Canvas &cv = p.cv;
  if (idx == LA_PORTAL) {                           // a ring around the saucer
    RGBA ringc = lit ? mixc(kGold, RGBA(1, 1, 1), 0.3f) : scale(kGold, 0.28f);
    ringc.a = 1;
    cv.ring(p.P(l.p), p.L(0.024), p.L(0.031), ringc);
    return;
  }
  auto insert = [&](float x, float y, V2 c, double r) {
    V2 m = p.toM(x, y);
    double d = std::clamp(len(m - c) / r, 0.0, 1.0);
    if (!lit) {
      RGBA k = mixc(off, scale(off, 0.55f), (float)d);
      // a faint reflection on the plastic
      double hl = std::max(0.0, 1 - len(m - (c + V2(-0.3, -0.35) * r)) / (0.45 * r));
      return mixc(k, RGBA(1, 1, 1), (float)hl * 0.12f);
    }
    RGBA hot = mixc(col, RGBA(1, 1, 0.92f), 0.65f);
    return mixc(hot, col, (float)smooth(0.1f, 1.0f, (float)d));
  };
  RGBA labelCol = lit ? RGBA(0.07f, 0.09f, 0.07f, 0.85f) : RGBA(col.r * 0.55f, col.g * 0.55f, col.b * 0.55f, 0.9f);
  switch (l.shape) {
    case L_ROUND: {
      V2 c = l.p;
      cv.fillCircle(p.P(c), p.L(l.size) + 1.2, RGBA(0, 0, 0, 0.6f));
      cv.fillCircle(p.P(c), p.L(l.size), [&](float x, float y) { return insert(x, y, c, l.size); });
      if (!l.label.empty()) text(cv, tx, p, l.label, c + V2(0, l.size * 0.05), l.size * (l.label.size() > 1 ? 0.8 : 1.15), 0, labelCol);
      break;
    }
    case L_ARROW: {
      const double sh[][2] = {{-1, -0.42}, {0.15, -0.42}, {0.15, -0.9}, {1, 0}, {0.15, 0.9}, {0.15, 0.42}, {-1, 0.42}};
      std::vector<V2> pts, pout;
      for (auto &q : sh) {
        pts.push_back(l.p + rot(V2(q[0], q[1]) * l.size, l.angle));
        pout.push_back(l.p + rot(V2(q[0] * 1.12 + 0.02, q[1] * 1.18) * l.size, l.angle));
      }
      cv.fillPoly(mapPts(p, pout, 0), RGBA(0, 0, 0, 0.6f));
      cv.fillPoly(mapPts(p, pts, 0), [&](float x, float y) { return insert(x, y, l.p + rot(V2(0.3, 0), l.angle) * l.size, l.size); });
      break;
    }
    case L_RECT: {
      double hw = l.size * (0.9 + 0.62 * l.label.size());
      V2 a = l.p - V2(hw - l.size, 0), b = l.p + V2(hw - l.size, 0);
      cv.fillCapsule(p.P(a), p.P(b), p.L(l.size) + 1.2, RGBA(0, 0, 0, 0.6f));
      cv.fillCapsule(p.P(a), p.P(b), p.L(l.size), [&](float x, float y) {
        V2 m = p.toM(x, y);
        V2 c = closestOnSeg(m, a, b);
        return insert(p.P(c).x + (float)(x - p.P(c).x), p.P(c).y + (float)(y - p.P(c).y), c, l.size);
      });
      text(cv, tx, p, l.label, l.p + V2(0, l.size * 0.05), l.size * 1.0, 0, labelCol);
      break;
    }
  }
}

// ---------------------------------------------------------------- raised parts
static void drawBumper(const Painter &p, V2 c, double r, bool lit, bool capOnly) {
  const double z = 0.032;
  if (!capOnly) {
    p.cv.fillCircle(p.P(c), p.L(r * 1.32), [&](float x, float y) {   // the metal skirt on the floor
      V2 m = p.toM(x, y);
      float d = (float)(len(m - c) / (r * 1.32));
      return mixc(hex(0x6a7a70), hex(0x1c2620), d);
    });
    // the body: melon flesh coloured plastic under the cap
    extrudeCircle(p, c, r * 0.93, z, hex(0xe0873a), [](float, float) { return hex(0xf0a35e); });
  }
  // the cap: a small cantaloupe
  V2 C = p.P(c, z);
  double R = p.L(r);
  p.cv.fillCircle(C, R, [&, C, R](float x, float y) {
    double nx = (x - C.x) / R, ny = (y - C.y) / R;
    double nz = std::sqrt(std::max(0.0, 1 - nx * nx - ny * ny));
    float light = clamp01((float)(0.5 - 0.5 * nx - 0.55 * ny + 0.5 * nz));
    double lon = std::atan2(nx, nz + 1e-6), lat = std::asin(std::clamp(ny, -1.0, 1.0));
    float n = melonNet((float)(lon * 2.6 + c.x * 50), (float)(lat * 2.6), 0.09f, 2.0f);
    RGBA base = mixc(kRind0, kRind1, light), netc = mixc(kNet0, kNet1, light);
    RGBA col = mixc(base, netc, n);
    if (lit) {
      // lit from inside: the flesh glows through the net
      RGBA glowc = mixc(kOrange, hex(0xffe2a0), (float)nz);
      col = mixc(glowc, mixc(kNet1, RGBA(1, 1, 0.85f), 0.5f), n);
    }
    double sp = std::pow(std::max(0.0, -0.45 * nx - 0.55 * ny + 0.7 * nz), 30);
    col = mixc(col, RGBA(1, 1, 1), (float)sp * 0.7f);
    double edge = std::sqrt(nx * nx + ny * ny);
    if (edge > 0.9) col = mixc(col, lit ? kGold : hex(0x0c1a0e), (float)((edge - 0.9) / 0.1) * 0.8f);
    return col;
  });
  // a little stalk
  p.cv.fillCapsule(p.P(c + V2(0, -r * 0.05), z + 0.004), p.P(c + V2(r * 0.12, -r * 0.2), z + 0.006), p.L(0.0016),
                   hex(lit ? 0xb07a3c : 0x6e4623));
}

static void drawRaised(const Painter &p, const Table &t, bool golden) {
  const World &w = t.world;
  // cabinet walls: everything outside the playfield outline, raised
  {
    std::vector<V2> poly = {V2(-0.05, -0.05), V2(0.56, -0.05), V2(0.56, 1.12), V2(-0.05, 1.12), V2(-0.05, -0.05)};
    for (auto &q : t.outline) poly.push_back(q);
    poly.push_back(t.outline[0]);
    RGBA trim = golden ? hex(0x8a6a1e) : hex(0x24362a);
    extrudePoly(p, poly, 0.03, hex(0x0e1810), [&](float x, float y) {
      V2 m = p.toM(x, y, 0.03);
      float br = 0.85f + 0.3f * noise2((float)(m.x * 400), (float)(m.y * 12));      // brushed metal
      RGBA c = scale(mixc(hex(0x1a241e), trim, 0.5f), br);
      return c;
    });
  }
  // walls and blocks: dark green rind with the net on top
  for (auto &poly : t.wallChains) {
    bool slingTri = poly.size() == 3;
    if (slingTri) continue;
    extrudePoly(p, poly, 0.024, hex(0x10261a), [&](float x, float y) {
      V2 m = p.toM(x, y, 0.024);
      float n = melonNet((float)(m.x / 0.012), (float)(m.y / 0.012), 0.12f, 5.0f);
      float shade = 0.8f + 0.4f * noise2((float)(m.x * 60), (float)(m.y * 60));
      return scale(mixc(hex(0x0f1f14), hex(0x4f7a3c), n * 0.55f), shade);
    });
  }
  // slingshots: rubber rings around three posts, a melon slice on top
  for (auto &poly : t.wallChains) {
    if (poly.size() != 3) continue;
    V2 A = poly[0], B = poly[1], C = poly[2];
    for (auto e : {std::make_pair(A, B), std::make_pair(B, C), std::make_pair(C, A)})
      extrudeCapsule(p, e.first, e.second, 0.0052, 0.012, hex(0x050505),
                     [](float, float) { return hex(0x161616); });
    std::vector<V2> tri = {A + (B - A) * 0.02 + (C - A) * 0.02, B, C};
    V2 kickN = perp(norm(C - A));
    if (dot(kickN, B - A) > 0) kickN = kickN * -1;               // pointing away from the triangle
    extrudePoly(p, tri, 0.03, hex(0x3a682e), [&, A, kickN](float x, float y) {
      V2 m = p.toM(x, y, 0.03);
      double d = -dot(m - A, kickN);                              // distance in from the kicking face
      if (d < 0.0035) return mixc(kRind1, kNet1, melonNet((float)(m.x / 0.004), (float)(m.y / 0.004), 0.15f));
      if (d < 0.0055) return hex(0xc8dd8a);
      RGBA flesh = mixc(hex(0xffb86e), hex(0xf07628), clamp01((float)((d - 0.0055) / 0.03)));
      // seeds near the middle of the slice
      double s = std::fmod(std::fabs(m.y * 700 + m.x * 350), 1.0);
      if (d > 0.022 && d < 0.03 && s < 0.18) flesh = mixc(flesh, hex(0xfae2b2), 0.8f);
      return flesh;
    });
  }
  // rubber posts
  for (auto &c : w.circles) {
    if (c.kind == K_BUMPER) continue;
    extrudeCircle(p, c.c, c.r, 0.02, hex(0x0a0a0a), [&](float x, float y) {
      V2 m = p.toM(x, y, 0.02);
      double d = len(m - c.c) / c.r;
      if (d > 0.62) return hex(0x151515);
      return sphere(p, c.c, c.r * 0.62, 0.02, hex(0x9a9a88), hex(0xfffff2))(x, y);
    });
  }
  // metal guides and rails
  for (auto &g : t.guideChains)
    for (size_t i = 0; i + 1 < g.size(); i++) {
      double r = 0.0028;
      extrudeCapsule(p, g[i], g[i + 1], r, 0.018, hex(0x1a221c), chrome(p, g[i], g[i + 1], r, 0.018));
    }
  // one-way gates: a wire
  for (auto &s : w.segs)
    if (s.oneway) extrudeCapsule(p, s.a, s.b, 0.0011, 0.012, hex(0x303a32), chrome(p, s.a, s.b, 0.0011, 0.012));
  // M-E-L-O-N standups
  for (auto &s : w.segs) {
    if (s.kind != K_STANDUP) continue;
    std::vector<V2> r = {s.a + V2(-0.003, 0), s.b + V2(-0.003, 0), s.b + V2(0.0015, 0), s.a + V2(0.0015, 0)};
    extrudePoly(p, r, 0.026, hex(0x6e4a18), [](float, float) { return hex(0xf2c45a); });
  }
  // pop bumpers
  for (int i = 0; i < 3; i++) {
    const Circle &c = w.circles[t.bumperCircle[i]];
    drawBumper(p, c.c, c.r, false, false);
  }
  // flipper pivots' shadows and the plunger housing are drawn live
}

// soft shadows of everything raised, cast down and to the right
static void castShadows(Canvas &cv, const Painter &p, const Table &t) {
  const int D = 4;                                  // shadow canvas is 1/4 size
  Canvas sh(cv.w / D + 2, cv.h / D + 2);
  Painter sp{sh, p.S / D, p.org};
  RGBA k(0, 0, 0, 1);
  V2 off(0.006, 0.008);
  for (auto &poly : t.wallChains) {
    std::vector<V2> q;
    for (auto &v : poly) q.push_back(v + off);
    sh.fillPoly(mapPts(sp, q, 0), k);
  }
  for (auto &c : t.world.circles) sh.fillCircle(sp.P(c.c + off * (c.kind == K_BUMPER ? 1.6 : 1.0)), sp.L(c.r * 1.05), k);
  for (auto &g : t.guideChains)
    for (size_t i = 0; i + 1 < g.size(); i++) sh.fillCapsule(sp.P(g[i] + off), sp.P(g[i + 1] + off), sp.L(0.003), k);
  // the ramp's shadow on the floor
  for (auto &r : t.world.ramps)
    for (size_t i = 0; i + 1 < r.pts.size(); i++) {
      V2 o1 = off * (1 + r.h[i] * 40), o2 = off * (1 + r.h[i + 1] * 40);
      sh.fillCapsule(sp.P(r.pts[i] + o1), sp.P(r.pts[i + 1] + o2), sp.L(0.012), RGBA(0, 0, 0, 0.45f));
    }
  sh.blur(2);
  for (int y = 0; y < cv.h; y++)
    for (int x = 0; x < cv.w; x++) {
      float fx = (x + 0.5f) / D - 0.5f, fy = (y + 0.5f) / D - 0.5f;
      int ix = (int)fx, iy = (int)fy;
      float tx = fx - ix, ty = fy - iy;
      auto A = [&](int i, int j) { return sh.px[((size_t)std::min(j, sh.h - 1) * sh.w + std::min(i, sh.w - 1)) * 4 + 3]; };
      float a = A(ix, iy) * (1 - tx) * (1 - ty) + A(ix + 1, iy) * tx * (1 - ty) + A(ix, iy + 1) * (1 - tx) * ty +
                A(ix + 1, iy + 1) * tx * ty;
      if (a <= 0) continue;
      float *px = &cv.px[((size_t)y * cv.w + x) * 4];
      float m = 1 - 0.6f * clamp01(a);
      px[0] *= m; px[1] *= m; px[2] *= m;
    }
}

static void drawRollovers(const Painter &p, const Table &t) {
  for (auto &s : t.world.sensors) {
    if (s.id == E_SHOOTER || s.id == E_ORBIT_ENTER || s.id == E_ORBIT_TOP) continue;
    V2 c = (s.a + s.b) * 0.5;
    p.cv.fillCapsule(p.P(c + V2(0, -0.007)), p.P(c + V2(0, 0.007)), p.L(0.0022), RGBA(0, 0, 0, 0.5f));
    p.cv.fillCapsule(p.P(c + V2(0, -0.006), 0.002), p.P(c + V2(0, 0.006), 0.002), p.L(0.0009),
                     chrome(p, c + V2(0, -0.006), c + V2(0, 0.006), 0.0009, 0.002));
  }
}

static void drawPortalCup(const Painter &p, const Table &t) {
  V2 c = t.portal;
  p.cv.fillCircle(p.P(c), p.L(0.0215), [&](float x, float y) {
    double d = len(p.toM(x, y) - c) / 0.0215;
    return mixc(hex(0x000000), hex(0x4a3a10), (float)std::pow(d, 3));
  });
  p.cv.ring(p.P(c, 0.002), p.L(0.0195), p.L(0.0235), hex(0xc9a23c));
  // the kicker's slot
  p.cv.fillCapsule(p.P(c + V2(-0.004, 0.008)), p.P(c + V2(0.004, 0.008)), p.L(0.0017), hex(0x1a1408));
}

static void drawDecals(Canvas &cv, Text &tx, const Painter &p, const Table &t, bool golden) {
  RGBA cream(kCream.r, kCream.g, kCream.b, 0.55f);
  text(cv, tx, p, "melon", V2(0.235, 0.80), 0.050, 0, RGBA(kGreen.r, kGreen.g, kGreen.b, 0.20f), FONT_SANS_ITALIC);
  text(cv, tx, p, "melon", V2(0.2335, 0.7985), 0.050, 0, RGBA(kGreen.r, kGreen.g, kGreen.b, 0.55f), FONT_SANS_ITALIC);
  text(cv, tx, p, "P I N B A L L", V2(0.235, 0.838), 0.011, 0, RGBA(kOrange.r, kOrange.g, kOrange.b, 0.6f));
  text(cv, tx, p, "RIND LOOP", V2(0.027, 0.465), 0.0105, -kPi / 2, cream);
  text(cv, tx, p, "apk upgrade", V2(0.250, 0.170), 0.009, 0, RGBA(kOrange.r, kOrange.g, kOrange.b, 0.55f));
  text(cv, tx, p, "SEED BANK", V2(0.382, 0.450), 0.0095, std::atan2(0.090, 0.030), cream);
  text(cv, tx, p, "this is the other side", V2(0.240, 0.522), 0.0072, 0,
       RGBA(kGold.r, kGold.g, kGold.b, golden ? 0.8f : 0.45f), FONT_SANS_ITALIC);
  text(cv, tx, p, "KERNEL", V2(0.314, 0.718), 0.0085, -74.7 * kPi / 180, RGBA(kOrange.r, kOrange.g, kOrange.b, 0.6f));
  text(cv, tx, p, "PORTAL", V2(0.240, 0.652), 0.0085, 0, RGBA(kGold.r, kGold.g, kGold.b, 0.6f));
  text(cv, tx, p, "LOOP", V2(0.027, 0.705), 0.0085, -kPi / 2, cream);
  text(cv, tx, p, "SEEDS", V2(0.350, 0.548), 0.0080, -57 * kPi / 180 + kPi / 2 - kPi / 2, cream);
  text(cv, tx, p, "BONUS", V2(0.240, 0.740), 0.0075, 0, cream);
  if (golden) text(cv, tx, p, "GAUNTLET SURVIVOR", V2(0.235, 0.862), 0.0075, 0, RGBA(kGold.r, kGold.g, kGold.b, 0.8f));
  // shooter lane arrows
  for (int i = 0; i < 4; i++) {
    double y = 0.86 - i * 0.05;
    std::vector<V2> chev = {V2(0.480, y + 0.006), V2(0.487, y - 0.002), V2(0.494, y + 0.006), V2(0.494, y + 0.010),
                            V2(0.487, y + 0.002), V2(0.480, y + 0.010)};
    cv.fillPoly(mapPts(p, chev, 0), RGBA(kOrange.r, kOrange.g, kOrange.b, 0.18f + 0.1f * i));
  }
  (void)t;
}

// ---------------------------------------------------------------- the wire ramp
static void drawRamp(const Painter &p, const Ramp &r, bool golden) {
  RGBA side = hex(0x223026);
  auto wire = [&](double off, double dz, double rad) {
    for (size_t i = 0; i + 1 < r.pts.size(); i++) {
      // subdivide for smooth curves
      for (int k = 0; k < 6; k++) {
        double s0 = r.cum[i] + (r.cum[i + 1] - r.cum[i]) * k / 6.0, s1 = r.cum[i] + (r.cum[i + 1] - r.cum[i]) * (k + 1) / 6.0;
        V2 p0, t0, p1, t1;
        double h0, h1, dh;
        r.at(s0, p0, t0, h0, dh);
        r.at(s1, p1, t1, h1, dh);
        V2 a = p0 + perp(t0) * off, b = p1 + perp(t1) * off;
        double za = h0 + dz + kBallR, zb = h1 + dz + kBallR;
        V2 A = p.P(a, za), B = p.P(b, zb);
        p.cv.fillCapsule(A, B, p.L(rad) + 0.6, RGBA(0, 0, 0, 0.35f));
        RGBA hi = golden ? hex(0xfff0b0) : hex(0xeef6ee);
        p.cv.fillCapsule(A, B, p.L(rad), [&, A, B](float x, float y) {
          V2 q(x, y);
          double d = len(q - closestOnSeg(q, A, B)) / std::max(1.0, p.L(rad));
          return mixc(hi, side, (float)std::clamp(d * 1.2, 0.0, 1.0));
        });
      }
    }
  };
  // supports: posts from the floor up to the wires
  for (double s = 0.10; s < r.total() - 0.05; s += 0.13) {
    V2 c, t;
    double h, dh;
    r.at(s, c, t, h, dh);
    if (h < 0.015) continue;
    for (double sgn : {-1.0, 1.0}) {
      V2 base = c + perp(t) * (sgn * 0.02);
      p.cv.fillCapsule(p.P(base, 0), p.P(base, h + kBallR + 0.01), p.L(0.0014), hex(0x3a463e));
    }
    p.cv.fillCapsule(p.P(c + perp(t) * 0.02, h + kBallR - 0.004), p.P(c - perp(t) * 0.02, h + kBallR - 0.004), p.L(0.0012),
                     hex(0x56645a));
  }
  wire(-0.0085, -0.0105, 0.0011);   // the two wires the ball rolls on
  wire(0.0085, -0.0105, 0.0011);
  wire(-0.0165, 0.0010, 0.0012);    // the side rails
  wire(0.0165, 0.0010, 0.0012);
  // the entrance flap
  V2 E = r.pts[0], t = norm(r.pts[1] - r.pts[0]), n = perp(t);
  std::vector<V2> flap = {E - n * 0.02, E + n * 0.02, E + n * 0.02 + t * 0.05, E - n * 0.02 + t * 0.05};
  std::vector<V2> fz;
  for (size_t i = 0; i < flap.size(); i++) fz.push_back(p.P(flap[i], i < 2 ? 0.001 : 0.012));
  p.cv.fillPoly(fz, RGBA(kGreen.r, kGreen.g, kGreen.b, 0.18f));
}

// ---------------------------------------------------------------- sprites
static SDL_Surface *ballSurface(bool golden) {
  const int N = 128;
  Canvas cv(N * 2, N * 2);
  RGBA tint = golden ? RGBA(1.0f, 0.82f, 0.42f) : RGBA(1, 1, 1);
  double R = N - 1;
  cv.fillCircle(V2(N, N), R, [&](float x, float y) {
    double nx = (x - N) / R, ny = (y - N) / R;
    double nz = std::sqrt(std::max(0.0, 1 - nx * nx - ny * ny));
    // reflection of an environment: dark space above, the lit table below, a bright band between
    double ry = 2 * nz * ny;                              // reflected ray's y (view along -z)
    RGBA env;
    if (ry < -0.15) env = mixc(hex(0x566a5c), hex(0x121a14), clamp01((float)((-ry - 0.15) * 1.6)));
    else if (ry < 0.1) env = hex(0xe6f2e8);
    else env = mixc(hex(0x6f9a64), hex(0x1c2c1e), clamp01((float)((ry - 0.1) * 1.4)));
    env = RGBA(env.r * tint.r, env.g * tint.g, env.b * tint.b);
    double fres = std::pow(1 - nz, 2);
    env = mixc(env, scale(env, 0.55f), (float)fres);
    double sp = std::pow(std::max(0.0, -0.42 * nx - 0.5 * ny + 0.76 * nz), 60);
    env = mixc(env, RGBA(1, 1, 1), (float)std::min(1.0, sp * 1.4));
    double sp2 = std::pow(std::max(0.0, 0.3 * nx + 0.2 * ny + 0.93 * nz), 18);
    env = mixc(env, RGBA(1, 1, 1), (float)sp2 * 0.25f);
    return env;
  });
  return cv.downsample2().toSurface();
}

static SDL_Surface *glowSurface() {
  const int N = 64;
  Canvas cv(N, N);
  for (int y = 0; y < N; y++)
    for (int x = 0; x < N; x++) {
      double d = std::sqrt((x + 0.5 - N / 2.0) * (x + 0.5 - N / 2.0) + (y + 0.5 - N / 2.0) * (y + 0.5 - N / 2.0)) / (N / 2.0);
      float a = (float)std::max(0.0, 1 - d);
      cv.blend(x, y, RGBA(1, 1, 1), a * a);
    }
  return cv.toSurface();
}

void Art::free() {
  auto f = [](SDL_Surface *&s) { if (s) SDL_DestroySurface(s); s = nullptr; };
  f(base); f(overlay); f(ball); f(glow); f(logo);
  for (auto &l : lamps) f(l.surf);
  for (auto &b : bumperLit) f(b.surf);
  lamps.clear();
}

bool buildArt(Art &art, const Table &t, Text &tx, bool golden, const char *dataDir) {
  art.free();
  const double S2 = kArtScale * 2;
  int W2 = (int)(t.world.width * S2) & ~1, H2 = (int)(t.world.length * S2) & ~1;
  art.w = W2 / 2;
  art.h = H2 / 2;
  {
    Canvas cv(W2, H2);
    Painter p{cv, S2, V2()};
    // floor
    for (int y = 0; y < H2; y++)
      for (int x = 0; x < W2; x++) {
        V2 m = p.toM(x + 0.5f, y + 0.5f);
        RGBA c = floorAt(m.x, m.y, t, golden);
        cv.blend(x, y, c, 1);
      }
    drawDecals(cv, tx, p, t, golden);
    drawPortalCup(p, t);
    for (int i = 0; i < LA_COUNT; i++) drawLamp(p, tx, t.lamps[i], i, false);
    drawRollovers(p, t);
    castShadows(cv, p, t);
    drawRaised(p, t, golden);
    Canvas small = cv.downsample2();
    art.base = small.toSurface();
  }
  // lamp sprites, lit
  art.lamps.resize(LA_COUNT);
  for (int i = 0; i < LA_COUNT; i++) {
    const Lamp &l = t.lamps[i];
    double ext = l.size * (l.shape == L_RECT ? 0.9 + 0.62 * l.label.size() + 0.3 : (l.shape == L_ARROW ? 1.4 : 1.2));
    if (i == LA_PORTAL) ext = 0.033;
    V2 org = l.p - V2(ext, ext);
    int n = (int)std::ceil(2 * ext * S2) + 4;
    n += n & 1;
    Canvas cv(n, n);
    Painter p{cv, S2, org};
    drawLamp(p, tx, l, i, true);
    art.lamps[i].surf = cv.downsample2().toSurface();
    art.lamps[i].x = (int)std::lround(org.x * kArtScale);
    art.lamps[i].y = (int)std::lround(org.y * kArtScale);
  }
  // pop bumper caps, lit
  for (int i = 0; i < 3; i++) {
    const Circle &c = t.world.circles[t.bumperCircle[i]];
    double ext = c.r * 1.1;
    V2 org = c.c - V2(ext, ext + 0.032 * kHeightK);
    int n = (int)std::ceil(2 * ext * S2) + 4;
    n += n & 1;
    Canvas cv(n, n);
    Painter p{cv, S2, org};
    drawBumper(p, c.c, c.r, true, true);
    art.bumperLit[i].surf = cv.downsample2().toSurface();
    art.bumperLit[i].x = (int)std::lround(org.x * kArtScale);
    art.bumperLit[i].y = (int)std::lround(org.y * kArtScale);
  }
  // the ramp overlay
  {
    Canvas cv(W2, H2);
    Painter p{cv, S2, V2()};
    for (auto &r : t.world.ramps) drawRamp(p, r, golden);
    art.overlay = cv.downsample2().toSurface();
  }
  art.ball = ballSurface(golden);
  art.glow = glowSurface();
  art.logo = buildLogo(tx, 424, 256, golden, dataDir);
  return art.base && art.overlay && art.ball;
}

// ---------------------------------------------------------------- the panel's title box
SDL_Surface *buildLogo(Text &tx, int w, int h, bool golden, const char *dataDir) {
  Canvas cv(w * 2, h * 2);
  const int W = w * 2, H = h * 2;
  // space with seeds and the planet in the corner
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      RGBA c = mixc(kSpace0, kSpace1, (float)y / H);
      float net = melonNet(x / 60.f, y / 60.f, 0.05f, 3.1f);
      c = mixc(c, kGreen, net * 0.06f);
      float s = hash2(std::floor(x / 9.f), std::floor(y / 9.f), 3.3f);
      if (s > 0.985f) c = mixc(c, kCream, 0.8f * (s - 0.985f) / 0.015f);
      double nx = (x - W * 0.95) / (H * 0.62), ny = (y - H * 1.12) / (H * 0.62), d = std::sqrt(nx * nx + ny * ny);
      if (d < 1) {
        double nz = std::sqrt(1 - d * d);
        float n = melonNet((float)(std::atan2(nx, nz) * 6), (float)(std::asin(std::clamp(ny, -1.0, 1.0)) * 6), 0.07f, 1.1f);
        float light = clamp01((float)(0.55 - 0.45 * nx - 0.35 * ny + 0.4 * nz));
        c = mixc(mixc(kRind0, kRind1, light), mixc(kNet0, kNet1, light), n);
      } else {
        c = mixc(c, kGreen, (float)std::exp(-(d - 1) * 14) * 0.5f);
      }
      cv.blend(x, y, c, 1);
    }
  // the mascot, if installed with the game
  std::string mascot = std::string(dataDir) + "/mascot.png";
  if (SDL_Surface *mc = IMG_Load(mascot.c_str())) {
    SDL_Surface *c = SDL_ConvertSurface(mc, SDL_PIXELFORMAT_ARGB8888);
    SDL_DestroySurface(mc);
    if (c) {
      // area-averaged downscale into the canvas
      double sc = (H * 0.62) / c->h;
      int tw = (int)(c->w * sc), th = (int)(c->h * sc);
      int x0 = (int)(W * 0.24) - tw / 2, y0 = (int)(H * 0.60) - th / 2;
      for (int y = 0; y < th; y++)
        for (int x = 0; x < tw; x++) {
          float acc[4] = {0, 0, 0, 0};
          int sx0 = x * c->w / tw, sx1 = std::max(sx0 + 1, (x + 1) * c->w / tw);
          int sy0 = y * c->h / th, sy1 = std::max(sy0 + 1, (y + 1) * c->h / th);
          for (int sy = sy0; sy < sy1; sy++)
            for (int sx = sx0; sx < sx1; sx++) {
              Uint32 v = ((Uint32 *)((Uint8 *)c->pixels + (size_t)sy * c->pitch))[sx];
              float a = ((v >> 24) & 255) / 255.f;
              acc[0] += ((v >> 16) & 255) / 255.f * a;
              acc[1] += ((v >> 8) & 255) / 255.f * a;
              acc[2] += (v & 255) / 255.f * a;
              acc[3] += a;
            }
          float n = (float)((sx1 - sx0) * (sy1 - sy0));
          if (acc[3] <= 0) continue;
          cv.blend(x0 + x, y0 + y, RGBA(acc[0] / acc[3], acc[1] / acc[3], acc[2] / acc[3]), acc[3] / n);
        }
      SDL_DestroySurface(c);
    }
  }
  RGBA title = golden ? kGold : kGreen;
  auto big = [&](const std::string &s, V2 at, float px, RGBA col, FontFace face) {
    SDL_Surface *surf = tx.render(s, px, face);
    if (!surf) return;
    double sc = px / std::max(1, surf->h) * 1.3;
    for (int k = 6; k >= 1; k--) cv.blitRotated(surf, at + V2(k * 0.9, k * 1.2), 0, sc, RGBA(0, 0, 0, 0.25f));
    cv.blitRotated(surf, at + V2(-2, -2), 0, sc, RGBA(1, 1, 1, 0.35f));
    cv.blitRotated(surf, at, 0, sc, col);
    SDL_DestroySurface(surf);
  };
  big("melon", V2(W * 0.64, H * 0.24), (float)H * 0.20f, title, FONT_SANS_ITALIC);
  big("PINBALL", V2(W * 0.66, H * 0.44), (float)H * 0.10f, kOrange, FONT_SANS);
  big(golden ? "gauntlet survivor" : "melon planet", V2(W * 0.66, H * 0.56), (float)H * 0.052f,
      golden ? kGold : kCream, FONT_SANS_ITALIC);
  return cv.downsample2().toSurface();
}

}  // namespace pb

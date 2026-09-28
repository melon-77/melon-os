#include "render.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "dotfont.hpp"
#include "raster.hpp"

namespace pb {

static SDL_FColor fc(unsigned hexc, float a = 1) {
  return {((hexc >> 16) & 255) / 255.f, ((hexc >> 8) & 255) / 255.f, (hexc & 255) / 255.f, a};
}
static SDL_Color c8(unsigned hexc, Uint8 a = 255) {
  return {(Uint8)((hexc >> 16) & 255), (Uint8)((hexc >> 8) & 255), (Uint8)(hexc & 255), a};
}

bool View::init(SDL_Renderer *r, Art &art, Text &text) {
  r_ = r;
  art_ = &art;
  text_ = &text;
  auto tex = [&](SDL_Surface *s) {
    SDL_Texture *t = s ? SDL_CreateTextureFromSurface(r_, s) : nullptr;
    if (t) SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    return t;
  };
  base_ = tex(art.base);
  overlay_ = tex(art.overlay);
  ball_ = tex(art.ball);
  glow_ = tex(art.glow);
  logo_ = tex(art.logo);
  for (auto &l : art.lamps) lamps_.push_back(tex(l.surf));
  for (int i = 0; i < 3; i++) bumperLit_[i] = tex(art.bumperLit[i].surf);
  target_ = SDL_CreateTexture(r_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, art.w, art.h);
  if (!base_ || !target_) return false;
  SDL_SetTextureBlendMode(target_, SDL_BLENDMODE_BLEND);
  // a round LED dot for the displays
  {
    SDL_Surface *s = SDL_CreateSurface(16, 16, SDL_PIXELFORMAT_ARGB8888);
    for (int y = 0; y < 16; y++)
      for (int x = 0; x < 16; x++) {
        double d = std::sqrt((x - 7.5) * (x - 7.5) + (y - 7.5) * (y - 7.5));
        double a = std::clamp(7.8 - d, 0.0, 1.0);
        double b = 0.75 + 0.25 * std::clamp(1 - d / 7.5, 0.0, 1.0);
        Uint8 v = (Uint8)(255 * b);
        ((Uint32 *)((Uint8 *)s->pixels + y * s->pitch))[x] = ((Uint32)(a * 255) << 24) | (v << 16) | (v << 8) | v;
      }
    dot_ = tex(s);
    SDL_DestroySurface(s);
  }
  if (const char *cam = SDL_getenv("MELON_PINBALL_CAMERA")) std::sscanf(cam, "%lf,%lf,%lf", &camH_, &camD_, &lookZ_);
  // fit the tilted table into the left part of the window
  V2 c[4] = {rawProject(0, 0, 0), rawProject(0.508, 0, 0), rawProject(0, 1.066, 0), rawProject(0.508, 1.066, 0)};
  double x0 = 1e9, x1 = -1e9, y0 = 1e9, y1 = -1e9;
  for (auto &p : c) { x0 = std::min(x0, p.x); x1 = std::max(x1, p.x); y0 = std::min(y0, p.y); y1 = std::max(y1, p.y); }
  const double RX0 = 34, RX1 = 790, RY0 = kMenuH + 14, RY1 = kLogicalH - 10;
  fitS_ = std::min((RX1 - RX0) / (x1 - x0), (RY1 - RY0) / (y1 - y0));
  fitX_ = (RX0 + RX1) / 2 - fitS_ * (x0 + x1) / 2;
  fitY_ = RY0 - fitS_ * y0;
  buildMesh();
  return true;
}

void View::shutdown() {
  auto d = [](SDL_Texture *&t) { if (t) SDL_DestroyTexture(t); t = nullptr; };
  d(base_); d(overlay_); d(target_); d(ball_); d(glow_); d(logo_); d(dot_);
  for (auto &t : lamps_) d(t);
  for (auto &t : bumperLit_) d(t);
  for (auto &kv : textCache_) SDL_DestroyTexture(kv.second);
  textCache_.clear();
}

// Camera: above and behind the player's end of the table, looking down the playfield.
V2 View::rawProject(double x, double y, double z) const {
  double u = x - 0.254, d = 1.066 - y;
  double fy = -camH_, fz = lookZ_ + camD_, fl = std::sqrt(fy * fy + fz * fz);
  fy /= fl;
  fz /= fl;
  double rx = u, ry = z - camH_, rz = d + camD_;
  double zc = ry * fy + rz * fz;
  double yc = ry * fz - rz * fy;
  return V2(rx / zc, -yc / zc);
}

V2 View::project(double x, double y, double z) const {
  V2 p = rawProject(x, y, z);
  return V2(fitX_ + fitS_ * p.x, fitY_ + fitS_ * p.y);
}

double View::pxPerM(double x, double y) const { return len(project(x + 0.001, y) - project(x, y)) / 0.001; }

void View::buildMesh() {
  const int NX = 12, NY = 36;
  mesh_.clear();
  meshIdx_.clear();
  for (int j = 0; j <= NY; j++)
    for (int i = 0; i <= NX; i++) {
      double u = (double)i / NX, v = (double)j / NY;
      V2 p = project(u * 0.508, v * 1.066);
      SDL_Vertex vx;
      vx.position = {(float)p.x, (float)p.y};
      vx.color = {1, 1, 1, 1};
      vx.tex_coord = {(float)u, (float)v};
      mesh_.push_back(vx);
    }
  for (int j = 0; j < NY; j++)
    for (int i = 0; i < NX; i++) {
      int a = j * (NX + 1) + i, b = a + 1, c = a + NX + 1, d = c + 1;
      meshIdx_.insert(meshIdx_.end(), {a, b, c, b, d, c});
    }
}

void View::poly(const std::vector<V2> &pts, SDL_FColor c) {
  if (pts.size() < 3) return;
  std::vector<SDL_Vertex> v;
  std::vector<int> idx;
  for (auto &p : pts) v.push_back({{(float)p.x, (float)p.y}, c, {0, 0}});
  for (size_t i = 1; i + 1 < pts.size(); i++) idx.insert(idx.end(), {0, (int)i, (int)i + 1});
  SDL_RenderGeometry(r_, nullptr, v.data(), (int)v.size(), idx.data(), (int)idx.size());
}

// ---------------------------------------------------------------- live parts of the table (drawn into its texture)
static std::vector<V2> flipperOutline(const Flipper &f, double grow) {
  double r0 = f.r0 + grow, r1 = f.r1 + grow, L = f.length;
  double b = std::asin(std::clamp((r0 - r1) / L, -1.0, 1.0));
  std::vector<V2> local;
  const int N = 10;
  for (int i = 0; i <= N; i++) {       // round tip
    double a = -(kPi / 2 - b) + (kPi - 2 * b) * i / N;
    local.push_back(V2(L + r1 * std::cos(a), r1 * std::sin(a)));
  }
  for (int i = 0; i <= 2 * N; i++) {   // round pivot end
    double a = (kPi / 2 - b) + (kPi + 2 * b) * i / (2 * N);
    local.push_back(V2(r0 * std::cos(a), r0 * std::sin(a)));
  }
  std::vector<V2> out;
  for (auto &p : local) out.push_back(f.pivot + rot(p, f.angle));
  return out;
}

void View::drawFlipper(const Flipper &f, bool golden) {
  const double H = 0.016;
  const double S = kArtScale;
  auto at = [&](const std::vector<V2> &pts, double z) {
    std::vector<V2> o;
    for (auto &p : pts) o.push_back(V2(p.x * S, (p.y - z * kHeightK) * S));
    return o;
  };
  auto outer = flipperOutline(f, 0), inner = flipperOutline(f, -0.0028);
  // shadow
  {
    std::vector<V2> sh;
    for (auto &p : outer) sh.push_back(V2((p.x + 0.004) * S, (p.y + 0.006) * S));
    poly(sh, {0, 0, 0, 0.35f});
  }
  int steps = (int)(H * kHeightK * S);
  for (int k = 0; k < steps; k++) {
    float t = (float)k / steps;
    poly(at(outer, H * t), {0.06f + 0.08f * t, 0.14f + 0.14f * t, 0.07f + 0.06f * t, 1});
  }
  poly(at(outer, H), fc(0x2f6e2c));                 // green rubber ring
  // the bat's top: melon flesh, lighter towards the pivot
  auto top = at(inner, H);
  std::vector<SDL_Vertex> v;
  std::vector<int> idx;
  SDL_FColor tipc = golden ? fc(0xd9a531) : fc(0xf07a2e), rootc = golden ? fc(0xfff0b8) : fc(0xffd2a0);
  for (size_t i = 0; i < top.size(); i++) {
    double along = dot(inner[i] - f.pivot, V2(std::cos(f.angle), std::sin(f.angle))) / f.length;
    float t = (float)std::clamp(along, 0.0, 1.0);
    SDL_FColor c = {rootc.r + (tipc.r - rootc.r) * t, rootc.g + (tipc.g - rootc.g) * t, rootc.b + (tipc.b - rootc.b) * t, 1};
    v.push_back({{(float)top[i].x, (float)top[i].y}, c, {0, 0}});
  }
  for (size_t i = 1; i + 1 < top.size(); i++) idx.insert(idx.end(), {0, (int)i, (int)i + 1});
  SDL_RenderGeometry(r_, nullptr, v.data(), (int)v.size(), idx.data(), (int)idx.size());
  // chrome pivot cap
  V2 pc(f.pivot.x * S, (f.pivot.y - H * kHeightK) * S);
  SDL_FRect rc = {(float)(pc.x - 7), (float)(pc.y - 7), 14, 14};
  SDL_SetTextureColorMod(dot_, 220, 230, 220);
  SDL_SetTextureAlphaMod(dot_, 255);
  SDL_RenderTexture(r_, dot_, nullptr, &rc);
}

void View::drawPlunger(const Plunger &p) {
  const double S = kArtScale;
  double top = p.restY + p.pos;
  double cx = (p.x0 + p.x1) / 2;
  // spring
  int coils = 9;
  double y0 = top + 0.012, y1 = 1.066;
  SDL_SetRenderDrawColor(r_, 150, 160, 150, 255);
  for (int i = 0; i < coils; i++) {
    double ya = y0 + (y1 - y0) * i / coils, yb = y0 + (y1 - y0) * (i + 0.5) / coils;
    SDL_RenderLine(r_, (float)((cx - 0.008) * S), (float)(ya * S), (float)((cx + 0.008) * S), (float)(yb * S));
    SDL_RenderLine(r_, (float)((cx - 0.008) * S + 1), (float)(ya * S), (float)((cx + 0.008) * S + 1), (float)(yb * S));
  }
  // rod
  std::vector<SDL_Vertex> v = {
      {{(float)((cx - 0.0035) * S), (float)(top * S)}, fc(0x707a72), {0, 0}},
      {{(float)((cx + 0.0035) * S), (float)(top * S)}, fc(0xe8f0e8), {0, 0}},
      {{(float)((cx + 0.0035) * S), (float)(y1 * S)}, fc(0x808a82), {0, 0}},
      {{(float)((cx - 0.0035) * S), (float)(y1 * S)}, fc(0x404842), {0, 0}}};
  int idx[] = {0, 1, 2, 0, 2, 3};
  SDL_RenderGeometry(r_, nullptr, v.data(), 4, idx, 6);
  // rubber tip, drawn as a raised block
  double H = 0.012;
  for (int k = 0; k <= 8; k++) {
    double z = H * k / 8;
    float t = (float)k / 8;
    std::vector<V2> tip = {V2((cx - 0.0125) * S, (top - z * kHeightK) * S), V2((cx + 0.0125) * S, (top - z * kHeightK) * S),
                           V2((cx + 0.0125) * S, (top + 0.009 - z * kHeightK) * S),
                           V2((cx - 0.0125) * S, (top + 0.009 - z * kHeightK) * S)};
    poly(tip, k == 8 ? fc(0xf0a35e) : SDL_FColor{0.35f + 0.3f * t, 0.18f + 0.15f * t, 0.08f, 1});
  }
}

void View::drawDrops(const Game &g) {
  const double S = kArtScale;
  for (int i = 0; i < 3; i++) {
    const Seg &s = g.table.world.segs[g.table.dropSeg[i]];
    V2 back = V2(0.949, -0.316) * 0.006;
    std::vector<V2> q = {s.a, s.b, s.b + back, s.a + back};
    if (!s.active) {
      std::vector<V2> slot;
      for (auto &p : q) slot.push_back(p * S);
      poly(slot, {0.02f, 0.03f, 0.02f, 1});
      continue;
    }
    const double H = 0.03;
    int steps = (int)(H * kHeightK * S);
    for (int k = 0; k <= steps; k++) {
      double z = H * k / steps;
      float t = (float)k / steps;
      std::vector<V2> o;
      for (auto &p : q) o.push_back(V2(p.x * S, (p.y - z * kHeightK) * S));
      poly(o, k == steps ? fc(0xfae2b2) : SDL_FColor{0.55f + 0.35f * t, 0.35f + 0.3f * t, 0.12f + 0.1f * t, 1});
    }
    // a seed painted on the face
    V2 c = (s.a + s.b) * 0.5;
    SDL_FRect rc = {(float)(c.x * S - 5), (float)((c.y - H * kHeightK * 0.5) * S - 8), 10, 16};
    SDL_SetTextureColorMod(dot_, 110, 70, 32);
    SDL_SetTextureAlphaMod(dot_, 200);
    SDL_RenderTexture(r_, dot_, nullptr, &rc);
  }
}

void View::drawTableTexture(const Game &g) {
  const double S = kArtScale;
  SDL_SetRenderTarget(r_, target_);
  SDL_SetRenderDrawColor(r_, 0, 0, 0, 0);
  SDL_RenderClear(r_);
  SDL_RenderTexture(r_, base_, nullptr, nullptr);
  // lamps: the lit insert faded in, then a soft additive glow
  for (int i = 0; i < LA_COUNT; i++) {
    float lv = std::clamp(g.lamp[i], 0.f, 1.f);
    if (lv <= 0.01f || !lamps_[i]) continue;
    const Sprite &sp = art_->lamps[i];
    SDL_FRect rc = {(float)sp.x, (float)sp.y, (float)sp.surf->w, (float)sp.surf->h};
    SDL_SetTextureAlphaModFloat(lamps_[i], lv);
    SDL_RenderTexture(r_, lamps_[i], nullptr, &rc);
  }
  SDL_SetTextureBlendMode(glow_, SDL_BLENDMODE_ADD);
  for (int i = 0; i < LA_COUNT; i++) {
    float lv = std::clamp(g.lamp[i], 0.f, 1.f);
    if (lv <= 0.01f) continue;
    const Lamp &l = g.table.lamps[i];
    double rad = (i == LA_PORTAL ? 0.05 : l.size * 3.2) * S;
    SDL_FRect rc = {(float)(l.p.x * S - rad), (float)(l.p.y * S - rad), (float)(2 * rad), (float)(2 * rad)};
    SDL_SetTextureColorMod(glow_, (l.color >> 16) & 255, (l.color >> 8) & 255, l.color & 255);
    SDL_SetTextureAlphaModFloat(glow_, lv * 0.55f);
    SDL_RenderTexture(r_, glow_, nullptr, &rc);
  }
  SDL_SetTextureBlendMode(glow_, SDL_BLENDMODE_BLEND);
  for (int i = 0; i < 3; i++) {
    float lv = std::clamp(g.bumperFlash[i], 0.f, 1.f);
    if (lv <= 0.01f || !bumperLit_[i]) continue;
    const Sprite &sp = art_->bumperLit[i];
    SDL_FRect rc = {(float)sp.x, (float)sp.y, (float)sp.surf->w, (float)sp.surf->h};
    SDL_SetTextureAlphaModFloat(bumperLit_[i], lv);
    SDL_RenderTexture(r_, bumperLit_[i], nullptr, &rc);
    const Circle &c = g.table.world.circles[g.table.bumperCircle[i]];
    double rad = c.r * 3.4 * S;
    SDL_FRect gr = {(float)(c.c.x * S - rad), (float)((c.c.y - 0.03) * S - rad), (float)(2 * rad), (float)(2 * rad)};
    SDL_SetTextureBlendMode(glow_, SDL_BLENDMODE_ADD);
    SDL_SetTextureColorMod(glow_, 255, 190, 110);
    SDL_SetTextureAlphaModFloat(glow_, lv * 0.7f);
    SDL_RenderTexture(r_, glow_, nullptr, &gr);
    SDL_SetTextureBlendMode(glow_, SDL_BLENDMODE_BLEND);
  }
  drawDrops(g);
  // ball shadows on the floor (under the flippers' tops, over everything flat)
  SDL_SetTextureColorMod(glow_, 0, 0, 0);
  for (auto &b : g.table.world.balls) {
    if (!b.alive) continue;
    double off = 1 + b.lift * 30;
    double rw = kBallR * 1.5 * S * (1 + b.lift * 6), rh = rw * 0.8;
    V2 c((b.p.x + 0.004 * off) * S, (b.p.y + 0.006 * off) * S);
    SDL_FRect rc = {(float)(c.x - rw), (float)(c.y - rh), (float)(2 * rw), (float)(2 * rh)};
    SDL_SetTextureAlphaModFloat(glow_, (float)(0.75 / (1 + b.lift * 20)));
    SDL_RenderTexture(r_, glow_, nullptr, &rc);
  }
  SDL_SetTextureColorMod(glow_, 255, 255, 255);
  for (auto &f : g.table.world.flippers) drawFlipper(f, g.golden);
  drawPlunger(g.table.world.plunger);
  SDL_SetRenderTarget(r_, nullptr);
}

void View::drawBalls(const Game &g, int layer) {
  std::vector<const Ball *> bs;
  for (auto &b : g.table.world.balls)
    if (b.alive && b.layer == layer) bs.push_back(&b);
  std::sort(bs.begin(), bs.end(), [](const Ball *a, const Ball *b) { return a->p.y < b->p.y; });
  for (auto *b : bs) {
    double y = b->p.y - (kBallR + b->lift) * kHeightK;
    V2 c = project(b->p.x, y);
    double r = kBallR * pxPerM(b->p.x, y) * (1 + b->lift * 2.5);
    if (b->captured >= 0) r *= 0.92;                // sitting down in the cup
    SDL_FRect rc = {(float)(c.x - r), (float)(c.y - r), (float)(2 * r), (float)(2 * r)};
    SDL_RenderTexture(r_, ball_, nullptr, &rc);
  }
}

void View::drawParticles(const Game &g) {
  for (auto &p : g.particles) {
    float a = std::clamp(p.life / p.maxLife, 0.f, 1.f);
    V2 c = project(p.p.x, p.p.y - p.z * kHeightK);
    double s = pxPerM(p.p.x, p.p.y);
    if (p.kind == 0) {                               // a seed
      double w = 0.0022 * s, h = 0.0036 * s;
      SDL_FRect rc = {(float)(c.x - w), (float)(c.y - h), (float)(2 * w), (float)(2 * h)};
      SDL_SetTextureColorMod(dot_, (p.color >> 16) & 255, (p.color >> 8) & 255, p.color & 255);
      SDL_SetTextureAlphaModFloat(dot_, a);
      SDL_RenderTextureRotated(r_, dot_, nullptr, &rc, p.spin, nullptr, SDL_FLIP_NONE);
    } else {                                         // a spark
      double w = 0.006 * s * (0.5 + a);
      SDL_FRect rc = {(float)(c.x - w), (float)(c.y - w), (float)(2 * w), (float)(2 * w)};
      SDL_SetTextureBlendMode(glow_, SDL_BLENDMODE_ADD);
      SDL_SetTextureColorMod(glow_, (p.color >> 16) & 255, (p.color >> 8) & 255, p.color & 255);
      SDL_SetTextureAlphaModFloat(glow_, a);
      SDL_RenderTexture(r_, glow_, nullptr, &rc);
      SDL_SetTextureBlendMode(glow_, SDL_BLENDMODE_BLEND);
    }
  }
  SDL_SetTextureAlphaMod(dot_, 255);
  // the gauntlet's golden rain, falling over the whole table
  if (g.goldRain > 0) {
    float fade = (float)std::min(1.0, g.goldRain / 1.5);
    SDL_SetTextureBlendMode(glow_, SDL_BLENDMODE_ADD);
    for (int i = 0; i < 140; i++) {
      double sx = hash2((float)i, 1.7f, 3.f), sp = 0.35 + 0.65 * hash2((float)i, 2.9f, 5.f);
      double t = g.time * sp * 0.5 + hash2((float)i, 7.1f, 1.f);
      double fy = t - std::floor(t);
      double x = 40 + sx * 740 + std::sin(g.time * 2 + i) * 6, y = kMenuH + fy * (kLogicalH - kMenuH);
      float w = (float)(3 + 5 * hash2((float)i, 4.f, 4.f));
      SDL_FRect rc = {(float)x - w, (float)y - w * 1.6f, 2 * w, 3.2f * w};
      SDL_SetTextureColorMod(glow_, 255, 210, 74);
      SDL_SetTextureAlphaModFloat(glow_, fade * 0.9f);
      SDL_RenderTexture(r_, glow_, nullptr, &rc);
    }
    SDL_SetTextureBlendMode(glow_, SDL_BLENDMODE_BLEND);
  }
}

void View::drawPopups(const Game &g) {
  float y = 330;
  for (auto &p : g.popups) {
    float k = p.t / p.dur;
    float sc = k < 0.12f ? 0.6f + 0.4f * (k / 0.12f) * 1.1f : 1.0f;
    float a = k > 0.75f ? (1 - k) / 0.25f : 1;
    SDL_Texture *t = textTex(p.text, 52, FONT_SANS_ITALIC);
    if (!t) continue;
    float tw, th;
    SDL_GetTextureSize(t, &tw, &th);
    tw *= sc;
    th *= sc;
    float cx = 412;
    for (int k2 = 3; k2 >= 1; k2--) {
      SDL_FRect sh = {cx - tw / 2 + k2 * 2.f, y - th / 2 + k2 * 2.5f, tw, th};
      SDL_SetTextureColorMod(t, 0, 0, 0);
      SDL_SetTextureAlphaModFloat(t, a * 0.35f);
      SDL_RenderTexture(r_, t, nullptr, &sh);
    }
    SDL_FRect rc = {cx - tw / 2, y - th / 2, tw, th};
    SDL_SetTextureColorMod(t, (p.color >> 16) & 255, (p.color >> 8) & 255, p.color & 255);
    SDL_SetTextureAlphaModFloat(t, a);
    SDL_RenderTexture(r_, t, nullptr, &rc);
    y += th + 6;
  }
}

// ---------------------------------------------------------------- the cabinet around the table
void View::drawCabinet() {
  // background: MelonDark
  SDL_SetRenderDrawColor(r_, 12, 16, 14, 255);
  SDL_RenderClear(r_);
  V2 tl = project(0, 0), tr = project(0.508, 0), bl = project(0, 1.066), br = project(0.508, 1.066);
  auto quad = [&](V2 a, V2 b, V2 c, V2 d, SDL_FColor ca, SDL_FColor cb) {
    SDL_Vertex v[4] = {{{(float)a.x, (float)a.y}, ca, {0, 0}}, {{(float)b.x, (float)b.y}, ca, {0, 0}},
                       {{(float)c.x, (float)c.y}, cb, {0, 0}}, {{(float)d.x, (float)d.y}, cb, {0, 0}}};
    int idx[] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(r_, nullptr, v, 4, idx, 6);
  };
  // side walls leaning out, as in the old game
  quad(tl, tl + V2(-22, -4), bl + V2(-30, 6), bl, fc(0x2a3a2e), fc(0x121c15));
  quad(tr, tr + V2(22, -4), br + V2(30, 6), br, fc(0x2a3a2e), fc(0x121c15));
  // back wall
  quad(tl + V2(-22, -4), tr + V2(22, -4), tr, tl, fc(0x1c281f), fc(0x34483a));
}

// ---------------------------------------------------------------- panel
SDL_Texture *View::textTex(const std::string &s, float px, FontFace face) {
  std::string key = s + "\x01" + std::to_string((int)px) + "\x01" + std::to_string((int)face);
  auto it = textCache_.find(key);
  if (it != textCache_.end()) return it->second;
  SDL_Surface *surf = text_->render(s, px, face);
  SDL_Texture *t = surf ? SDL_CreateTextureFromSurface(r_, surf) : nullptr;
  if (surf) SDL_DestroySurface(surf);
  if (textCache_.size() > 400) {
    for (auto &kv : textCache_) SDL_DestroyTexture(kv.second);
    textCache_.clear();
  }
  textCache_[key] = t;
  return t;
}

void View::drawText(const std::string &s, float x, float y, float px, SDL_Color c, int align, FontFace face) {
  SDL_Texture *t = textTex(s, px, face);
  if (!t) return;
  float w, h;
  SDL_GetTextureSize(t, &w, &h);
  float sc = px / std::max(1.f, h) * 1.25f;
  w *= sc;
  h *= sc;
  float x0 = align < 0 ? x : (align == 0 ? x - w / 2 : x - w);
  SDL_FRect rc = {x0, y, w, h};
  SDL_SetTextureColorMod(t, c.r, c.g, c.b);
  SDL_SetTextureAlphaMod(t, c.a);
  SDL_RenderTexture(r_, t, nullptr, &rc);
}

void View::bevel(const SDL_FRect &rc, bool sunken, SDL_Color fill) {
  SDL_Color hi = {88, 110, 94, 255}, lo = {6, 8, 7, 255};
  if (sunken) std::swap(hi, lo);
  SDL_SetRenderDrawColor(r_, fill.r, fill.g, fill.b, fill.a);
  SDL_RenderFillRect(r_, &rc);
  for (int i = 0; i < 3; i++) {
    SDL_SetRenderDrawColor(r_, hi.r, hi.g, hi.b, 255 - i * 60);
    SDL_RenderLine(r_, rc.x + i, rc.y + i, rc.x + rc.w - 1 - i, rc.y + i);
    SDL_RenderLine(r_, rc.x + i, rc.y + i, rc.x + i, rc.y + rc.h - 1 - i);
    SDL_SetRenderDrawColor(r_, lo.r, lo.g, lo.b, 255 - i * 60);
    SDL_RenderLine(r_, rc.x + i, rc.y + rc.h - 1 - i, rc.x + rc.w - 1 - i, rc.y + rc.h - 1 - i);
    SDL_RenderLine(r_, rc.x + rc.w - 1 - i, rc.y + i, rc.x + rc.w - 1 - i, rc.y + rc.h - 1 - i);
  }
}

void View::dotBox(const SDL_FRect &rc, int cols, int rows, float pitch, SDL_Color on) {
  // the unlit matrix behind the text
  SDL_SetTextureColorMod(dot_, on.r / 5, on.g / 5, on.b / 5);
  SDL_SetTextureAlphaMod(dot_, 255);
  float d = pitch * 0.8f;
  for (int j = 0; j < rows; j++)
    for (int i = 0; i < cols; i++) {
      SDL_FRect r = {rc.x + i * pitch, rc.y + j * pitch, d, d};
      SDL_RenderTexture(r_, dot_, nullptr, &r);
    }
}

void View::dotText(const std::string &s, float x, float y, float pitch, SDL_Color on, int cols, bool center) {
  if (center && cols > 0) {
    int pad = (cols - (int)s.size() * 6 + 1) / 2;
    x += std::max(0, pad) * pitch;
  }
  SDL_SetTextureColorMod(dot_, on.r, on.g, on.b);
  SDL_SetTextureAlphaMod(dot_, on.a);
  float d = pitch * 0.8f;
  for (size_t k = 0; k < s.size(); k++) {
    if (cols > 0 && (int)(k * 6 + 5) > cols) break;
    auto gl = dotGlyph(s[k]);
    for (int r = 0; r < 7; r++)
      for (int c = 0; c < 5; c++)
        if (gl[r][c] == '#') {
          SDL_FRect rc = {x + (k * 6 + c) * pitch, y + r * pitch, d, d};
          SDL_RenderTexture(r_, dot_, nullptr, &rc);
        }
  }
}

static std::string commas(long long v) {
  std::string s = std::to_string(v), o;
  int n = 0;
  for (int i = (int)s.size() - 1; i >= 0; i--) {
    o.insert(o.begin(), s[i]);
    if (++n % 3 == 0 && i > 0) o.insert(o.begin(), ',');
  }
  return o;
}

void View::drawPanel(const Game &g) {
  const float X = 812, Y = kMenuH + 12, W = kLogicalW - X - 14, H = kLogicalH - Y - 12;
  SDL_Color frame = {26, 36, 29, 255}, well = {5, 7, 6, 255};
  bevel({X, Y, W, H}, false, frame);
  SDL_Color green = c8(0x6fbf4a), orange = c8(0xf0a35e), gold = c8(0xffd24a), cream = c8(0xece2b4);
  SDL_Color accent = g.golden ? gold : green;
  // title box
  SDL_FRect logo = {X + 12, Y + 12, W - 24, 262};
  bevel(logo, true, well);
  if (logo_) {
    SDL_FRect in = {logo.x + 3, logo.y + 3, logo.w - 6, logo.h - 6};
    SDL_RenderTexture(r_, logo_, nullptr, &in);
  }
  // ball number, as in the old game: inside the title box, bottom right
  {
    SDL_FRect bb = {logo.x + logo.w - 150, logo.y + logo.h - 58, 140, 48};
    bevel(bb, true, well);
    dotText("BALL", bb.x + 12, bb.y + 14, 3.4f, orange);
    SDL_FRect nb = {bb.x + 96, bb.y + 5, 38, 38};
    SDL_SetRenderDrawColor(r_, 200, 60, 40, 255);
    SDL_RenderRect(r_, &nb);
    std::string bn = g.mode == MODE_PLAY || g.mode == MODE_BALL_END ? std::to_string(std::min(g.ball, 9)) : "-";
    dotText(bn, nb.x + 10, nb.y + 6, 3.8f, accent);
  }
  // score
  float sy = logo.y + logo.h + 12;
  SDL_FRect pbox = {X + 12, sy, 52, 62}, sbox = {X + 70, sy, W - 82, 62};
  bevel(pbox, true, well);
  bevel(sbox, true, well);
  dotText("1", pbox.x + 16, pbox.y + 14, 5.0f, accent);
  {
    const float pitch = 4.6f;
    int cols = (int)((sbox.w - 16) / pitch);
    dotBox({sbox.x + 8, sbox.y + 14, 0, 0}, cols, 7, pitch, accent);
    std::string sc = commas(g.mode == MODE_ATTRACT ? g.lastScore : g.score);
    int pad = cols - (int)sc.size() * 6 + 1;
    dotText(sc, sbox.x + 8 + std::max(0, pad) * pitch, sbox.y + 14, pitch, accent);
  }
  // message boxes
  auto msgBox = [&](float y, const std::string *lines, SDL_Color col) {
    SDL_FRect b = {X + 12, y, W - 24, 116};
    bevel(b, true, well);
    const float pitch = 3.55f;
    int cols = (int)((b.w - 16) / pitch);
    dotBox({b.x + 8, b.y + 16, 0, 0}, cols, 26, pitch, col);
    dotText(lines[0], b.x + 8, b.y + 20, pitch, col, cols, true);
    dotText(lines[1], b.x + 8, b.y + 20 + 9 * pitch + 12, pitch, col, cols, true);
  };
  msgBox(sy + 76, g.msg, orange);
  msgBox(sy + 204, g.info, g.golden ? gold : cream);
  // missions: lit when done, blinking while active
  {
    SDL_FRect mb = {X + 12, sy + 332, W - 24, 84};
    bevel(mb, true, well);
    drawText("MISSIONS", mb.x + 12, mb.y + 6, 13, c8(0x8aa08e));
    drawText(std::string("RANK: ") + g.rankName(), mb.x + mb.w - 12, mb.y + 6, 13, accent, 1);
    unsigned cols[] = {0x6fbf4a, 0xf0a35e, 0xf07a5e, 0x9be07a, 0xffd24a};
    for (int i = 0; i < 5; i++) {
      int m = i;
      bool done = (g.missionsDone() >> m) & 1, active = g.activeMission() == m;
      bool on = done || (active && std::fmod(g.time * 2.5, 1.0) < 0.5);
      float cx = mb.x + 42 + i * (mb.w - 84) / 4.f, cy = mb.y + 50;
      SDL_FRect rc = {cx - 14, cy - 14, 28, 28};
      unsigned c = cols[m];
      SDL_SetTextureColorMod(dot_, on ? (c >> 16) & 255 : ((c >> 16) & 255) / 5, on ? (c >> 8) & 255 : ((c >> 8) & 255) / 5,
                             on ? c & 255 : (c & 255) / 5);
      SDL_SetTextureAlphaMod(dot_, 255);
      SDL_RenderTexture(r_, dot_, nullptr, &rc);
      const char *names[] = {"KERNEL", "APK", "SEEDS", "LOOP", "GAUNTLET"};
      drawText(names[m], cx, cy + 14, 11, on ? c8(c) : c8(0x5a6a5e), 0);
    }
  }
  // keys
  drawText("F2 new game   F3 pause   F4 full screen   Esc menu", X + W / 2, Y + H - 26, 13, c8(0x8aa08e), 0);
}

void View::drawMenu(const MenuState &m) {
  SDL_FRect bar = {0, 0, (float)kLogicalW, (float)kMenuH};
  SDL_SetRenderDrawColor(r_, 19, 24, 21, 255);
  SDL_RenderFillRect(r_, &bar);
  SDL_SetRenderDrawColor(r_, 44, 58, 48, 255);
  SDL_RenderLine(r_, 0, kMenuH - 1, kLogicalW, kMenuH - 1);
  for (size_t i = 0; i < m.titles.size(); i++) {
    const SDL_FRect &rc = m.titleRect[i];
    if ((int)i == m.open) {
      SDL_SetRenderDrawColor(r_, 58, 104, 46, 255);
      SDL_RenderFillRect(r_, &rc);
    }
    drawText(m.titles[i], rc.x + 12, rc.y + 3, 15, c8(0xdde6dd));
  }
  if (m.open < 0) return;
  auto &items = m.items[m.open];
  if (items.empty()) return;
  SDL_FRect box = m.itemRect[0];
  box.h = m.itemRect[items.size() - 1].y + m.itemRect[items.size() - 1].h - box.y;
  box.x -= 2; box.y -= 2; box.w += 4; box.h += 4;
  bevel(box, false, {24, 30, 26, 250});
  for (size_t i = 0; i < items.size(); i++) {
    const SDL_FRect &rc = m.itemRect[i];
    if (items[i].separator) {
      SDL_SetRenderDrawColor(r_, 60, 76, 64, 255);
      SDL_RenderLine(r_, rc.x + 6, rc.y + rc.h / 2, rc.x + rc.w - 6, rc.y + rc.h / 2);
      continue;
    }
    if ((int)i == m.hover && items[i].enabled) {
      SDL_SetRenderDrawColor(r_, 58, 104, 46, 255);
      SDL_RenderFillRect(r_, &rc);
    }
    SDL_Color c = items[i].enabled ? c8(0xe8f0e8) : c8(0x6a7a6e);
    if (items[i].checked) drawText("*", rc.x + 8, rc.y + 3, 15, c8(0xf0a35e));
    drawText(items[i].label, rc.x + 26, rc.y + 3, 15, c);
    if (!items[i].key.empty()) drawText(items[i].key, rc.x + rc.w - 10, rc.y + 3, 14, c8(0x9ab09e), 1);
  }
}

void View::draw(const Game &g, const MenuState &menu, bool paused, const std::string &overlay) {
  drawTableTexture(g);
  double fx = fitX_, fy = fitY_;
  if (g.shake > 0) {                                 // a nudge moves the cabinet
    fitX_ += g.shakeDir.x * g.shake * 9;
    fitY_ += g.shakeDir.y * g.shake * 6;
    buildMesh();
  }
  drawCabinet();
  SDL_RenderGeometry(r_, target_, mesh_.data(), (int)mesh_.size(), meshIdx_.data(), (int)meshIdx_.size());
  drawBalls(g, 0);
  SDL_RenderGeometry(r_, overlay_, mesh_.data(), (int)mesh_.size(), meshIdx_.data(), (int)meshIdx_.size());
  drawBalls(g, 1);
  drawParticles(g);
  drawPopups(g);
  if (g.shake > 0) {
    fitX_ = fx;
    fitY_ = fy;
    buildMesh();
  }
  drawPanel(g);
  if (paused || !overlay.empty()) {
    SDL_FRect dim = {0, (float)kMenuH, 800, (float)(kLogicalH - kMenuH)};
    SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r_, 0, 0, 0, overlay.empty() ? 150 : 195);
    SDL_RenderFillRect(r_, &dim);
    if (paused && overlay.empty()) drawText("PAUSED", 412, 360, 48, c8(0xece2b4), 0, FONT_SANS_ITALIC);
    if (!overlay.empty()) {
      // several lines separated by '\n'
      float y = 150;
      size_t a = 0;
      while (a <= overlay.size()) {
        size_t b = overlay.find('\n', a);
        if (b == std::string::npos) b = overlay.size();
        std::string line = overlay.substr(a, b - a);
        bool head = !line.empty() && line[0] == '#';
        if (head) line = line.substr(1);
        if (!line.empty())
          drawText(line, 412, y, head ? 30.f : 17.f, head ? c8(0x6fbf4a) : c8(0xdde6dd), 0,
                   head ? FONT_SANS_ITALIC : FONT_SANS);
        y += head ? 46 : 27;
        a = b + 1;
      }
    }
  }
  drawMenu(menu);
}

}  // namespace pb

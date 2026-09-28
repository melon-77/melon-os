#include "physics.hpp"

#include <algorithm>

namespace pb {

// Restitution and friction of each surface against the ball (typical measured values:
// steel rails and wood bounce little, rubber rings and posts a lot, flipper rubber in between).
const Material kMaterials[MAT_COUNT] = {
    {0.45, 0.20},  // MAT_WALL    painted wood / plastic guides
    {0.40, 0.10},  // MAT_METAL   steel rails and wire gates
    {0.78, 0.30},  // MAT_RUBBER  rubber rings (slingshots, lane guides)
    {0.70, 0.25},  // MAT_POST    rubber posts, bumper skirts
    {0.58, 0.35},  // MAT_FLIPPER flipper rubber
    {0.35, 0.20},  // MAT_TARGET  plastic target faces
    {0.25, 0.10},  // MAT_PLUNGER plunger tip
};

void World::addBall(V2 p, V2 v) {
  Ball b;
  b.p = p;
  b.v = v;
  balls.push_back(b);
}

int World::liveBalls() const {
  int n = 0;
  for (auto &b : balls) n += b.alive;
  return n;
}

void World::eject(int bi, V2 vel) {
  Ball &b = balls[bi];
  b.captured = -1;
  b.v = vel;
  // start just outside the cup's capture zone so it isn't caught again at once
  b.p += norm(vel) * 0.004;
}

void Ramp::finish() {
  cum.assign(pts.size(), 0);
  for (size_t i = 1; i < pts.size(); i++) cum[i] = cum[i - 1] + len(pts[i] - pts[i - 1]);
}

void Ramp::at(double s, V2 &p, V2 &t, double &height, double &dh) const {
  size_t i = 1;
  while (i + 1 < pts.size() && cum[i] < s) i++;
  double segl = std::max(cum[i] - cum[i - 1], 1e-9);
  double f = std::clamp((s - cum[i - 1]) / segl, 0.0, 1.0);
  p = pts[i - 1] + (pts[i] - pts[i - 1]) * f;
  t = norm(pts[i] - pts[i - 1]);
  height = h[i - 1] + (h[i] - h[i - 1]) * f;
  dh = (h[i] - h[i - 1]) / segl;
}

void World::rideRamp(int bi) {
  Ball &b = balls[bi];
  Ramp &r = ramps[0];
  V2 p, t;
  double hgt, dh;
  r.at(b.rampS, p, t, hgt, dh);
  // along the wire: the table's pitch pulls towards +y, the ramp's rise pulls back, the wires take a little
  double a = slopeAccel() * t.y - 5.0 / 7.0 * kG * dh * std::cos(kSlopeDeg * kPi / 180.0);
  a -= (b.rampU > 0 ? 1 : -1) * 0.02 * kG;
  b.rampU += a * kStep;
  b.rampS += b.rampU * kStep;
  if (b.rampS <= 0) {                               // it didn't make it: rolls back out of the mouth
    b.layer = 0;
    b.lift = 0;
    r.at(0, p, t, hgt, dh);
    b.p = p;
    b.v = t * std::min(b.rampU, -0.05);
    events.push_back({EV_RAMP_FAIL, r.enterId, bi, 0, 0});
    return;
  }
  if (b.rampS >= r.total()) {                       // off the end, back onto the playfield
    r.at(r.total(), p, t, hgt, dh);
    b.layer = 0;
    b.lift = 0;
    b.p = p;
    b.v = t * b.rampU;
    events.push_back({EV_SENSOR, r.exitId, bi, b.rampU, 1});
    return;
  }
  r.at(b.rampS, p, t, hgt, dh);
  b.roll += p - b.p;
  b.p = p;
  b.v = t * b.rampU;
  b.lift = hgt;
}

void World::moveFlippers() {
  for (auto &f : flippers) {
    double dir = f.up > f.rest ? 1 : -1;            // direction of the up stroke
    if (f.pressed) {
      f.omega += dir * f.accelUp * kStep;
      if (std::fabs(f.omega) > f.omegaUp) f.omega = dir * f.omegaUp;
    } else {
      f.omega -= dir * f.accelDown * kStep;
      if (std::fabs(f.omega) > f.omegaDown) f.omega = -dir * f.omegaDown;
    }
    f.angle += f.omega * kStep;
    // the stops: the bat hits its end stop and stays there
    double lo = std::min(f.rest, f.up), hi = std::max(f.rest, f.up);
    if (f.angle <= lo) { f.angle = lo; if (f.omega < 0) f.omega = 0; }
    if (f.angle >= hi) { f.angle = hi; if (f.omega > 0) f.omega = 0; }
  }
}

void World::movePlunger() {
  if (!hasPlunger) return;
  Plunger &p = plunger;
  if (p.autoPower >= 0) {                           // an automatic launch: pull fully at once, then release
    p.pos = p.maxPull * std::min(1.0, p.autoPower);
    p.vel = 0;
    p.pulling = false;
    p.autoPower = -1;
    events.push_back({EV_LAUNCH, -1, -1, 0, 0});
    return;
  }
  if (p.pulling) {                                  // the player pulls the knob back at a steady rate
    p.vel = 0;
    p.pos = std::min(p.maxPull, p.pos + 0.11 * kStep);
  } else if (p.pos > 0 || p.vel < 0) {
    // released: a spring (simple harmonic motion) drives the tip up until it hits the stop at pos = 0
    p.vel -= p.springW * p.springW * p.pos * kStep;
    p.pos += p.vel * kStep;
    if (p.pos <= 0) { p.pos = 0; p.vel = 0; }
  }
}

void World::contact(Ball &b, int bi, V2 n, double pen, V2 sv, int mat, Kind kind, int id, double kick) {
  b.p += n * pen;
  V2 vr = b.v - sv;                                 // velocity relative to the surface
  double vn = dot(vr, n);
  if (vn >= 0 && kick <= 0) return;                 // already separating
  const Material &m = kMaterials[mat];
  double impact = vn < 0 ? -vn : 0;
  if (vn < 0) {
    // below a few cm/s the ball is resting or rolling along the surface: no bounce (stops jitter)
    double e = impact < 0.06 ? 0 : m.e;
    double jn = -(1 + e) * vn;
    vr += n * jn;
    // Coulomb friction; a rolling sphere can lose at most 2/7 of its sliding speed to spin
    V2 t = vr - n * dot(vr, n);
    double tl = len(t);
    if (tl > 1e-9) {
      double dv = std::min(m.mu * jn, 2.0 / 7.0 * tl);
      vr -= t / tl * dv;
    }
  }
  // active elements: pop bumpers and slingshots fire a solenoid that throws the ball away
  if (kick > 0) {
    double out = dot(vr, n);
    if (out < kick) vr += n * (kick - out);
  }
  b.v = vr + sv;
  if (id >= 0 && (impact > 0.02 || kick > 0)) events.push_back({EV_HIT, id, bi, impact, 0});
  (void)kind;
}

// Signed distance from point q to a tapered capsule (circle r0 at the origin, r1 at (0,h)), in the
// capsule's own frame, with the outward normal.
static double sdTaper(V2 q, double r0, double r1, double h, V2 &n) {
  double sx = q.x < 0 ? -1 : 1;
  V2 p(std::fabs(q.x), q.y);
  double bb = (r0 - r1) / h, aa = std::sqrt(1 - bb * bb);
  double k = dot(p, V2(-bb, aa));
  double d;
  if (k < 0) { n = norm(p); d = len(p) - r0; }
  else if (k > aa * h) { V2 t = p - V2(0, h); n = norm(t); d = len(t) - r1; }
  else { n = V2(aa, bb); d = dot(p, n) - r0; }
  n.x *= sx;
  return d;
}

void World::collide(int bi) {
  Ball &b = balls[bi];
  const double R = kBallR;

  for (auto &s : segs) {
    if (!s.active || s.layer != b.layer) continue;
    V2 c = closestOnSeg(b.p, s.a, s.b);
    V2 d = b.p - c;
    double dist = len(d);
    double rr = R + s.r;
    if (dist >= rr) continue;
    V2 n = dist > 1e-9 ? d / dist : perp(norm(s.b - s.a));
    if (s.oneway) {
      // a gate: only blocks a ball whose centre is on the blocking side and moving into it
      if (dot(b.p - s.a, s.side) < 0 || dot(b.v, s.side) > 0) continue;
      n = s.side;
      dist = dot(b.p - s.a, s.side);
      if (dist >= rr) continue;
    }
    double kick = 0;
    if (s.kind == K_SLING) {
      // the slingshot's switch closes when the rubber is pushed in hard enough
      double vn = -dot(b.v, n);
      if (vn > 0.18) kick = 1.55;
    }
    if (s.kind == K_DROP) {
      double vn = -dot(b.v, n);
      contact(b, bi, n, rr - dist, {}, s.mat, s.kind, -1, 0);
      if (vn > 0.20) { s.active = false; events.push_back({EV_HIT, s.id, bi, vn, 0}); }
      continue;
    }
    contact(b, bi, n, rr - dist, {}, s.mat, s.kind, s.id, kick);
  }

  for (auto &c : circles) {
    if (!c.active || b.layer != 0) continue;
    V2 d = b.p - c.c;
    double dist = len(d), rr = R + c.r;
    if (dist >= rr) continue;
    V2 n = dist > 1e-9 ? d / dist : V2(0, -1);
    double kick = 0;
    if (c.kind == K_BUMPER && c.cooldown <= 0) {
      kick = 1.9;                                   // a pop bumper's ring pulls down and flings the ball
      c.cooldown = 0.12;
    }
    contact(b, bi, n, rr - dist, {}, c.mat, c.kind, kick > 0 || c.kind != K_BUMPER ? c.id : -1, kick);
  }

  if (b.layer == 0) {
    for (auto &f : flippers) {
      // flipper frame: pivot at the origin, the bat along +y
      V2 ax(std::cos(f.angle), std::sin(f.angle));
      V2 q = b.p - f.pivot;
      V2 local(cross(ax, q) * -1, dot(q, ax));      // x: across the bat, y: along it
      V2 nl;
      double d = sdTaper(local, f.r0, f.r1, f.length, nl);
      if (d >= R) continue;
      // back to table coordinates: local x axis is -perp(ax)... derive from the mapping above
      V2 xw(ax.y, -ax.x);                           // unit vector for local x
      V2 n = norm(xw * nl.x + ax * nl.y);
      V2 cp = b.p - n * (d + R);                    // contact point on the bat's surface
      V2 r = cp - f.pivot;
      V2 sv = perp(r) * f.omega;                    // surface velocity of the rotating bat
      contact(b, bi, n, R - d, sv, MAT_FLIPPER, K_STATIC, -1, 0);
    }
  }

  if (hasPlunger && b.layer == 0) {
    Plunger &p = plunger;
    double top = p.restY + p.pos;
    if (b.p.x > p.x0 && b.p.x < p.x1 && b.p.y + R > top && b.p.y < top + 0.03) {
      contact(b, bi, V2(0, -1), b.p.y + R - top, V2(0, p.vel), MAT_PLUNGER, K_STATIC, -1, 0);
    }
  }

  // other balls: equal masses, nearly elastic
  for (size_t j = bi + 1; j < balls.size(); j++) {
    Ball &o = balls[j];
    if (!o.alive || o.captured >= 0 || o.layer != b.layer) continue;
    V2 d = b.p - o.p;
    double dist = len(d);
    if (dist >= 2 * R || dist < 1e-9) continue;
    V2 n = d / dist;
    double pen = 2 * R - dist;
    b.p += n * (pen / 2);
    o.p -= n * (pen / 2);
    double vn = dot(b.v - o.v, n);
    if (vn < 0) {
      double j2 = -(1 + 0.93) * vn / 2;
      b.v += n * j2;
      o.v -= n * j2;
    }
  }
}

void World::step() {
  events.clear();
  time += kStep;
  moveFlippers();
  movePlunger();
  for (auto &c : circles)
    if (c.cooldown > 0) c.cooldown -= kStep;

  const double g = slopeAccel();
  const double rr = kRollingResistance * kG * std::cos(kSlopeDeg * kPi / 180.0);
  for (size_t i = 0; i < balls.size(); i++) {
    Ball &b = balls[i];
    if (!b.alive) continue;
    if (b.captured >= 0) { b.v = {}; continue; }
    if (b.layer == 1) { rideRamp((int)i); continue; }
    V2 prev = b.p;
    b.v.y += g * kStep;
    b.v += nudge * kStep;
    double sp = len(b.v);
    if (sp > 1e-9) {
      double dv = std::min(sp, rr * kStep);
      b.v -= b.v / sp * dv;
    }
    if (sp > kMaxSpeed) b.v = b.v / sp * kMaxSpeed;
    b.p += b.v * kStep;
    collide((int)i);

    // saucers: inside the cup the ball rolls towards the centre; a slow ball stays there
    for (auto &h : holes) {
      if (!h.enabled || b.layer != 0) continue;
      V2 d = h.c - b.p;
      double dist = len(d);
      if (dist < h.r) {
        b.v += d / std::max(dist, 1e-6) * (6.0 * kStep);  // the cup's slope
        if (dist < 0.006 && len(b.v) < 0.9) {
          b.captured = h.id;
          b.p = h.c;
          b.v = {};
          events.push_back({EV_HOLE, h.id, (int)i, 0, 0});
          break;
        }
      }
    }
    // rollover switches
    for (auto &s : sensors) {
      if (s.layer != b.layer) continue;
      V2 d = s.b - s.a;
      double c0 = cross(d, prev - s.a), c1 = cross(d, b.p - s.a);
      if ((c0 < 0) == (c1 < 0)) continue;
      double t = dot(b.p - s.a, d) / dot(d, d);
      if (t < 0 || t > 1) continue;
      int dir = (b.p.y < prev.y) ? 1 : -1;
      events.push_back({EV_SENSOR, s.id, (int)i, len(b.v), dir});
    }
    // the ramp's mouth: a ball crossing it going up the ramp climbs on
    for (auto &r : ramps) {
      V2 t = norm(r.pts[1] - r.pts[0]);
      V2 n = perp(t);
      double s0 = dot(prev - r.pts[0], t), s1 = dot(b.p - r.pts[0], t);
      if (s0 < 0 && s1 >= 0 && std::fabs(dot(b.p - r.pts[0], n)) < r.mouthHalf && dot(b.v, t) > 0) {
        b.layer = 1;
        b.rampS = s1;
        b.rampU = dot(b.v, t) * 0.95;               // the entrance flap takes a little
        events.push_back({EV_SENSOR, r.enterId, (int)i, b.rampU, 1});
      }
    }
    // visual rolling
    V2 mv = b.p - prev;
    b.roll += mv;
    if (b.p.y > drainY) {
      b.alive = false;
      events.push_back({EV_DRAIN, -1, (int)i, 0, 0});
    }
  }
  nudge = {};
}

}  // namespace pb

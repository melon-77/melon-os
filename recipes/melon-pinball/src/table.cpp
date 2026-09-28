#include "table.hpp"

#include <cmath>

namespace pb {

// Layout coordinates are written in millimetres.
static V2 M(double x, double y) { return {x / 1000.0, y / 1000.0}; }

static void chain(World &w, const std::vector<V2> &pts, bool closed, int mat, double r = 0, int id = -1, Kind kind = K_STATIC) {
  size_t n = pts.size();
  for (size_t i = 0; i + 1 < n + (closed ? 1 : 0); i++) {
    Seg s;
    s.a = pts[i];
    s.b = pts[(i + 1) % n];
    s.r = r;
    s.mat = mat;
    s.id = id;
    s.kind = kind;
    w.segs.push_back(s);
  }
}

static std::vector<V2> mirror(const std::vector<V2> &pts) {
  // the playfield (left of the shooter lane) is symmetric about x = 235 mm
  std::vector<V2> out;
  for (auto it = pts.rbegin(); it != pts.rend(); ++it) out.push_back({0.470 - it->x, it->y});
  return out;
}

static void post(World &w, V2 c, double r, int mat = MAT_POST, int id = E_RUBBER) {
  Circle k;
  k.c = c;
  k.r = r;
  k.mat = mat;
  k.id = id;
  w.circles.push_back(k);
}

static void lamp(Table &t, int idx, const char *name, V2 p, double size, double angDeg, int shape, unsigned color,
                 const char *label = "") {
  if ((int)t.lamps.size() <= idx) t.lamps.resize(idx + 1);
  t.lamps[idx] = {name, p, size, angDeg * kPi / 180.0, shape, color, label};
}

void Table::build() {
  World &w = world;
  w = World();
  lamps.clear();
  outline.clear();
  wallChains.clear();
  guideChains.clear();

  // ---- cabinet: straight sides, a round top that sends the ball from the shooter lane across the table
  std::vector<V2> outer;
  outer.push_back(M(4, 1066));
  for (int i = 0; i <= 48; i++) {
    double a = kPi + kPi * i / 48.0;                 // 180 -> 360 degrees: over the top
    outer.push_back(M(254 + 250 * std::cos(a), 254 + 250 * std::sin(a)));
  }
  outer.push_back(M(504, 1066));
  chain(w, outer, false, MAT_WALL);
  outline = outer;

  // ---- shooter lane: inner wall, the plunger, and a one-way gate at the top
  {
    Seg s;
    s.a = M(468, 300);
    s.b = M(468, 1066);
    s.r = 0.002;
    s.mat = MAT_METAL;
    w.segs.push_back(s);
    guideChains.push_back({s.a, s.b});
    Seg g;
    g.a = M(470, 300);
    g.b = M(504, 264);
    g.mat = MAT_METAL;
    g.oneway = true;
    g.side = norm(V2(-1, -1));
    g.kind = K_GATE;
    w.segs.push_back(g);
    w.hasPlunger = true;
    w.plunger.x0 = 0.470;
    w.plunger.x1 = 0.504;
    w.plunger.restY = 1.000;
    ballStart = V2(0.487, 1.000 - kBallR - 0.0005);
    w.sensors.push_back({M(470, 330), M(504, 330), E_SHOOTER});
  }

  // ---- rind loop: a lane up the left side that follows the round top over to the right
  {
    Seg s;
    s.a = M(50, 545);                                // ends 32 mm (clear of a ball) above the exit gate
    s.b = M(50, 330);
    s.r = 0.003;
    s.mat = MAT_METAL;
    w.segs.push_back(s);
    guideChains.push_back({s.a, s.b});
    Seg g;                                           // balls coming down the loop roll off under its wall
    g.a = M(4, 545);
    g.b = M(50, 600);
    g.mat = MAT_METAL;
    g.oneway = true;
    g.side = norm(perp(g.b - g.a));
    if (g.side.y > 0) g.side = g.side * -1;
    g.kind = K_GATE;
    w.segs.push_back(g);
    w.sensors.push_back({M(4, 625), M(50, 625), E_ORBIT_ENTER});
    spinnerA = M(4, 650);
    spinnerB = M(50, 650);
    w.sensors.push_back({spinnerA, spinnerB, E_SPINNER});
    w.sensors.push_back({M(402, 106), M(433, 75), E_ORBIT_TOP});
  }

  // ---- M-E-L-O-N: five standups on an angled bank facing the right flipper (and the mini flipper),
  // on an island that fills the corner between the bank and the loop's wall
  {
    V2 A = M(58, 530), B = M(158, 397);              // lower-left end, upper-right end
    V2 d = norm(B - A), n = perp(d);                 // n: the targets' face normal
    if (n.x < 0) n = n * -1;                         // facing right and down, towards the flippers
    melonA = A;
    melonN = B;
    const char *mel[] = {"M", "E", "L", "O", "N"};
    const double tl = 0.030, gap = 0.004;
    for (int i = 0; i < 5; i++) {
      // M at the top, N at the bottom
      V2 s0 = B - d * (i * (tl + gap)), s1 = s0 - d * tl;
      Seg s;
      s.a = s0;
      s.b = s1;
      s.mat = MAT_TARGET;
      s.kind = K_STANDUP;
      s.id = E_TARGET_M + i;
      s.r = 0.0015;
      w.segs.push_back(s);
      lamp(*this, LA_M + i, mel[i], (s0 + s1) * 0.5 + n * 0.024, 0.0085, 0, L_ROUND, 0x6fbf4a, mel[i]);
    }
    // the island behind the bank: its top slopes down to the right so nothing rests against the loop wall
    V2 back = n * -0.003;
    std::vector<V2> island = {M(53, 330), B + back + d * 0.002, A + back - d * 0.002, M(53, 540)};
    chain(w, island, true, MAT_WALL);
    wallChains.push_back(island);
  }

  // ---- top lanes A-P-K
  for (int i = 0; i < 4; i++) {
    double x = 175 + 50 * i;
    Seg s;
    s.a = M(x, 95);
    s.b = M(x, 150);
    s.r = 0.003;
    s.mat = MAT_RUBBER;
    s.id = E_RUBBER;
    w.segs.push_back(s);
    guideChains.push_back({s.a, s.b});
  }
  const char *apk[] = {"A", "P", "K"};
  for (int i = 0; i < 3; i++) {
    double x = 200 + 50 * i;
    w.sensors.push_back({M(x - 22, 128), M(x + 22, 128), E_LANE_A + i});
    lamp(*this, LA_LANE_A + i, apk[i], M(x, 108), 0.0095, 0, L_ROUND, 0xf0a35e, apk[i]);
  }

  // ---- three pop bumpers: "the melons"
  V2 bp[3] = {M(190, 235), M(300, 235), M(245, 320)};
  for (int i = 0; i < 3; i++) {
    Circle c;
    c.c = bp[i];
    c.r = 0.024;
    c.mat = MAT_POST;
    c.kind = K_BUMPER;
    c.id = E_BUMPER1 + i;
    bumperCircle[i] = (int)w.circles.size();
    w.circles.push_back(c);
  }

  // ---- the gauntlet portal: a kickout saucer in the middle
  portal = M(240, 452);
  w.holes.push_back({portal, 0.020, E_PORTAL, true});

  // ---- seed bank: three drop targets on the right, with a backstop behind them
  {
    V2 a = M(410, 390), b = M(440, 480);
    V2 d = norm(b - a);
    double L = len(b - a), tl = 0.028, gap = (L - 3 * tl) / 2;
    for (int i = 0; i < 3; i++) {
      Seg s;
      s.a = a + d * (i * (tl + gap));
      s.b = s.a + d * tl;
      s.mat = MAT_TARGET;
      s.kind = K_DROP;
      s.id = E_DROP1 + i;
      s.r = 0.002;
      dropSeg[i] = (int)w.segs.size();
      w.segs.push_back(s);
      V2 c = (s.a + s.b) * 0.5 + V2(-0.949, 0.316) * 0.022;
      lamp(*this, LA_DROP1 + i, "seed", c, 0.0070, 0, L_ROUND, 0xf0a35e);
    }
    // the backstop sits right behind the targets: a dropped target leaves a shallow slot, never a pocket
    // narrower than the ball that could wedge it
    std::vector<V2> block = {M(417.6, 387.5), M(466, 371), M(466, 480), M(447.6, 477.5)};
    chain(w, block, true, MAT_WALL);
    wallChains.push_back(block);
    chain(w, {M(410, 390), M(417.6, 387.5)}, false, MAT_WALL);
    chain(w, {M(440, 480), M(447.6, 477.5)}, false, MAT_WALL);
  }

  // ---- lower playfield: outlane/inlane blocks, slingshots, flippers (the right side mirrors the left)
  // the inlane's lower edge ends over the flipper's flat top at the bat's own rest angle, so a ball rolls
  // straight onto the flipper (and a raised flipper cradles it in the V between the two)
  std::vector<V2> blockL = {M(36, 692), M(44, 692), M(44, 870), M(150, 934), M(126, 962), M(126, 1066), M(36, 1066)};
  std::vector<V2> blockR = mirror(blockL);
  for (auto *bl : {&blockL, &blockR}) {
    chain(w, *bl, true, MAT_WALL);
    wallChains.push_back(*bl);
  }
  post(w, M(40, 692), 0.0045);
  post(w, M(430, 692), 0.0045);

  auto sling = [&](V2 A, V2 B, V2 C, int id) {
    std::vector<V2> tri = {A, B, C};
    wallChains.push_back(tri);
    Seg face;                                        // the kicking rubber
    face.a = A;
    face.b = C;
    face.r = 0.005;
    face.mat = MAT_RUBBER;
    face.kind = K_SLING;
    face.id = id;
    w.segs.push_back(face);
    chain(w, {A, B}, false, MAT_RUBBER, 0.005, E_RUBBER);
    chain(w, {B, C}, false, MAT_WALL, 0.005);
    post(w, A, 0.006);
    post(w, B, 0.006);
    post(w, C, 0.006);
  };
  sling(M(80, 735), M(80, 828), M(124, 856), E_SLING_L);
  sling(M(390, 735), M(390, 828), M(346, 856), E_SLING_R);

  Flipper fl;
  fl.pivot = M(137, 948);
  fl.rest = 30 * kPi / 180;
  fl.up = -25 * kPi / 180;
  fl.angle = fl.rest;
  Flipper fr = fl;
  fr.pivot = M(333, 948);
  fr.rest = kPi - fl.rest;
  fr.up = kPi - fl.up;
  fr.angle = fr.rest;
  // the mini flipper on the right wall under the seed bank: shoots up-left at M-E-L-O-N and the portal
  Flipper fm = fl;
  fm.pivot = M(453, 560);
  fm.length = 0.050;
  fm.r0 = 0.0090;
  fm.r1 = 0.0055;
  fm.rest = kPi - 30 * kPi / 180;
  fm.up = kPi + 22 * kPi / 180;
  fm.angle = fm.rest;
  fm.accelUp = 2600;
  fm.omegaUp = 34;
  w.flippers = {fl, fr, fm};

  // kickback: a switch near the bottom of the left outlane, the plunger below it
  kicker = M(20, 985);
  w.sensors.push_back({M(4, 930), M(36, 930), E_KICKBACK});
  // magnet under the playfield in front of the portal (off unless a run turns it on)
  magnet = M(240, 575);
  magnetHole = (int)w.holes.size();
  Hole mg;
  mg.c = magnet;
  mg.r = 0.045;
  mg.id = E_MAGNET;
  mg.enabled = false;
  mg.pull = 9.0;
  mg.damping = 7.0;
  mg.catchSpeed = 0.35;
  w.holes.push_back(mg);

  w.sensors.push_back({M(4, 780), M(36, 780), E_OUTLANE_L});
  w.sensors.push_back({M(44, 780), M(75, 780), E_INLANE_L});
  w.sensors.push_back({M(395, 780), M(426, 780), E_INLANE_R});
  w.sensors.push_back({M(434, 780), M(466, 780), E_OUTLANE_R});

  // ---- the kernel ramp: up the right, over the top lanes, down the left into the left inlane
  {
    Ramp r;
    const double pts[][3] = {{345, 600, 0},  {360, 545, 15}, {378, 470, 32}, {392, 380, 48}, {395, 300, 58},
                             {386, 225, 62}, {360, 170, 64}, {320, 138, 64}, {270, 125, 64}, {215, 128, 63},
                             {165, 145, 62}, {125, 180, 60}, {100, 230, 57}, {84, 300, 52},  {76, 380, 45},
                             {70, 470, 36},  {66, 560, 26},  {63, 640, 16},  {61, 700, 7},   {60, 745, 0}};
    for (auto &p : pts) {
      r.pts.push_back(M(p[0], p[1]));
      r.h.push_back(p[2] / 1000.0);
    }
    r.enterId = E_RAMP_ENTER;
    r.exitId = E_RAMP_EXIT;
    r.finish();
    // the mouth: two rails on the playfield lead into it
    V2 E = r.pts[0], t = norm(r.pts[1] - r.pts[0]), n = perp(t);
    for (double sgn : {-1.0, 1.0}) {
      Seg s;
      s.a = E + n * (sgn * (r.mouthHalf + 0.004)) - t * 0.045;
      s.b = E + n * (sgn * (r.mouthHalf + 0.004));
      s.r = 0.002;
      s.mat = MAT_METAL;
      w.segs.push_back(s);
      guideChains.push_back({s.a, s.b});
    }
    w.ramps.push_back(r);
  }

  // ---- lamps
  const char *mis[] = {"K", "A", "S", "R", "G"};   // same order as the missions in game.cpp
  const char *misName[] = {"kernel", "apk", "seeds", "rind", "gauntlet"};
  unsigned misCol[] = {0x6fbf4a, 0xf0a35e, 0xf07a5e, 0x9be07a, 0xffd24a};
  for (int i = 0; i < 5; i++) {
    double a = (90 + 72 * i) * kPi / 180.0;
    lamp(*this, LA_MISSION1 + i, misName[i], portal + V2(std::cos(a), std::sin(a)) * 0.052, 0.0085, 0, L_ROUND,
         misCol[i], mis[i]);
  }
  lamp(*this, LA_PORTAL, "portal", portal, 0.034, 0, L_ROUND, 0xffd24a);
  lamp(*this, LA_EXTRA_BALL, "extra ball", M(240, 548), 0.0100, 0, L_ROUND, 0xf0a35e, "EB");
  lamp(*this, LA_JACKPOT, "jackpot", M(240, 380), 0.012, 0, L_RECT, 0xffd24a, "JACKPOT");
  lamp(*this, LA_ORBIT_ARROW, "rind loop", M(27, 612), 0.014, -90, L_ARROW, 0x9be07a, "LOOP");
  lamp(*this, LA_RAMP_ARROW, "kernel ramp", M(328, 670), 0.016, -74.7, L_ARROW, 0xf0a35e, "KERNEL");
  lamp(*this, LA_DROP_ARROW, "seed bank", M(368, 520), 0.014, -57, L_ARROW, 0xf07a5e, "SEEDS");
  lamp(*this, LA_PORTAL_ARROW, "portal", M(240, 612), 0.016, -90, L_ARROW, 0xffd24a, "PORTAL");
  const char *mult[] = {"2x", "3x", "4x", "5x"};
  for (int i = 0; i < 4; i++)
    lamp(*this, LA_MULT2 + i, "bonus", M(183 + 38 * i, 715), 0.0095, 0, L_ROUND, 0x6fbf4a, mult[i]);
  lamp(*this, LA_SHOOT_AGAIN, "shoot again", M(235, 1030), 0.016, 0, L_RECT, 0xf0a35e, "SHOOT AGAIN");
  lamp(*this, LA_OUTLANE_L, "outlane", M(20, 830), 0.0065, 0, L_ROUND, 0xf07a5e);
  lamp(*this, LA_INLANE_L, "inlane", M(60, 830), 0.0065, 0, L_ROUND, 0x6fbf4a);
  lamp(*this, LA_INLANE_R, "inlane", M(410, 830), 0.0065, 0, L_ROUND, 0x6fbf4a);
  lamp(*this, LA_OUTLANE_R, "outlane", M(450, 830), 0.0065, 0, L_ROUND, 0xf07a5e);
  lamp(*this, LA_SKILL, "skill", M(487, 600), 0.0090, -90, L_ARROW, 0xffd24a);
  lamp(*this, LA_KICKBACK, "kickback", M(20, 890), 0.013, -90, L_ARROW, 0xf0a35e, "KICK");
  lamp(*this, LA_MAGNET, "magnet", magnet, 0.012, 0, L_ROUND, 0x9ab8ff);
}

void Table::resetTargets() {
  for (int i = 0; i < 3; i++) world.segs[dropSeg[i]].active = true;
}

}  // namespace pb

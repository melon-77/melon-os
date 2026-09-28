// Headless checks of the table physics: launches, flipper shots, cradling, the ramp, and no ball ever
// leaving the cabinet. Run: melon-pinball-physics-test (built with -DPINBALL_TESTS=ON).
#include <cstdio>
#include <cstdlib>
#include <random>

#include "../src/table.hpp"

using namespace pb;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; std::printf("FAIL: " __VA_ARGS__); std::printf("\n"); } \
                           else { std::printf("ok:   " __VA_ARGS__); std::printf("\n"); } } while (0)

static bool inside(const Table &t, const Ball &b) {
  (void)t;
  return b.p.x > -0.001 && b.p.x < 0.509 && b.p.y > -0.001 && std::isfinite(b.p.x) && std::isfinite(b.p.y);
}

static void run(Table &t, double secs) {
  int n = (int)(secs / kStep);
  for (int i = 0; i < n; i++) t.world.step();
}

int main() {
  Table t;
  t.build();

  // 1. a full plunge sends the ball out of the shooter lane, over the top and into the playfield
  {
    t.build();
    t.world.addBall(t.ballStart);
    run(t, 0.5);
    t.world.plunger.pulling = true;
    run(t, 0.7);
    t.world.plunger.pulling = false;
    double minY = 9;
    bool shooter = false, leftLane = false;
    for (int i = 0; i < 6000; i++) {
      t.world.step();
      for (auto &e : t.world.events) if (e.type == EV_SENSOR && e.id == E_SHOOTER) shooter = true;
      auto &b = t.world.balls[0];
      minY = std::min(minY, b.p.y);
      if (b.p.x < 0.466 && b.p.y > 0.2) leftLane = true;
    }
    CHECK(shooter, "full plunge leaves the shooter lane");
    CHECK(minY < 0.06, "full plunge reaches the top arc (min y %.3f m)", minY);
    CHECK(leftLane, "plunged ball comes down in the playfield");
  }
  // 2. a weak plunge falls back onto the plunger
  {
    t.build();
    t.world.addBall(t.ballStart);
    run(t, 0.3);
    t.world.plunger.pulling = true;
    run(t, 0.06);
    t.world.plunger.pulling = false;
    run(t, 4);
    auto &b = t.world.balls[0];
    CHECK(b.p.x > 0.47 && b.p.y > 0.9, "weak plunge falls back to the plunger (%.3f, %.3f)", b.p.x, b.p.y);
  }
  // 3. a ball dropped onto a raised left flipper comes to rest on it (a cradle)
  {
    t.build();
    t.world.flippers[0].pressed = true;
    run(t, 0.1);
    t.world.addBall(V2(0.060, 0.80));        // down the left inlane
    run(t, 4);
    auto &b = t.world.balls[0];
    CHECK(b.alive && len(b.v) < 0.02, "ball cradles on the raised left flipper (speed %.4f m/s at %.3f, %.3f)",
          len(b.v), b.p.x, b.p.y);
  }
  // 4. from a cradle, a drop-catch-flip sends the ball up the table
  {
    double best = 9;
    double bestT = 0;
    for (int k = 0; k < 40; k++) {
      t.build();
      t.world.flippers[0].pressed = true;
      run(t, 0.1);
      t.world.addBall(V2(0.060, 0.80));
      run(t, 3);
      t.world.flippers[0].pressed = false;
      run(t, 0.02 + k * 0.01);
      t.world.flippers[0].pressed = true;
      double minY = 9;
      for (int i = 0; i < 3000; i++) { t.world.step(); minY = std::min(minY, t.world.balls[0].p.y); }
      if (minY < best) { best = minY; bestT = 0.02 + k * 0.01; }
    }
    CHECK(best < 0.3, "a flipper shot reaches the upper playfield (best min y %.3f m, flip after %.2f s)", best, bestT);
  }
  // 4b. the mini flipper: a ball rolling onto it from the seed bank can be shot up-left across the table
  {
    double best = 9;
    bool hitMelon = false;
    for (int k = 0; k < 60; k++) {
      t.build();
      t.world.addBall(V2(0.450, 0.50), V2(-0.08, 0.25));   // falling off the seed bank
      run(t, 0.05 + k * 0.015);
      t.world.flippers[2].pressed = true;
      for (int i = 0; i < 4000; i++) {
        t.world.step();
        best = std::min(best, t.world.balls[0].p.x);
        for (auto &e : t.world.events)
          if (e.type == EV_HIT && e.id >= E_TARGET_M && e.id <= E_TARGET_N) hitMelon = true;
      }
    }
    CHECK(best < 0.25 && hitMelon, "the mini flipper shoots across the table (min x %.3f m) and can hit M-E-L-O-N", best);
  }
  // 4c. a shot from the right flipper reaches M-E-L-O-N
  {
    bool hit = false;
    for (int k = 0; k < 40 && !hit; k++) {
      t.build();
      t.world.flippers[1].pressed = true;
      run(t, 0.1);
      t.world.addBall(V2(0.410, 0.80));              // down the right inlane
      run(t, 3);
      t.world.flippers[1].pressed = false;
      run(t, 0.02 + k * 0.01);
      t.world.flippers[1].pressed = true;
      for (int i = 0; i < 3000; i++) {
        t.world.step();
        for (auto &e : t.world.events)
          if (e.type == EV_HIT && e.id >= E_TARGET_M && e.id <= E_TARGET_N) hit = true;
      }
    }
    CHECK(hit, "a drop-catch-flip from the right flipper hits M-E-L-O-N");
  }
  // 5. the ramp: a fast ball rides it to the left inlane, a slow one rolls back
  {
    t.build();
    Ramp &r = t.world.ramps[0];
    V2 tdir = norm(r.pts[1] - r.pts[0]);
    t.world.addBall(r.pts[0] - tdir * 0.03, tdir * 3.0);
    bool exited = false, failed = false;
    for (int i = 0; i < 8000; i++) {
      t.world.step();
      for (auto &e : t.world.events) {
        if (e.type == EV_SENSOR && e.id == E_RAMP_EXIT) exited = true;
        if (e.type == EV_RAMP_FAIL) failed = true;
      }
    }
    CHECK(exited && !failed, "a 3 m/s shot makes the ramp");
    t.build();
    t.world.addBall(r.pts[0] - tdir * 0.03, tdir * 1.1);
    exited = failed = false;
    for (int i = 0; i < 8000; i++) {
      t.world.step();
      for (auto &e : t.world.events) {
        if (e.type == EV_SENSOR && e.id == E_RAMP_EXIT) exited = true;
        if (e.type == EV_RAMP_FAIL) failed = true;
      }
    }
    CHECK(failed && !exited, "a 1.1 m/s shot rolls back down the ramp");
  }
  // 6. chaos: many random balls, random flipping and bumpers, nobody escapes the cabinet
  {
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> ux(0.06, 0.44), uy(0.1, 0.8), uv(-5, 5), u01(0, 1);
    int escaped = 0, drained = 0, total = 0, stuck = 0;
    for (int trial = 0; trial < 150; trial++) {
      t.build();
      for (int k = 0; k < 3; k++) t.world.addBall(V2(ux(rng), uy(rng)), V2(uv(rng), uv(rng)));
      total += 3;
      for (int i = 0; i < 40000; i++) {
        if (i % 400 == 0) {
          t.world.flippers[0].pressed = u01(rng) < 0.5;
          t.world.flippers[1].pressed = t.world.flippers[2].pressed = u01(rng) < 0.5;
        }
        t.world.step();
        for (size_t bi = 0; bi < t.world.balls.size(); bi++) {
          auto &b = t.world.balls[bi];
          if (b.captured >= 0 && u01(rng) < 0.001) t.world.eject((int)bi, V2(-0.3, 1.0));
          if (b.alive && !inside(t, b)) { escaped++; b.alive = false;
            std::printf("  escaped at %.3f %.3f v %.2f %.2f\n", b.p.x, b.p.y, b.v.x, b.v.y); }
        }
      }
      for (auto &b : t.world.balls) { if (!b.alive) drained++; else if (len(b.v) < 1e-4 && b.captured < 0 && b.p.y < 0.9) { stuck++; std::printf("  at rest: %.3f %.3f\n", b.p.x, b.p.y); } }
    }
    CHECK(escaped == 0, "no ball escapes the cabinet (%d of %d escaped, %d drained, %d at rest mid-table)", escaped, total,
          drained, stuck);
  }
  std::printf(fails ? "%d FAILED\n" : "all passed\n", fails);
  return fails ? 1 : 0;
}

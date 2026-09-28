// Headless checks of the rules: a Harvest run through the Seed Market, melons and pests acting on the
// machine, the kickback, and a lost run. Built with -DPINBALL_TESTS=ON as melon-pinball-game-test.
#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>

#include "../src/game.hpp"

using namespace pb;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; std::printf("FAIL: " __VA_ARGS__); } else { std::printf("ok:   " __VA_ARGS__); } \
                           std::printf("\n"); } while (0)

static void play(Game &g, double secs) {
  for (double t = 0; t < secs; t += 1.0 / 120) g.update(1.0 / 120);
}

static void clearBalls(Game &g) {
  for (auto &b : g.table.world.balls) b.alive = false;
}

int main() {
  // keep the unlock and score files out of the real home
  char tmpl[] = "/tmp/melon-pinball-test-XXXXXX";
  const char *home = mkdtemp(tmpl);
  if (home) setenv("HOME", home, 1);
  std::srand(7);

  Game g;
  g.init(false);
  g.startRun(PK_CANTALOUPE);
  CHECK(g.mode == MODE_PLAY && g.run.target == 20000 && g.run.ballsLeft == 1 && g.run.seeds == 4,
        "a run starts on season 1's sprout field: target %lld, %d spare ball, %d seeds", g.run.target, g.run.ballsLeft,
        g.run.seeds);

  // reaching the target harvests the field and opens the Seed Market
  g.run.fieldScore = g.run.target;
  play(g, 3.5);
  CHECK(g.mode == MODE_SHOP, "reaching the target opens the Seed Market (mode %d)", (int)g.mode);
  CHECK(g.run.seeds == 8, "sprout field pays 3 + 1 for the unused ball: %d seeds", g.run.seeds);
  CHECK(g.run.shopMelons.size() == 3 && g.run.shopGraft >= 0, "the market has 3 melons and a graft");

  // buying and selling
  g.run.seeds = 20;
  g.run.shopMelons[0] = ML_CANTALOUPE;
  int before = g.run.seeds;
  CHECK(g.buyMelon(0) && g.run.has(ML_CANTALOUPE) && g.run.seeds == before - kMelons[ML_CANTALOUPE].price, "buying a melon");
  CHECK(!g.buyMelon(0), "a sold slot can't be bought again");
  int s0 = g.run.seeds;
  CHECK(g.reroll() && g.run.seeds == s0 - 2 && g.run.rerollCost == 3, "a reroll costs 2, then 3");
  g.run.shopMelons[1] = ML_MAGNET_SEED;
  g.buyMelon(1);
  int s1 = g.run.seeds;
  CHECK(g.sellMelon(1) && !g.run.has(ML_MAGNET_SEED) && g.run.seeds == s1 + kMelons[ML_MAGNET_SEED].price / 2,
        "selling a melon gives half its price back");
  g.run.shopGraft = GR_EXTRA_VINE;
  CHECK(g.buyGraft() && g.run.grafts.size() == 1, "buying a graft");
  g.nextField();
  CHECK(g.mode == MODE_PLAY && g.run.field == 1 && g.run.target == 30000 && g.run.ballsLeft == 2,
        "the vine field: target %lld, the graft adds a ball (%d spare)", g.run.target, g.run.ballsLeft);

  // Cantaloupe: a pop bumper hit scores 1,000 + 2,000
  {
    clearBalls(g);
    long long sc = g.run.fieldScore;
    const Circle &c = g.table.world.circles[g.table.bumperCircle[0]];
    g.table.world.addBall(c.c + V2(0, -0.045), V2(0, 0.6));
    play(g, 0.12);
    long long gained = g.run.fieldScore - sc;
    CHECK(gained >= 3000, "Cantaloupe: a bumper hit scores %lld (>= 3,000)", gained);
  }
  // the kickback throws a ball in the left outlane back up
  {
    clearBalls(g);
    g.kickbackLit = true;
    g.table.world.addBall(V2(0.020, 0.88), V2(0, 0.9));
    play(g, 0.15);
    const Ball &b = g.table.world.balls.back();
    CHECK(b.alive && b.v.y < -0.5 && !g.kickbackLit, "the kickback fires (ball v.y %.2f m/s)", b.v.y);
  }
  // the spinner scores as it turns
  {
    clearBalls(g);
    long long sc = g.run.fieldScore;
    g.table.world.addBall(V2(0.020, 0.70), V2(0, -2.2));   // up the left outlane, clear of its divider
    play(g, 0.6);
    CHECK(g.run.fieldScore - sc >= 150 * 4, "a shot through the spinner scores %lld", g.run.fieldScore - sc);
  }
  // melons change the machine
  {
    g.run.melons.push_back({ML_OVERCLOCK});
    g.run.melons.push_back({ML_MAGNET_SEED});
    g.run.fieldScore = g.run.target;               // win this field
    play(g, 3.5);
    g.nextField();                                 // the pest field
    CHECK(g.run.field == 2 && g.run.pest >= 0, "the third field of a season has a pest (%s)",
          g.run.pest >= 0 ? kPests[g.run.pest].name : "none");
    CHECK(g.table.world.flippers[0].power > 1.0 || g.run.pest == PE_FROST, "Overclock: flipper power %.2f",
          g.table.world.flippers[0].power);
    CHECK(g.magnetOn, "Magnet Seed switches the magnet on");
  }
  // every pest, under Harvest Moon and without
  {
    g.run.pest = PE_LOCK;
    g.run.melons.clear();
    CHECK(g.pestActive(PE_LOCK), "a pest is active without Harvest Moon");
    g.run.melons.push_back({ML_HARVEST_MOON});
    CHECK(!g.pestActive(PE_LOCK), "Harvest Moon cancels the pest");
    g.run.melons.clear();
  }
  // Magnet Seed: a ball rolling past the magnet is caught and fed into the portal
  {
    Game h;
    h.init(false);
    h.startRun(PK_CANTALOUPE);
    h.run.melons.push_back({ML_MAGNET_SEED});
    h.run.fieldScore = h.run.target;
    play(h, 3.5);
    h.nextField();
    clearBalls(h);
    int sinks = h.run.portalSinks;
    h.table.world.addBall(h.table.magnet + V2(0.03, -0.03), V2(-0.25, 0.2));
    play(h, 3.0);
    CHECK(h.run.portalSinks > sinks, "the magnet catches a passing ball and feeds it to the portal");
  }
  // losing: both balls drain short of the target ends the run
  {
    Game h;
    h.init(false);
    h.startRun(PK_CANTALOUPE);
    for (int k = 0; k < 2; k++) {
      clearBalls(h);
      h.table.world.addBall(V2(0.235, 1.05), V2(0, 1));   // straight down the middle
      play(h, 3.2);
    }
    CHECK(h.mode == MODE_RUN_OVER, "two drained balls short of the target end the run (mode %d)", (int)h.mode);
  }
  // A-P-K lanes light M-E-L-O-N letters: all three lanes spell three letters
  {
    Game h;
    h.init(false);
    h.newGame();
    clearBalls(h);
    for (double x : {0.200, 0.250, 0.300}) {
      h.table.world.addBall(V2(x, 0.105), V2(0, 0.4));
      play(h, 0.25);
      clearBalls(h);
    }
    int lit = 0;
    for (int i = LA_M; i <= LA_N; i++) lit += h.lamp[i] > 0.5f;
    CHECK(lit >= 3, "three A-P-K lanes light %d letters of M-E-L-O-N", lit);
  }
  std::printf(fails ? "%d FAILED\n" : "all passed\n", fails);
  return fails ? 1 : 0;
}

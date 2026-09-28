#include "game.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace pb {

// Missions: spell M-E-L-O-N to light the portal, sink the portal to start the next mission.
const char *kMissionNames[5] = {"COMPILE KERNEL", "APK UPGRADE", "SEED HARVEST", "RIND RUNNER", "THE GAUNTLET"};
const char *kMissionGoals[5] = {"RAMP 3 TIMES", "SPELL A-P-K TWICE", "CLEAR SEED BANK 2X", "LOOP 3 TIMES",
                                "PORTAL 3 TIMES"};
static const int kMissionNeed[5] = {3, 2, 2, 3, 3};
static const char *kRanks[] = {"SEEDLING", "SPROUT", "VINE", "BLOSSOM", "RIPE MELON", "GOLDEN MELON"};

static std::string upper(std::string s) {
  for (auto &c : s) c = (char)std::toupper((unsigned char)c);
  return s;
}
static std::string commas(long long v) {
  std::string s = std::to_string(v < 0 ? -v : v), o;
  int n = 0;
  for (int i = (int)s.size() - 1; i >= 0; i--) {
    o.insert(o.begin(), s[i]);
    if (++n % 3 == 0 && i > 0) o.insert(o.begin(), ',');
  }
  return v < 0 ? "-" + o : o;
}
static double frand() { return (std::rand() % 10000) / 10000.0; }

void Game::init(bool gold) {
  golden = gold;
  unlocks.load();
  unlocks.survivor = gold;
  table.build();
  applyMachine();
  const char *u = std::getenv("USER");
  if (u && *u) playerName = upper(u).substr(0, 10);
  loadScores();
  mode = MODE_ATTRACT;
  table.world.balls.clear();
  table.world.addBall(table.ballStart);
  message("F2 HARVEST RUN", "F5 CLASSIC GAME", 1e9);
  updateInfo();
}

// ---------------------------------------------------------------- classic
void Game::newGame() {
  harvest = false;
  table.build();
  applyMachine();
  score = 0;
  ball = 1;
  extraBalls = 0;
  missionsDone_ = 0;
  missionCount_ = 0;
  missionNext_ = 0;
  mission_ = -1;
  missionReady_ = false;
  extraLit_ = false;
  particles.clear();
  popups.clear();
  goldRain = 0;
  mode = MODE_PLAY;
  resetBall();
  serveBall(false);
  kickbackLit = true;
  sfx(SFX_START);
  message("WELCOME TO", "MELON PLANET", 3);
}

// ---------------------------------------------------------------- Harvest
void Game::newRun() {
  int n = 0;
  for (int p = 0; p < PK_COUNT; p++) n += unlocks.hasPack(p);
  if (n <= 1) { startRun(PK_CANTALOUPE); return; }
  mode = MODE_PACK_SELECT;
  table.world.balls.clear();
  message("CHOOSE A", "SEED PACK", 1e9);
}

void Game::startRun(int pack) {
  harvest = true;
  run = Run();
  run.pack = pack;
  score = 0;
  missionsDone_ = 0;
  missionCount_ = 0;
  missionNext_ = 0;
  mission_ = -1;
  missionReady_ = false;
  extraLit_ = false;
  extraBalls = 0;
  particles.clear();
  popups.clear();
  goldRain = 0;
  if (pack == PK_HONEYDEW) run.seeds = 12;
  if (pack == PK_WATERMELON) run.ballsPerField = 3;
  if (pack == PK_BITTER) run.melons.push_back({ML_BITTER});
  if (pack == PK_GAUNTLET) run.melons.push_back({ML_SURVIVOR});
  if (pack == PK_WILD) {
    run.priceBump = 1;
    std::vector<int> pool;
    for (int m = 0; m < ML_COUNT; m++)
      if (unlocks.hasMelon(m) && m != ML_SURVIVOR) pool.push_back(m);
    for (int k = 0; k < 2 && !pool.empty(); k++) {
      int i = std::rand() % pool.size();
      run.melons.push_back({pool[i]});
      pool.erase(pool.begin() + i);
    }
  }
  sfx(SFX_START);
  startField();
}

void Game::toAttract() {
  mode = MODE_ATTRACT;
  harvest = false;
  table.build();
  applyMachine();
  table.world.addBall(table.ballStart);
  message("F2 HARVEST RUN", "F5 CLASSIC GAME", 1e9);
}

bool Game::pestActive(int p) const {
  return harvest && run.pest == p && !run.has(ML_HARVEST_MOON);
}

void Game::startField() {
  table.build();
  run.target = fieldTarget(run.season, run.field, run.pack);
  run.activeGrafts = run.grafts;
  run.grafts.clear();
  auto graft = [&](int g) { return std::find(run.activeGrafts.begin(), run.activeGrafts.end(), g) != run.activeGrafts.end(); };
  if (graft(GR_FERTILIZER)) run.target = run.target * 3 / 4 / 1000 * 1000;
  run.pest = -1;
  if (run.field == 2 || (run.pack == PK_BITTER && run.field == 1)) {
    std::vector<int> pool;
    for (int p = 0; p < PE_COUNT; p++)
      if (std::find(run.pestsSeen.begin(), run.pestsSeen.end(), p) == run.pestsSeen.end()) pool.push_back(p);
    if (pool.empty())
      for (int p = 0; p < PE_COUNT; p++) pool.push_back(p);
    run.pest = pool[std::rand() % pool.size()];
    run.pestsSeen.push_back(run.pest);
  }
  run.fieldScore = 0;
  run.fieldBumpers = 0;
  run.insuranceUsed = false;
  run.ballsLeft = run.ballsPerField + (graft(GR_EXTRA_VINE) ? 1 : 0) - 1;
  ball = 1;
  extraBalls = 0;
  mode = MODE_PLAY;
  applyMachine();
  resetBall();
  serveBall(false);
  kickbackLit = !pestActive(PE_WEEVIL);
  if (graft(GR_POLLINATOR)) lightLetter(5);
  if (graft(GR_SUNLAMP)) bonusMult = 4;
  char t[40];
  std::snprintf(t, sizeof t, "SEASON %d %s", run.season, run.field == 2 ? "PEST" : (run.field == 1 ? "VINE" : "SPROUT"));
  message(t, "TARGET " + commas(run.target), 4);
  if (run.pest >= 0) {
    popup(upper(kPests[run.pest].name), 0xff7a5a, 2.6f);
    if (run.has(ML_HARVEST_MOON)) popup("HARVEST MOON", 0xe8e0b0, 2.6f);
  } else {
    popup(fieldName(run.field), 0x9be07a, 2.0f);
  }
}

// melons and pests change the machine itself
void Game::applyMachine() {
  World &w = table.world;
  double power = 1, grav = 1, bump = 1.9, sling = 1.55;
  if (harvest) {
    if (run.has(ML_OVERCLOCK)) power *= 1.15;
    if (run.has(ML_BITTER)) power *= 0.88;
    if (pestActive(PE_FROST)) power *= 0.75;
    if (run.has(ML_FEATHERWEIGHT)) grav *= 0.8;
    if (pestActive(PE_HEAT)) grav *= 1.3;
    if (run.has(ML_ECHO_BUMPER)) bump *= 1.3;
    if (pestActive(PE_BLIGHT)) sling = 0;
  }
  for (auto &f : w.flippers) f.power = power;
  w.gravityScale = grav;
  w.bumperKick = bump;
  w.slingKick = sling;
  w.wind = {};
  for (auto &h : w.holes)
    if (h.id == E_PORTAL) h.enabled = !pestActive(PE_LOCK);
  magnetOn = harvest && (run.has(ML_MAGNET_SEED) || pestActive(PE_MAGNET));
  w.holes[table.magnetHole].enabled = magnetOn;
}

double Game::currentMult() const {
  if (!harvest) return 1;
  double m = 1;
  if (run.has(ML_BITTER)) m *= 2;
  if (run.has(ML_GLASS)) m *= 2.5;
  if (run.has(ML_GOLDEN_RIND) && table.world.liveBalls() >= 2) m *= 3;
  if (run.has(ML_LAST_SEED) && run.ballsLeft == 0 && extraBalls == 0) m *= 3;
  if (run.has(ML_SURVIVOR)) m *= 1.5 + 0.5 * run.pestsBeaten;
  for (auto &o : run.melons)
    if (o.id == ML_BALLER) m *= 1 + o.counter;
  if (run.has(ML_WILD_GROWTH)) {
    int kinds = 0;
    for (int t = 1; t < T_TAG_COUNT; t++) kinds += tagsHit_[t];
    m *= 1 + 0.1 * kinds;
  }
  return m;
}

void Game::add(long long pts, long long bonusPts, ScoreTag tag) {
  if (tilted || mode != MODE_PLAY) return;
  double mult = 1;
  long long base = pts;
  if (harvest) {
    tagsHit_[tag] = true;
    switch (tag) {
      case T_BUMPER:
        if (pestActive(PE_DROUGHT)) { base = 0; break; }
        if (run.has(ML_CANTALOUPE)) base += 2000;
        if (run.has(ML_ECHO_BUMPER)) mult *= 1.5;
        if (run.has(ML_TENTH_SEED) && run.fieldBumpers % 10 == 0) {
          mult *= 20;
          popup("TENTH SEED X20", 0xffe0a0, 1.2f);
        }
        break;
      case T_SLING: if (run.has(ML_SLICE_SHOT)) base += 3000; break;
      case T_SPINNER: if (run.has(ML_WHIRLIGIG)) mult *= 5; break;
      case T_LOOP: if (run.has(ML_RIND_RUNNER)) mult *= 3; break;
      case T_RAMP: if (run.has(ML_KERNEL_PANIC)) mult *= 4; break;
      case T_PORTAL: if (run.has(ML_PORTAL_KEY)) mult *= 5; break;
      default: break;
    }
    mult *= currentMult();
  }
  long long v = (long long)std::llround(base * mult);
  score += v;
  if (harvest) run.fieldScore += v;
  bonus += bonusPts;
}

void Game::giveSeeds(int n, const char *why) {
  run.seeds += n;
  sfx(SFX_COIN, 0.7f);
  if (why) message(why, "+" + std::to_string(n) + (n == 1 ? " SEED" : " SEEDS"), 1.8);
  if (run.seeds >= 20) unlock(ML_COMPOST);
}

void Game::unlock(int m) {
  if (unlocks.melon[m] || !kMelons[m].unlock || m == ML_SURVIVOR) return;
  unlocks.melon[m] = true;
  unlocks.save();
  run.unlockedThisRun.push_back(kMelons[m].name);
  popups.push_back({"UNLOCKED: " + upper(kMelons[m].name), 0, 2.6f, 0x9ab8ff});
  sfx(SFX_UNLOCK);
}

void Game::unlockPack(int p) {
  if (unlocks.pack[p] || p == PK_GAUNTLET) return;
  unlocks.pack[p] = true;
  unlocks.save();
  run.unlockedThisRun.push_back(kPacks[p].name);
  popups.push_back({"UNLOCKED: " + upper(kPacks[p].name), 0, 2.6f, 0x9ab8ff});
  sfx(SFX_UNLOCK);
}

void Game::fieldWon() {
  mode = MODE_FIELD_WON;
  modeTimer = 2.8;
  for (auto &b : table.world.balls)
    if (b.alive) {
      seeds(b.p, 14, 0xffd24a, 0.4);
      b.alive = false;
    }
  for (auto &f : table.world.flippers) f.pressed = false;
  table.world.plunger.pulling = false;
  popups.push_back({"HARVESTED!", 0, 2.6f, 0xffd24a});
  sfx(SFX_MISSION);
  goldRain = std::max(goldRain, 1.8);
  // the seeds for this field
  payout.clear();
  int base = 3 + run.field;
  payout.push_back({std::string(fieldName(run.field)), base});
  int spare = run.ballsLeft + extraBalls;
  if (spare > 0) payout.push_back({"UNUSED BALLS", spare});
  int cap = run.has(ML_COMPOST) ? 10 : 5;
  int interest = std::min(cap, run.seeds / 5);
  if (interest > 0) payout.push_back({"INTEREST", interest});
  if (run.has(ML_SEEDLESS)) payout.push_back({"SEEDLESS", 3});
  int total = 0;
  for (auto &p : payout) total += p.second;
  run.seeds += total;
  // unlocks earned on this field
  if (run.pest >= 0) { run.pestsBeaten++; unlock(ML_HARVEST_MOON); }
  if (spare == 0) unlock(ML_LAST_SEED);
  if (run.fieldBumpers >= 100) unlock(ML_TENTH_SEED);
  if (run.seeds >= 20) unlock(ML_COMPOST);
  // glass melons may shatter
  for (size_t i = 0; i < run.melons.size();) {
    if (run.melons[i].id == ML_GLASS && std::rand() % 6 == 0) {
      run.melons.erase(run.melons.begin() + i);
      popups.push_back({"GLASS MELON SHATTERED", 0, 2.4f, 0xa8d8c8});
      sfx(SFX_SHATTER);
      continue;
    }
    i++;
  }
  // on to the next field or season
  run.field++;
  if (run.field == 3) {
    int cleared = run.season;
    if (cleared >= 2) { unlock(ML_BITTER); unlockPack(PK_HONEYDEW); }
    if (cleared >= 4) unlockPack(PK_WATERMELON);
    if (cleared >= 6) unlockPack(PK_BITTER);
    run.field = 0;
    run.season++;
    if (run.season >= 4) unlock(ML_GLASS);
    if (run.season > kSeasons) {
      run.won = true;
      unlockPack(PK_WILD);
    }
  }
  unlocks.bestSeason = std::max(unlocks.bestSeason, std::min(run.season, kSeasons));
  unlocks.save();
  message("HARVESTED", "+" + std::to_string(total) + " SEEDS", 3);
}

void Game::runOver() {
  mode = MODE_RUN_OVER;
  modeTimer = 30;
  lastScore = score;
  for (auto &f : table.world.flippers) f.pressed = false;
  unlocks.bestSeason = std::max(unlocks.bestSeason, run.season);
  unlocks.save();
  if (run.won) {
    message("HARVEST COMPLETE", "YOU FED THE PLANET", 1e9);
    goldRain = 8;
    sfx(SFX_JACKPOT);
  } else {
    message("THE HARVEST FAILED", "SEASON " + std::to_string(run.season) + (run.field == 2 ? " PEST" : (run.field == 1 ? " VINE" : " SPROUT")), 1e9);
    sfx(SFX_GAME_OVER);
  }
}

// ---------------------------------------------------------------- the Seed Market
void Game::stockShop() {
  run.shopMelons.clear();
  std::vector<int> pool[3];
  for (int m = 0; m < ML_COUNT; m++)
    if (unlocks.hasMelon(m) && !run.has(m)) pool[kMelons[m].rarity].push_back(m);
  for (int k = 0; k < 3; k++) {
    double r = frand();
    int rar = r < 0.62 ? R_COMMON : (r < 0.9 ? R_UNCOMMON : R_RARE);
    // fall back to another rarity when one runs dry
    for (int tries = 0; tries < 3 && pool[rar].empty(); tries++) rar = (rar + 1) % 3;
    if (pool[rar].empty()) { run.shopMelons.push_back(-1); continue; }
    int i = std::rand() % pool[rar].size();
    run.shopMelons.push_back(pool[rar][i]);
    pool[rar].erase(pool[rar].begin() + i);
  }
  int g;
  do g = std::rand() % GR_COUNT; while (g == GR_TRELLIS && run.slots >= 7);
  run.shopGraft = g;
}

bool Game::buyMelon(int slot) {
  if (mode != MODE_SHOP || slot < 0 || slot >= (int)run.shopMelons.size()) return false;
  int m = run.shopMelons[slot];
  if (m < 0 || run.seeds < run.price(m) || run.count() >= run.slots) return false;
  run.seeds -= run.price(m);
  run.melons.push_back({m});
  run.shopMelons[slot] = -1;
  sfx(SFX_BUY);
  return true;
}

bool Game::buyGraft() {
  if (mode != MODE_SHOP || run.shopGraft < 0) return false;
  int g = run.shopGraft, price = kGrafts[g].price + run.priceBump;
  if (run.seeds < price) return false;
  run.seeds -= price;
  if (g == GR_TRELLIS) run.slots++;
  else run.grafts.push_back(g);
  run.shopGraft = -1;
  sfx(SFX_BUY);
  return true;
}

bool Game::sellMelon(int idx) {
  if (mode != MODE_SHOP || idx < 0 || idx >= run.count()) return false;
  run.seeds += run.sellValue(idx);
  run.melons.erase(run.melons.begin() + idx);
  sfx(SFX_COIN);
  return true;
}

bool Game::reroll() {
  if (mode != MODE_SHOP || run.seeds < run.rerollCost) return false;
  run.seeds -= run.rerollCost;
  run.rerollCost++;
  stockShop();
  sfx(SFX_BUY, 0.6f);
  return true;
}

void Game::nextField() {
  if (mode != MODE_SHOP) return;
  if (run.won) { runOver(); return; }
  startField();
}

// ---------------------------------------------------------------- per ball
void Game::resetBall() {
  for (bool &a : apk_) a = false;
  for (bool &m : melon_) m = false;
  for (bool &t : tagsHit_) t = false;
  table.resetTargets();
  dropsBanked_ = 0;
  bonus = 0;
  bonusMult = 1;
  tilted = false;
  tiltMeter_ = 0;
  tiltWarnings_ = 0;
  multiball_ = false;
  jackpotLit_ = false;
  bumperHits_ = 0;
  ballSave_ = 0;
  ballSaveArmed_ = true;
  insureNext_ = false;
  pendingBalls_ = 0;
  launchQueue_ = -1;
  portalHold_ = magnetHold_ = -1;
}

void Game::serveBall(bool autoLaunch) {
  table.world.balls.erase(std::remove_if(table.world.balls.begin(), table.world.balls.end(),
                                         [](const Ball &b) { return !b.alive; }),
                          table.world.balls.end());
  table.world.addBall(table.ballStart);
  inShooterLane_ = true;
  skillLane_ = (int)(SDL_GetTicks() / 7 % 3);
  skillTime_ = 0;
  if (autoLaunch) table.world.plunger.autoPower = 0.95;
}

bool Game::plungerReady() const {
  for (auto &b : table.world.balls)
    if (b.alive && b.p.x > 0.470 && b.p.y > 0.9) return true;
  return false;
}

void Game::sfx(Sfx s, float vol, double x) {
  if (sound && mode != MODE_ATTRACT) sound(s, vol, (float)((x - 0.254) / 0.254));
}

void Game::message(const std::string &a, const std::string &b, double secs) {
  msg[0] = a;
  msg[1] = b;
  msgTimer_ = secs;
}

void Game::popup(const std::string &s, unsigned color, float dur) {
  if (mode != MODE_PLAY && mode != MODE_FIELD_WON) return;
  popups.push_back({s, 0, dur, color});
  if (popups.size() > 3) popups.erase(popups.begin());
}

void Game::seeds(V2 at, int n, unsigned color, double speed) {
  for (int i = 0; i < n; i++) {
    double a = frand() * 2 * kPi, s = speed * (0.4 + frand() * 0.8);
    Particle p;
    p.p = at;
    p.v = V2(std::cos(a), std::sin(a)) * s;
    p.z = 0.02;
    p.vz = 0.3 + frand() * 0.5;
    p.maxLife = p.life = 0.7f + (float)frand() * 0.5f;
    p.color = color;
    p.kind = (i % 3 == 0) ? 1 : 0;
    p.spin = (float)(frand() * 360);
    particles.push_back(p);
  }
  if (particles.size() > 400) particles.erase(particles.begin(), particles.begin() + (particles.size() - 400));
}

int Game::rankIndex() const {
  int n = 0;
  for (int i = 0; i < 5; i++) n += (missionsDone_ >> i) & 1;
  return n;
}
const char *Game::rankName() const { return kRanks[rankIndex()]; }

void Game::flipper(int side, bool down) {
  if (mode == MODE_ATTRACT) return;
  bool want = down && !tilted && mode == MODE_PLAY;
  // the right button also works the mini flipper
  for (int k : side == 0 ? std::vector<int>{0} : std::vector<int>{1, 2}) {
    Flipper &f = table.world.flippers[k];
    bool pressed = want && !f.pressed;
    if (want != f.pressed && k != 2) sfx(want ? SFX_FLIP_UP : SFX_FLIP_DOWN, want ? 1.f : 0.5f, f.pivot.x);
    f.pressed = want;
    // lane change: each flip moves the lit A-P-K lanes along
    if (pressed && k != 2) {
      bool t[3];
      for (int i = 0; i < 3; i++) t[i] = apk_[i];
      for (int i = 0; i < 3; i++) apk_[i] = side == 0 ? t[(i + 1) % 3] : t[(i + 2) % 3];
    }
  }
}

void Game::plunger(bool down) {
  if (mode != MODE_PLAY) return;
  Plunger &p = table.world.plunger;
  if (down && !p.pulling) sfx(SFX_PULL, 0.5f, 0.49);
  if (!down && p.pulling) sfx(SFX_LAUNCH, (float)(0.3 + p.pos / p.maxPull), 0.49);
  p.pulling = down;
}

void Game::nudge(V2 dir) {
  if (mode != MODE_PLAY || tilted) return;
  // a shove of the cabinet: a short sharp acceleration of the table under the ball
  nudgeSteps_ = 24;                 // 12 ms at 15 m/s^2: the ball gains about 0.18 m/s against the table
  nudgeDir_ = dir;
  shake = 1;
  shakeDir = dir;
  sfx(SFX_NUDGE, 0.7f);
  tiltMeter_ += harvest && run.has(ML_STEADY_HANDS) ? 0.5 : 1.0;
  if (tiltMeter_ > 3.2) {
    tilted = true;
    for (auto &f : table.world.flippers) f.pressed = false;
    message("TILT", "", 4);
    popup("TILT", 0xff5a3a, 2.5f);
    sfx(SFX_TILT);
  } else if (tiltMeter_ > 1.9) {
    tiltWarnings_++;
    message("DANGER", "", 1.5);
    sfx(SFX_WARNING);
  }
}

// ---------------------------------------------------------------- M-E-L-O-N and missions
void Game::lightLetter(int n) {
  for (int k = 0; k < 5 && n > 0; k++)
    if (!melon_[k]) { melon_[k] = true; n--; }
  bool all = true;
  for (bool m : melon_) all = all && m;
  if (all) spelled();
}

void Game::spelled() {
  run.melonsSpelled++;
  for (auto &o : run.melons)
    if (o.id == ML_BALLER) o.counter += 0.5;
  if (harvest && run.melonsSpelled >= 5) unlock(ML_BALLER);
  if (!missionReady_ && mission_ < 0) {
    missionReady_ = true;
    add(50000, 5000, T_MELON);
    message("M-E-L-O-N", std::string("PORTAL: ") + kMissionNames[missionNext_], 3.5);
    popup("MELON", 0x6fbf4a);
  } else {
    for (bool &m : melon_) m = false;
    add(25000, 5000, T_MELON);
    message("M-E-L-O-N", "25,000", 2);
  }
}

void Game::startMission(int m) {
  mission_ = m;
  missionProgress_ = 0;
  missionReady_ = false;
  for (bool &x : melon_) x = false;
  if (m == 2) table.resetTargets();
  message(kMissionNames[m], kMissionGoals[m], 4);
  popup(kMissionNames[m], 0x9be07a, 2.2f);
  sfx(SFX_MISSION);
}

void Game::progressMission(int m, int amount) {
  if (mission_ != m) return;
  missionProgress_ += amount;
  if (missionProgress_ >= kMissionNeed[m]) {
    completeMission();
  } else {
    char b[32];
    std::snprintf(b, sizeof b, "%d MORE TO GO", kMissionNeed[m] - missionProgress_);
    message(kMissionNames[m], b, 2.5);
  }
}

void Game::completeMission() {
  int m = mission_;
  missionsDone_ |= 1 << m;
  missionCount_++;
  run.missionsDone++;
  mission_ = -1;
  long long award = 250000LL * missionCount_;
  add(award, 25000, T_MISSION);
  message(std::string(kMissionNames[m]) + " DONE", std::string("RANK ") + rankName(), 4);
  popup("MISSION COMPLETE", 0xffd24a, 2.4f);
  sfx(SFX_MISSION);
  if (harvest && run.missionsDone >= 3) unlock(ML_WILD_GROWTH);
  if (m == 4) {                                     // through the gauntlet: the other side
    goldRain = 6;
    popup("THE OTHER SIDE", 0xffd24a, 3.0f);
    unlock(ML_GOLDEN_RIND);
  }
  if (missionCount_ == 2 || missionCount_ == 4) {
    extraLit_ = true;
    message("EXTRA BALL", "IS LIT AT THE PORTAL", 3);
  }
  missionNext_ = -1;
  for (int i = 1; i <= 5; i++)
    if (!((missionsDone_ >> ((m + i) % 5)) & 1)) { missionNext_ = (m + i) % 5; break; }
  if (missionNext_ < 0) {                           // all five: seed storm
    startMultiball();
    missionsDone_ = 0;
    missionNext_ = 0;
  }
}

void Game::startMultiball() {
  multiball_ = true;
  jackpotLit_ = true;
  pendingBalls_ += 2;
  launchQueue_ = 0.3;
  ballSave_ = 15;
  message("SEED STORM", "JACKPOT AT THE PORTAL", 5);
  popup("SEED STORM", 0xf0a35e, 2.5f);
  sfx(SFX_MULTIBALL);
  unlock(ML_TWIN_SEEDS);
}

// ---------------------------------------------------------------- draining
void Game::ballDrained(int bi) {
  (void)bi;
  if (mode != MODE_PLAY) return;
  sfx(SFX_DRAIN, 0.8f);
  int live = table.world.liveBalls();
  if (!tilted && insureNext_) {
    insureNext_ = false;
    run.insuranceUsed = true;
    message("CROP INSURANCE", "BALL RETURNED", 2);
    popup("INSURED", 0xc0d0f0);
    pendingBalls_++;
    launchQueue_ = 0.8;
    return;
  }
  if (!tilted && ballSave_ > 0 && !inShooterLane_) {
    message("BALL SAVED", "", 2);
    popup("BALL SAVED", 0x9be07a);
    sfx(SFX_SAVE);
    pendingBalls_++;
    launchQueue_ = 0.8;
    if (live == 0) ballSave_ = 0;
    return;
  }
  if (live + pendingBalls_ >= 1) {                 // multiball goes on while any ball is left
    if (live + pendingBalls_ == 1) {
      multiball_ = false;
      jackpotLit_ = false;
    }
    return;
  }
  multiball_ = false;
  jackpotLit_ = false;
  endOfBall();
}

void Game::endOfBall() {
  mode = MODE_BALL_END;
  endTimer_ = 2.6;
  long long b = tilted ? 0 : bonus * bonusMult;
  if (harvest && !tilted) b = (long long)std::llround(b * currentMult());
  char l1[40];
  std::snprintf(l1, sizeof l1, "BONUS %s X %d", commas(bonus).c_str(), bonusMult);
  message(tilted ? "TILT" : l1, tilted ? "NO BONUS" : "= " + commas(b), 2.6);
  score += b;
  if (harvest) run.fieldScore += b;
  sfx(SFX_BONUS);
  for (auto &f : table.world.flippers) f.pressed = false;
}

// ---------------------------------------------------------------- switches
void Game::handle(const Event &e) {
  World &w = table.world;
  if (e.type == EV_DRAIN) { ballDrained(e.ball); return; }
  if (e.type == EV_LAUNCH) return;
  if (e.type == EV_RAMP_FAIL) { sfx(SFX_WALL, 0.4f); return; }
  double x = e.ball >= 0 ? w.balls[e.ball].p.x : 0.254;
  if (e.type == EV_HOLE && e.id == E_MAGNET) {
    magnetHold_ = run.has(ML_MAGNET_SEED) ? 0.8 : 1.6;
    magnetBall_ = e.ball;
    sfx(SFX_MAGNET, 1, x);
    return;
  }
  if (e.type == EV_HOLE && e.id == E_PORTAL) {
    portalHold_ = 1.2;
    portalBall_ = e.ball;
    run.portalSinks++;
    if (harvest && run.portalSinks >= 10) unlock(ML_PORTAL_KEY);
    sfx(SFX_PORTAL_IN, 1, x);
    seeds(table.portal, 18, 0xffd24a, 0.35);
    add(25000, 5000, T_PORTAL);
    if (harvest && run.has(ML_PORTAL_KEY)) giveSeeds(1, nullptr);
    if (harvest && run.has(ML_TWIN_SEEDS)) {
      pendingBalls_++;
      launchQueue_ = std::max(launchQueue_, 0.6);
      popup("TWIN SEEDS", 0xf0a35e, 1.4f);
    }
    if (jackpotLit_) {
      add(1000000, 0, T_JACKPOT);
      popup("JACKPOT", 0xffd24a, 2.4f);
      message("JACKPOT", "1,000,000", 3);
      sfx(SFX_JACKPOT);
      goldRain = std::max(goldRain, 2.5);
    } else if (extraLit_) {
      extraLit_ = false;
      extraBalls++;
      popup("EXTRA BALL", 0xf0a35e, 2.4f);
      message("EXTRA BALL", "SHOOT AGAIN", 3);
      sfx(SFX_EXTRA_BALL);
    } else if (missionReady_ && mission_ < 0) {
      startMission(missionNext_);
    } else if (mission_ == 4) {
      progressMission(4);
    } else {
      message("THE OTHER SIDE", "25,000", 2);
    }
    return;
  }
  if (e.type == EV_SENSOR) {
    switch (e.id) {
      case E_SHOOTER:
        if (inShooterLane_) {
          inShooterLane_ = false;
          if (ballSaveArmed_) {
            ballSave_ = 10;
            if (harvest) {
              if (run.has(ML_HONEY_TRAP)) ballSave_ += 12;
              if (std::find(run.activeGrafts.begin(), run.activeGrafts.end(), GR_IRRIGATION) != run.activeGrafts.end())
                ballSave_ = std::max(ballSave_, 30.0);
              if (pestActive(PE_WEEVIL)) ballSave_ = 0;
            }
            ballSaveArmed_ = false;
          }
          skillTime_ = 5;
        }
        break;
      case E_LANE_A: case E_LANE_P: case E_LANE_K: {
        int i = e.id - E_LANE_A;
        sfx(SFX_ROLLOVER, 0.8f, x);
        if (skillTime_ > 0 && i == skillLane_) {
          add(75000, 5000, T_SKILL);
          popup("SKILL SHOT", 0xffd24a);
          message("SKILL SHOT", "75,000", 2.5);
        }
        skillTime_ = 0;
        add(1000, 1000, T_LANE);
        apk_[i] = true;
        lightLetter(1);                             // every lane lights the next letter of M-E-L-O-N
        if (apk_[0] && apk_[1] && apk_[2]) {
          for (bool &a : apk_) a = false;
          bool cache = harvest && run.has(ML_APK_CACHE);
          bonusMult = std::min(cache ? 9 : 5, bonusMult + (cache ? 2 : 1));
          add(20000, 0, T_APK);
          if (cache) lightLetter(2);
          message("APK UPGRADE", "BONUS " + std::to_string(bonusMult) + "X", 2.5);
          popup("APK UPGRADE", 0xf0a35e);
          progressMission(1);
        }
        break;
      }
      case E_INLANE_L: case E_INLANE_R:
        sfx(SFX_ROLLOVER, 0.6f, x);
        add(1000, 500, T_INLANE);
        break;
      case E_OUTLANE_L: case E_OUTLANE_R:
        sfx(SFX_ROLLOVER, 0.6f, x);
        add(5000, 500, T_OUTLANE);
        if (e.dir < 0 && harvest && run.has(ML_CROP_INSURANCE) && !run.insuranceUsed) insureNext_ = true;
        break;
      case E_KICKBACK:
        if (e.dir < 0 && kickbackLit && !tilted && !pestActive(PE_WEEVIL)) {
          Ball &b = w.balls[e.ball];
          b.p.y = std::min(b.p.y, 0.925);
          b.v = V2(0.03, -1.45);
          kickbackLit = false;
          kickFire = 0.25;
          insureNext_ = false;
          sfx(SFX_KICKBACK, 1, x);
          message("KICKBACK", "", 1.5);
          if (harvest && run.has(ML_KICKBACK_VINE)) kickRelight_ = 15;
        }
        break;
      case E_SPINNER:
        spinnerOmega_ = std::max(spinnerOmega_, e.speed * 60.0);
        break;
      case E_ORBIT_ENTER:
        if (e.dir > 0) orbitClock_ = 3.0;
        break;
      case E_ORBIT_TOP:
        if (orbitClock_ > 0) {
          orbitClock_ = -1;
          add(10000, 2000, T_LOOP);
          sfx(SFX_LOOP, 1, x);
          message("RIND LOOP", "10,000", 1.5);
          progressMission(3);
        }
        break;
      case E_RAMP_ENTER:
        sfx(SFX_RAMP, 0.8f, x);
        break;
      case E_RAMP_EXIT:
        add(15000, 3000, T_RAMP);
        sfx(SFX_RAMP, 1, x);
        message("KERNEL RAMP", "15,000", 1.5);
        seeds(w.ramps[0].pts.back(), 10, 0xf0a35e, 0.3);
        progressMission(0);
        break;
    }
    return;
  }
  if (e.type != EV_HIT || tilted) return;
  float vol = (float)std::clamp(e.speed / 2.0, 0.15, 1.0);
  switch (e.id) {
    case E_BUMPER1: case E_BUMPER2: case E_BUMPER3: {
      int i = e.id - E_BUMPER1;
      bumperFlash[i] = 1;
      bumperHits_++;
      run.fieldBumpers++;
      add(1000, 100, T_BUMPER);
      sfx(SFX_BUMPER, 1, x);
      seeds(w.circles[table.bumperCircle[i]].c, 7, 0xece2b4, 0.35);
      break;
    }
    case E_SLING_L: case E_SLING_R:
      add(100, 10, T_SLING);
      sfx(SFX_SLING, 1, x);
      break;
    case E_TARGET_M: case E_TARGET_E: case E_TARGET_L: case E_TARGET_O: case E_TARGET_N: {
      int i = e.id - E_TARGET_M;
      sfx(SFX_TARGET, vol, x);
      add(melon_[i] ? 1000 : 5000, 1000, T_LETTER);
      if (!melon_[i]) {
        melon_[i] = true;
        bool all = true;
        for (bool m : melon_) all = all && m;
        if (all) spelled();
      }
      break;
    }
    case E_DROP1: case E_DROP2: case E_DROP3: {
      sfx(SFX_DROP, 1, x);
      add(5000, 1000, T_DROP);
      bool all = true;
      for (int k = 0; k < 3; k++) all = all && !w.segs[table.dropSeg[k]].active;
      if (all) {
        dropsBanked_++;
        add(25000, 5000, T_BANK);
        message("SEED BANK", "25,000", 2);
        seeds(V2(0.425, 0.435), 20, 0xf0a35e, 0.4);
        dropReset_ = 1.2;
        progressMission(2);
        if (harvest && run.has(ML_SEED_MONEY)) giveSeeds(1, "SEED MONEY");
      } else if (pestActive(PE_MOLE)) {
        dropReset_ = 0.45;                          // the mole pushes them back up
      }
      break;
    }
    case E_RUBBER:
      add(10, 0, T_RUBBER);
      sfx(SFX_RUBBER, vol, x);
      break;
    default:
      if (e.speed > 0.4) sfx(SFX_WALL, vol * 0.6f, x);
  }
}

// ---------------------------------------------------------------- lamps and displays
void Game::updateLamps() {
  auto blink = [&](double hz) { return std::fmod(time * hz, 1.0) < 0.5 ? 1.f : 0.f; };
  auto set = [&](int i, float v) { lamp[i] += (v - lamp[i]) * 0.35f; };
  if (mode == MODE_ATTRACT || mode == MODE_PACK_SELECT || mode == MODE_RUN_OVER) {   // attract: chase the lamps
    for (int i = 0; i < LA_COUNT; i++) set(i, std::fmod(time * 1.3 + i * 0.137, 1.0) < 0.3 ? 1.f : 0.f);
    return;
  }
  if (tilted || pestActive(PE_FOG)) {
    for (int i = 0; i < LA_COUNT; i++) set(i, 0);
    return;
  }
  for (int i = 0; i < 3; i++) set(LA_LANE_A + i, apk_[i] ? 1.f : (skillTime_ > 0 && i == skillLane_ ? blink(6) : 0.f));
  for (int i = 0; i < 5; i++) set(LA_M + i, melon_[i] ? 1.f : (missionReady_ ? blink(3) : 0.f));
  for (int i = 0; i < 3; i++) set(LA_DROP1 + i, table.world.segs[table.dropSeg[i]].active ? 0.f : 1.f);
  for (int i = 0; i < 5; i++) {
    float v = (missionsDone_ >> i) & 1 ? 1.f : 0.f;
    if (mission_ == i) v = blink(2.5);
    if (missionReady_ && missionNext_ == i) v = blink(5);
    set(LA_MISSION1 + i, v);
  }
  bool portalOpen = !pestActive(PE_LOCK);
  set(LA_PORTAL, !portalOpen ? 0.f : (jackpotLit_ ? blink(6) : (missionReady_ || extraLit_ || mission_ == 4 ? blink(2) : 0.2f)));
  set(LA_EXTRA_BALL, extraLit_ ? blink(3) : 0.f);
  set(LA_JACKPOT, jackpotLit_ ? blink(4) : 0.f);
  set(LA_ORBIT_ARROW, mission_ == 3 ? blink(3) : 0.f);
  set(LA_RAMP_ARROW, mission_ == 0 ? blink(3) : 0.f);
  set(LA_DROP_ARROW, mission_ == 2 ? blink(3) : 0.f);
  set(LA_PORTAL_ARROW, portalOpen && (missionReady_ || jackpotLit_ || extraLit_ || mission_ == 4) ? blink(3) : 0.f);
  for (int i = 0; i < 4; i++) set(LA_MULT2 + i, bonusMult >= i + 2 ? 1.f : 0.f);
  set(LA_SHOOT_AGAIN, ballSave_ > 0 ? (ballSave_ < 3 ? blink(6) : 1.f) : (extraBalls > 0 ? 1.f : 0.f));
  set(LA_INLANE_L, 0.f);
  set(LA_INLANE_R, 0.f);
  set(LA_OUTLANE_L, insureNext_ ? 1.f : 0.f);
  set(LA_OUTLANE_R, insureNext_ ? 1.f : 0.f);
  set(LA_SKILL, inShooterLane_ ? blink(2) : 0.f);
  set(LA_KICKBACK, kickbackLit ? 1.f : (kickRelight_ > 0 ? blink(1) * 0.4f : 0.f));
  set(LA_MAGNET, magnetOn ? (magnetCool_ > 0 ? 0.25f : (magnetHold_ > 0 ? blink(8) : 0.8f)) : 0.f);
}

void Game::updateInfo() {
  if (mode == MODE_ATTRACT) {
    bool best = std::fmod(time, 10.0) < 5;
    if (best && unlocks.bestSeason > 0) {
      info[0] = "BEST HARVEST";
      info[1] = "SEASON " + std::to_string(unlocks.bestSeason);
    } else {
      info[0] = "HIGH SCORE";
      info[1] = hiscores.empty() ? "0" : hiscores[0].name.substr(0, 8) + " " + commas(hiscores[0].score);
    }
    return;
  }
  if (mode == MODE_PACK_SELECT) { info[0] = "ARROWS TO CHOOSE"; info[1] = "ENTER TO PLANT"; return; }
  if (mode == MODE_SHOP) {
    info[0] = run.won ? "THE HARVEST IS IN" : std::string("NEXT ") + fieldName(run.field);
    info[1] = run.won ? "" : "TARGET " + commas(fieldTarget(run.season, run.field, run.pack));
    return;
  }
  if (mode == MODE_RUN_OVER) {
    info[0] = run.won ? "ALL 8 SEASONS" : "REACHED SEASON " + std::to_string(run.season);
    info[1] = "F2 FOR A NEW RUN";
    return;
  }
  int page = (int)(time / 4.0) % 4;
  if (harvest && page == 0) {
    info[0] = "SEASON " + std::to_string(run.season) + " " + (run.field == 2 ? "PEST" : (run.field == 1 ? "VINE" : "SPROUT"));
    info[1] = "TARGET " + commas(run.target);
    return;
  }
  if (harvest && page == 1 && run.pest >= 0) {
    info[0] = upper(kPests[run.pest].name);
    info[1] = run.has(ML_HARVEST_MOON) ? "HARVEST MOON" : kPests[run.pest].lcd;
    return;
  }
  if (harvest && page == 2) {
    char b[32];
    std::snprintf(b, sizeof b, "MULT X%.1f", currentMult());
    info[0] = std::to_string(run.seeds) + " SEEDS";
    info[1] = b;
    return;
  }
  if (mission_ >= 0) {
    char b[40];
    std::snprintf(b, sizeof b, "%s %d/%d", kMissionGoals[mission_], missionProgress_, kMissionNeed[mission_]);
    info[0] = kMissionNames[mission_];
    info[1] = b;
  } else if (missionReady_) {
    info[0] = "SINK THE PORTAL";
    info[1] = std::string("FOR ") + kMissionNames[missionNext_];
  } else {
    info[0] = "HIT M-E-L-O-N TO";
    info[1] = "SELECT MISSION";
  }
}

void Game::attractAI(double dt) {
  // the demo plays itself: flip when a ball comes down onto a flipper
  World &w = table.world;
  for (size_t s = 0; s < w.flippers.size(); s++) {
    Flipper &f = w.flippers[s];
    attractFlip_[s] -= dt;
    bool want = false;
    V2 ax(std::cos(f.rest), std::sin(f.rest));
    for (auto &b : w.balls) {
      if (!b.alive || b.layer) continue;
      V2 d = b.p - f.pivot;
      double along = dot(d, ax);
      if (along > 0.02 && along < f.length + 0.01 && std::fabs(cross(ax, d)) < 0.03 && b.v.y > -0.2) want = true;
    }
    if (want && attractFlip_[s] <= 0) attractFlip_[s] = 0.28;
    f.pressed = attractFlip_[s] > 0.08;
  }
  if (w.liveBalls() == 0) {
    w.balls.clear();
    w.addBall(table.ballStart);
  }
  for (auto &b : w.balls)
    if (b.alive && b.p.x > 0.47 && b.p.y > 0.95 && len(b.v) < 0.01) w.plunger.autoPower = 0.75 + 0.25 * std::fmod(time, 1.0);
  for (size_t i = 0; i < w.balls.size(); i++)
    if (w.balls[i].captured >= 0 && portalHold_ < 0) { portalHold_ = 1.0; portalBall_ = (int)i; }
}

void Game::update(double dt) {
  dt = std::min(dt, 0.05);
  time += dt;
  World &w = table.world;
  bool physics = mode == MODE_PLAY || mode == MODE_BALL_END || mode == MODE_ATTRACT || mode == MODE_GAME_OVER;
  if (mode == MODE_ATTRACT) attractAI(dt);
  // the gale
  if (pestActive(PE_GALE) && mode == MODE_PLAY) {
    galePhase_ += dt * 0.45;
    w.wind = V2(0.38 * std::sin(galePhase_), 0);
  }
  // the magnet is off for a moment after it lets a ball go
  if (magnetCool_ > 0) magnetCool_ -= dt;
  w.holes[table.magnetHole].enabled = magnetOn && magnetCool_ <= 0 && mode == MODE_PLAY;
  if (physics) {
    accum_ += dt;
    while (accum_ >= kStep) {
      accum_ -= kStep;
      if (nudgeSteps_ > 0) {                        // a nudge lasts about 12 ms
        w.nudge = nudgeDir_ * 15.0;
        nudgeSteps_--;
      }
      w.step();
      for (const Event &e : w.events) {
        if (mode == MODE_ATTRACT) {
          if (e.type == EV_HIT && e.id >= E_BUMPER1 && e.id <= E_BUMPER3) bumperFlash[e.id - E_BUMPER1] = 1;
          if (e.type == EV_HIT && e.id >= E_DROP1 && e.id <= E_DROP3) dropReset_ = 1.0;
          if (e.type == EV_SENSOR && e.id == E_SPINNER) spinnerOmega_ = std::max(spinnerOmega_, e.speed * 60.0);
          continue;
        }
        handle(e);
        if (mode != MODE_PLAY && mode != MODE_BALL_END) break;
      }
      if (harvest && mode == MODE_PLAY && run.fieldScore >= run.target) {
        fieldWon();
        break;
      }
    }
  } else {
    accum_ = 0;
  }
  // timers
  for (float &b : bumperFlash) b = std::max(0.f, b - (float)dt * 6);
  shake = std::max(0.0, shake - dt * 8);
  goldRain = std::max(0.0, goldRain - dt);
  tiltMeter_ = std::max(0.0, tiltMeter_ - dt * 0.45);
  kickFire = std::max(0.0, kickFire - dt);
  if (orbitClock_ > 0) orbitClock_ -= dt;
  if (skillTime_ > 0) skillTime_ -= dt;
  if (ballSave_ > 0 && !inShooterLane_) ballSave_ = std::max(0.0, ballSave_ - dt);
  if (kickRelight_ > 0 && (kickRelight_ -= dt) <= 0) {
    kickbackLit = true;
    message("KICKBACK", "RELIT", 1.5);
  }
  // the spinner: a plate on an axle, slowed by friction; every half turn scores
  if (spinnerOmega_ > 0) {
    double a = spinnerOmega_ * dt;
    spinnerAngle += a;
    spinnerHalf_ += a;
    spinnerOmega_ = std::max(0.0, spinnerOmega_ - (2.2 * spinnerOmega_ + 6) * dt);
    while (spinnerHalf_ >= kPi) {
      spinnerHalf_ -= kPi;
      if (mode == MODE_PLAY) {
        add(150, 20, T_SPINNER);
        sfx(SFX_SPINNER, 0.35f, 0.03);
        if (++spinCount_ % 25 == 0 && !kickbackLit && !pestActive(PE_WEEVIL)) {
          kickbackLit = true;
          message("SPINNER", "KICKBACK LIT", 1.8);
        }
      }
    }
  } else {
    // settle the plate flat
    double target = std::round(spinnerAngle / kPi) * kPi;
    spinnerAngle += (target - spinnerAngle) * std::min(1.0, dt * 6);
  }
  if (dropReset_ > 0) {
    dropReset_ -= dt;
    if (dropReset_ <= 0) { table.resetTargets(); sfx(SFX_DROP, 0.6f, 0.43); }
  }
  if (portalHold_ > 0) {                            // the portal holds the ball, then kicks it out
    portalHold_ -= dt;
    if (portalHold_ <= 0) {
      portalHold_ = -1;
      if (portalBall_ >= 0 && portalBall_ < (int)w.balls.size() && w.balls[portalBall_].captured >= 0) {
        double a = frand() * 0.5 - 0.25;
        w.eject(portalBall_, V2(std::sin(a - 0.35), std::cos(a - 0.35)) * 1.35);
        magnetCool_ = std::max(magnetCool_, 1.5);   // don't catch the ball the portal just kicked out
        sfx(SFX_PORTAL_OUT, 1, table.portal.x);
        seeds(table.portal, 8, 0xffd24a, 0.3);
      }
      portalBall_ = -1;
    }
  }
  if (magnetHold_ > 0) {                            // the magnet lets go: up into the portal, or (pest) down the middle
    magnetHold_ -= dt;
    if (magnetHold_ <= 0) {
      magnetHold_ = -1;
      if (magnetBall_ >= 0 && magnetBall_ < (int)w.balls.size() && w.balls[magnetBall_].captured >= 0) {
        bool friendly = harvest && run.has(ML_MAGNET_SEED);
        // friendly: just enough speed to arrive at the portal slowly (0.35 m/s), whatever the ball weighs
        double d = len(table.portal - table.magnet), ga = slopeAccel() * w.gravityScale;
        V2 up = norm(table.portal - table.magnet);
        double v0 = std::sqrt(0.35 * 0.35 + 2 * ga * d * -up.y);
        V2 v = friendly ? up * v0 : V2((frand() - 0.5) * 0.1, 0.35);
        w.eject(magnetBall_, v);
        magnetCool_ = 2.5;
      }
      magnetBall_ = -1;
    }
  }
  if (launchQueue_ > 0 && mode == MODE_PLAY) {       // multiball / ball save: plunge more balls
    launchQueue_ -= dt;
    if (launchQueue_ <= 0 && pendingBalls_ > 0) {
      bool laneFree = true;
      for (auto &b : w.balls)
        if (b.alive && b.p.x > 0.47 && b.p.y > 0.8) laneFree = false;
      if (laneFree) {
        w.addBall(table.ballStart);
        w.plunger.autoPower = 0.9;
        sfx(SFX_LAUNCH, 1, 0.49);
        pendingBalls_--;
      }
      launchQueue_ = pendingBalls_ > 0 ? 1.2 : -1;
    }
  }
  // ball search: a ball that has come to rest somewhere it shouldn't gets a small kick
  if (physics) {
    bool moving = false, any = false;
    for (auto &b : w.balls) {
      if (!b.alive || b.captured >= 0 || b.layer) continue;
      if (b.p.x > 0.47 && b.p.y > 0.9) continue;     // waiting on the plunger is fine
      any = true;
      if (len(b.v) > 0.02) moving = true;
    }
    stillTime_ = any && !moving ? stillTime_ + dt : 0;
    if (stillTime_ > 4) {
      for (auto &b : w.balls)
        if (b.alive && b.captured < 0 && b.layer == 0 && !(b.p.x > 0.47 && b.p.y > 0.9))
          b.v += V2((frand() - 0.5) * 0.6, -0.4);
      stillTime_ = 0;
      if (mode == MODE_PLAY) message("BALL SEARCH", "", 1.5);
    }
  }
  // particles and popups
  for (auto &p : particles) {
    p.life -= (float)dt;
    p.p += p.v * dt;
    p.v = p.v * (1 - dt * 2);
    p.vz -= 3.0 * dt;
    p.z = std::max(0.0, p.z + p.vz * dt);
    if (p.z == 0 && p.vz < 0) p.vz = -p.vz * 0.35;
    p.spin += (float)dt * 400;
  }
  particles.erase(std::remove_if(particles.begin(), particles.end(), [](const Particle &p) { return p.life <= 0; }),
                  particles.end());
  for (auto &p : popups) p.t += (float)dt;
  popups.erase(std::remove_if(popups.begin(), popups.end(), [](const Popup &p) { return p.t >= p.dur; }), popups.end());

  // the flow between balls, fields and games
  if (mode == MODE_BALL_END) {
    endTimer_ -= dt;
    if (endTimer_ <= 0) {
      if (harvest) {
        if (run.fieldScore >= run.target) { mode = MODE_PLAY; fieldWon(); }
        else if (extraBalls > 0 || run.ballsLeft > 0) {
          if (extraBalls > 0) extraBalls--;
          else { run.ballsLeft--; ball++; }
          mode = MODE_PLAY;
          resetBall();
          serveBall(false);
          message(run.ballsLeft == 0 && extraBalls == 0 ? "LAST BALL" : "BALL " + std::to_string(ball),
                  "NEED " + commas(run.target - run.fieldScore), 2.5);
        } else {
          runOver();
        }
      } else {
        if (extraBalls > 0) {
          extraBalls--;
          message("SHOOT AGAIN", "", 2);
        } else {
          ball++;
        }
        if (ball > ballsPerGame) {
          mode = MODE_GAME_OVER;
          lastScore = score;
          endTimer_ = 4;
          bool high = hiscores.size() < 5 || score > hiscores.back().score;
          if (high && score > 0) {
            hiscores.push_back({playerName, score});
            std::sort(hiscores.begin(), hiscores.end(), [](const HiScore &a, const HiScore &b) { return a.score > b.score; });
            if (hiscores.size() > 5) hiscores.resize(5);
            saveScores();
          }
          message("GAME OVER", high && score > 0 ? "NEW HIGH SCORE" : rankName(), 4);
          sfx(SFX_GAME_OVER);
        } else {
          mode = MODE_PLAY;
          resetBall();
          serveBall(false);
          message("BALL " + std::to_string(ball), "", 2);
        }
      }
    }
  } else if (mode == MODE_GAME_OVER) {
    endTimer_ -= dt;
    if (endTimer_ <= 0) toAttract();
  } else if (mode == MODE_FIELD_WON) {
    modeTimer -= dt;
    if (modeTimer <= 0) {
      if (run.won) {
        runOver();
      } else {
        mode = MODE_SHOP;
        run.rerollCost = 2;
        stockShop();
        message("SEED MARKET", std::to_string(run.seeds) + " SEEDS", 1e9);
      }
    }
  } else if (mode == MODE_RUN_OVER) {
    modeTimer -= dt;
    if (modeTimer <= 0) toAttract();
  }
  if (msgTimer_ > 0 && msgTimer_ < 1e8) {
    msgTimer_ -= dt;
    if (msgTimer_ <= 0 && mode == MODE_PLAY) {
      if (inShooterLane_ && plungerReady()) message("PULL THE PLUNGER", "SPACE OR DOWN", 1e8);
      else if (harvest) message("FIELD " + commas(run.fieldScore), "OF " + commas(run.target), 1e8);
      else message(std::string("RANK ") + rankName(), "", 1e8);
    }
  }
  if (mode == MODE_PLAY && msgTimer_ > 1e7) {
    if (!inShooterLane_ && msg[0] == "PULL THE PLUNGER") msgTimer_ = 0.01;
    if (harvest && msg[0].rfind("FIELD ", 0) == 0) { msg[0] = "FIELD " + commas(run.fieldScore); }
  }
  updateLamps();
  updateInfo();
}

// ---------------------------------------------------------------- high scores, in the user's data folder
static std::string scoresPath() {
  char *p = SDL_GetPrefPath("melon", "pinball");
  std::string s = p ? std::string(p) + "scores.txt" : "";
  SDL_free(p);
  return s;
}

void Game::loadScores() {
  hiscores.clear();
  std::ifstream f(scoresPath());
  std::string line;
  while (std::getline(f, line)) {
    std::istringstream is(line);
    HiScore h;
    if (is >> h.score && std::getline(is >> std::ws, h.name)) hiscores.push_back(h);
  }
  std::sort(hiscores.begin(), hiscores.end(), [](const HiScore &a, const HiScore &b) { return a.score > b.score; });
  if (hiscores.size() > 5) hiscores.resize(5);
}

void Game::saveScores() {
  std::string p = scoresPath();
  if (p.empty()) return;
  std::ofstream f(p);
  for (auto &h : hiscores) f << h.score << " " << h.name << "\n";
}

}  // namespace pb

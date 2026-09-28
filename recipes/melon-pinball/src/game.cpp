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

// Missions: spell M-E-L-O-N on the standups to light the portal, sink the portal to start the next mission.
const char *kMissionNames[5] = {"COMPILE KERNEL", "APK UPGRADE", "SEED HARVEST", "RIND RUNNER", "THE GAUNTLET"};
const char *kMissionGoals[5] = {"RAMP 3 TIMES", "SPELL A-P-K TWICE", "CLEAR SEED BANK 2X", "LOOP 3 TIMES",
                                "PORTAL 3 TIMES"};
static const int kMissionNeed[5] = {3, 2, 2, 3, 3};
static const char *kRanks[] = {"SEEDLING", "SPROUT", "VINE", "BLOSSOM", "RIPE MELON", "GOLDEN MELON"};

static std::string upper(std::string s) {
  for (auto &c : s) c = (char)std::toupper((unsigned char)c);
  return s;
}

void Game::init(bool gold) {
  golden = gold;
  table.build();
  const char *u = std::getenv("USER");
  if (u && *u) playerName = upper(u).substr(0, 10);
  loadScores();
  mode = MODE_ATTRACT;
  table.world.balls.clear();
  table.world.addBall(table.ballStart);
  message("PRESS F2 OR START", "TO PLAY", 1e9);
  updateInfo();
}

void Game::newGame() {
  table.build();
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
  sfx(SFX_START);
  message("WELCOME TO", "MELON PLANET", 3);
}

void Game::resetBall() {
  for (bool &a : apk_) a = false;
  for (bool &m : melon_) m = false;
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
  missionProgress_ = mission_ >= 0 ? missionProgress_ : 0;
  ballSave_ = 0;
  ballSaveArmed_ = true;
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

void Game::add(long long pts, long long bonusPts) {
  if (tilted || mode != MODE_PLAY) return;
  score += pts;
  bonus += bonusPts;
}

void Game::message(const std::string &a, const std::string &b, double secs) {
  msg[0] = a;
  msg[1] = b;
  msgTimer_ = secs;
}

void Game::popup(const std::string &s, unsigned color, float dur) {
  if (mode != MODE_PLAY) return;
  popups.push_back({s, 0, dur, color});
  if (popups.size() > 3) popups.erase(popups.begin());
}

void Game::seeds(V2 at, int n, unsigned color, double speed) {
  for (int i = 0; i < n; i++) {
    double a = (std::rand() % 1000) / 1000.0 * 2 * kPi, s = speed * (0.4 + (std::rand() % 1000) / 1000.0 * 0.8);
    Particle p;
    p.p = at;
    p.v = V2(std::cos(a), std::sin(a)) * s;
    p.z = 0.02;
    p.vz = 0.3 + (std::rand() % 1000) / 1000.0 * 0.5;
    p.maxLife = p.life = 0.7f + (std::rand() % 100) / 200.f;
    p.color = color;
    p.kind = (i % 3 == 0) ? 1 : 0;
    p.spin = (float)(std::rand() % 360);
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
  Flipper &f = table.world.flippers[side];
  bool want = down && !tilted && mode == MODE_PLAY;
  bool pressed = want && !f.pressed;
  if (want != f.pressed) sfx(want ? SFX_FLIP_UP : SFX_FLIP_DOWN, want ? 1.f : 0.5f, f.pivot.x);
  f.pressed = want;
  // lane change: each flip moves the lit A-P-K lanes along
  if (pressed) {
    bool t[3];
    for (int i = 0; i < 3; i++) t[i] = apk_[i];
    for (int i = 0; i < 3; i++) apk_[i] = side == 0 ? t[(i + 1) % 3] : t[(i + 2) % 3];
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
  tiltMeter_ += 1.0;
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
  mission_ = -1;
  long long award = 250000LL * missionCount_;
  add(award, 25000);
  char b[32];
  std::snprintf(b, sizeof b, "%lld", award);
  message(std::string(kMissionNames[m]) + " DONE", std::string("RANK ") + rankName(), 4);
  popup("MISSION COMPLETE", 0xffd24a, 2.4f);
  sfx(SFX_MISSION);
  if (m == 4) {                                     // through the gauntlet: the other side
    goldRain = 6;
    popup("THE OTHER SIDE", 0xffd24a, 3.0f);
  }
  if (missionCount_ == 2 || missionCount_ == 4) {
    extraLit_ = true;
    message("EXTRA BALL", "IS LIT AT THE PORTAL", 3);
  }
  // pick the next mission not done yet
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
  pendingBalls_ = 2;
  launchQueue_ = 0.3;
  ballSave_ = 15;
  message("SEED STORM", "JACKPOT AT THE PORTAL", 5);
  popup("SEED STORM", 0xf0a35e, 2.5f);
  sfx(SFX_MULTIBALL);
}

void Game::ballDrained(int bi) {
  (void)bi;
  if (mode != MODE_PLAY) return;
  sfx(SFX_DRAIN, 0.8f);
  int live = table.world.liveBalls();
  if (!tilted && ballSave_ > 0 && !inShooterLane_) {
    // ball save: another one out of the shooter lane, automatically
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
  char l1[32], l2[32];
  std::snprintf(l1, sizeof l1, "BONUS %lld X %d", bonus, bonusMult);
  std::snprintf(l2, sizeof l2, "= %lld", b);
  message(tilted ? "TILT" : l1, tilted ? "NO BONUS" : l2, 2.6);
  score += b;
  sfx(SFX_BONUS);
  for (auto &f : table.world.flippers) f.pressed = false;
}

void Game::handle(const Event &e) {
  World &w = table.world;
  if (e.type == EV_DRAIN) { ballDrained(e.ball); return; }
  if (e.type == EV_LAUNCH) return;
  if (e.type == EV_RAMP_FAIL) { sfx(SFX_WALL, 0.4f); return; }
  double x = e.ball >= 0 ? w.balls[e.ball].p.x : 0.254;
  if (e.type == EV_HOLE && e.id == E_PORTAL) {
    portalHold_ = 1.2;
    portalBall_ = e.ball;
    sfx(SFX_PORTAL_IN, 1, x);
    seeds(table.portal, 18, 0xffd24a, 0.35);
    add(25000, 5000);
    if (jackpotLit_) {
      add(1000000);
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
          if (ballSaveArmed_) { ballSave_ = 10; ballSaveArmed_ = false; }
          skillTime_ = 5;
        }
        break;
      case E_LANE_A: case E_LANE_P: case E_LANE_K: {
        int i = e.id - E_LANE_A;
        sfx(SFX_ROLLOVER, 0.8f, x);
        if (skillTime_ > 0 && i == skillLane_) {
          add(75000, 5000);
          popup("SKILL SHOT", 0xffd24a);
          message("SKILL SHOT", "75,000", 2.5);
          skillTime_ = 0;
        }
        skillTime_ = 0;
        add(1000, 1000);
        apk_[i] = true;
        if (apk_[0] && apk_[1] && apk_[2]) {
          for (bool &a : apk_) a = false;
          bonusMult = std::min(5, bonusMult + 1);
          add(20000);
          char b[32];
          std::snprintf(b, sizeof b, "BONUS %dX", bonusMult);
          message("APK UPGRADE", b, 2.5);
          popup("APK UPGRADE", 0xf0a35e);
          progressMission(1);
        }
        break;
      }
      case E_INLANE_L: case E_INLANE_R:
        sfx(SFX_ROLLOVER, 0.6f, x);
        add(1000, 500);
        break;
      case E_OUTLANE_L: case E_OUTLANE_R:
        sfx(SFX_ROLLOVER, 0.6f, x);
        add(5000, 500);
        break;
      case E_ORBIT_ENTER:
        if (e.dir > 0) orbitClock_ = 3.0;
        break;
      case E_ORBIT_TOP:
        if (orbitClock_ > 0) {
          orbitClock_ = -1;
          add(10000, 2000);
          sfx(SFX_LOOP, 1, x);
          message("RIND LOOP", "10,000", 1.5);
          progressMission(3);
        }
        break;
      case E_RAMP_ENTER:
        sfx(SFX_RAMP, 0.8f, x);
        break;
      case E_RAMP_EXIT:
        add(15000, 3000);
        sfx(SFX_RAMP, 1, x);
        message("KERNEL RAMP", "15,000", 1.5);
        seeds(w.ramps[0].pts.back(), 10, 0xf0a35e, 0.3);
        progressMission(0);
        break;
    }
    return;
  }
  if (e.type != EV_HIT || tilted) return;
  lastHit_ = time;
  float vol = (float)std::clamp(e.speed / 2.0, 0.15, 1.0);
  switch (e.id) {
    case E_BUMPER1: case E_BUMPER2: case E_BUMPER3: {
      int i = e.id - E_BUMPER1;
      bumperFlash[i] = 1;
      add(1000, 100);
      bumperHits_++;
      sfx(SFX_BUMPER, 1, x);
      seeds(w.circles[table.bumperCircle[i]].c, 7, 0xece2b4, 0.35);
      break;
    }
    case E_SLING_L: case E_SLING_R:
      add(100, 10);
      sfx(SFX_SLING, 1, x);
      break;
    case E_TARGET_M: case E_TARGET_E: case E_TARGET_L: case E_TARGET_O: case E_TARGET_N: {
      int i = e.id - E_TARGET_M;
      sfx(SFX_TARGET, vol, x);
      add(melon_[i] ? 1000 : 5000, 1000);
      melon_[i] = true;
      bool all = true;
      for (bool m : melon_) all = all && m;
      if (all && !missionReady_ && mission_ < 0) {
        missionReady_ = true;
        add(50000);
        message("M-E-L-O-N", std::string("PORTAL STARTS ") + kMissionNames[missionNext_], 3.5);
        popup("MELON", 0x6fbf4a);
      } else if (all) {
        for (bool &m : melon_) m = false;
        add(25000);
      }
      break;
    }
    case E_DROP1: case E_DROP2: case E_DROP3: {
      sfx(SFX_DROP, 1, x);
      add(5000, 1000);
      bool all = true;
      for (int k = 0; k < 3; k++) all = all && !w.segs[table.dropSeg[k]].active;
      if (all) {
        dropsBanked_++;
        add(25000, 5000);
        message("SEED BANK", "25,000", 2);
        seeds(V2(0.425, 0.435), 20, 0xf0a35e, 0.4);
        dropReset_ = 1.2;
        progressMission(2);
      }
      break;
    }
    case E_RUBBER:
      add(10);
      sfx(SFX_RUBBER, vol, x);
      break;
    default:
      if (e.speed > 0.4) sfx(SFX_WALL, vol * 0.6f, x);
  }
}

void Game::updateLamps() {
  auto blink = [&](double hz) { return std::fmod(time * hz, 1.0) < 0.5 ? 1.f : 0.f; };
  auto set = [&](int i, float v) { lamp[i] += (v - lamp[i]) * 0.35f; };
  if (mode == MODE_ATTRACT) {                       // attract: chase the lamps
    for (int i = 0; i < LA_COUNT; i++) set(i, std::fmod(time * 1.3 + i * 0.137, 1.0) < 0.3 ? 1.f : 0.f);
    return;
  }
  if (tilted) {
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
  set(LA_PORTAL, jackpotLit_ ? blink(6) : (missionReady_ || extraLit_ || mission_ == 4 ? blink(2) : 0.2f));
  set(LA_EXTRA_BALL, extraLit_ ? blink(3) : 0.f);
  set(LA_JACKPOT, jackpotLit_ ? blink(4) : 0.f);
  set(LA_ORBIT_ARROW, mission_ == 3 ? blink(3) : 0.f);
  set(LA_RAMP_ARROW, mission_ == 0 ? blink(3) : 0.f);
  set(LA_DROP_ARROW, mission_ == 2 ? blink(3) : 0.f);
  set(LA_PORTAL_ARROW, missionReady_ || jackpotLit_ || extraLit_ || mission_ == 4 ? blink(3) : 0.f);
  for (int i = 0; i < 4; i++) set(LA_MULT2 + i, bonusMult >= i + 2 ? 1.f : 0.f);
  set(LA_SHOOT_AGAIN, ballSave_ > 0 ? (ballSave_ < 3 ? blink(6) : 1.f) : (extraBalls > 0 ? 1.f : 0.f));
  set(LA_INLANE_L, 0.f);
  set(LA_INLANE_R, 0.f);
  set(LA_OUTLANE_L, 0.f);
  set(LA_OUTLANE_R, 0.f);
  set(LA_SKILL, inShooterLane_ ? blink(2) : 0.f);
}

void Game::updateInfo() {
  if (mode == MODE_ATTRACT) {
    info[0] = "HIGH SCORE";
    info[1] = hiscores.empty() ? "0" : hiscores[0].name.substr(0, 8) + " " + std::to_string(hiscores[0].score);
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
  if (std::fmod(time, 8.0) > 5.5) {
    info[0] = "RANK";
    info[1] = rankName();
  }
}

void Game::attractAI(double dt) {
  // the demo plays itself: flip when a ball comes down onto a flipper
  World &w = table.world;
  for (int s = 0; s < 2; s++) {
    Flipper &f = w.flippers[s];
    attractFlip_[s] -= dt;
    bool want = false;
    for (auto &b : w.balls) {
      if (!b.alive || b.layer) continue;
      V2 d = b.p - f.pivot;
      double along = dot(d, V2(std::cos(f.rest), std::sin(f.rest)));
      if (along > 0.02 && along < 0.085 && std::fabs(cross(V2(std::cos(f.rest), std::sin(f.rest)), d)) < 0.03 && b.v.y > -0.2)
        want = true;
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
  if (mode == MODE_ATTRACT) attractAI(dt);
  accum_ += dt;
  World &w = table.world;
  while (accum_ >= kStep) {
    accum_ -= kStep;
    if (nudgeSteps_ > 0) {                          // a nudge lasts about 12 ms
      w.nudge = nudgeDir_ * 15.0;
      nudgeSteps_--;
    }
    w.step();
    for (const Event &e : w.events) {
      if (mode == MODE_ATTRACT) {
        if (e.type == EV_HIT && e.id >= E_BUMPER1 && e.id <= E_BUMPER3) bumperFlash[e.id - E_BUMPER1] = 1;
        if (e.type == EV_HIT && e.id >= E_DROP1 && e.id <= E_DROP3) dropReset_ = 1.0;
        continue;
      }
      handle(e);
    }
  }
  // timers
  for (float &b : bumperFlash) b = std::max(0.f, b - (float)dt * 6);
  shake = std::max(0.0, shake - dt * 8);
  goldRain = std::max(0.0, goldRain - dt);
  tiltMeter_ = std::max(0.0, tiltMeter_ - dt * 0.45);
  if (orbitClock_ > 0) orbitClock_ -= dt;
  if (skillTime_ > 0) skillTime_ -= dt;
  if (ballSave_ > 0 && !inShooterLane_) ballSave_ = std::max(0.0, ballSave_ - dt);
  if (dropReset_ > 0) {
    dropReset_ -= dt;
    if (dropReset_ <= 0) { table.resetTargets(); sfx(SFX_DROP, 0.6f, 0.43); }
  }
  if (portalHold_ > 0) {                            // the portal holds the ball, then kicks it out
    portalHold_ -= dt;
    if (portalHold_ <= 0) {
      portalHold_ = -1;
      if (portalBall_ >= 0 && portalBall_ < (int)w.balls.size() && w.balls[portalBall_].captured >= 0) {
        double a = (std::rand() % 100) / 100.0 * 0.5 - 0.25;
        w.eject(portalBall_, V2(std::sin(a - 0.35), std::cos(a - 0.35)) * 1.35);
        sfx(SFX_PORTAL_OUT, 1, table.portal.x);
        seeds(table.portal, 8, 0xffd24a, 0.3);
      }
      portalBall_ = -1;
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
  // ball search: a real machine fires its coils when no switch has closed for a while; here a ball
  // that has come to rest somewhere it shouldn't gets a small kick
  {
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
          b.v += V2(((std::rand() % 100) / 100.0 - 0.5) * 0.6, -0.4);
      stillTime_ = 0;
      if (mode == MODE_PLAY) message("BALL SEARCH", "", 1.5);
    }
  }
  // particles
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

  if (mode == MODE_BALL_END) {
    endTimer_ -= dt;
    if (endTimer_ <= 0) {
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
        char b[16];
        std::snprintf(b, sizeof b, "BALL %d", ball);
        message(b, "", 2);
      }
    }
  } else if (mode == MODE_GAME_OVER) {
    endTimer_ -= dt;
    if (endTimer_ <= 0) {
      mode = MODE_ATTRACT;
      table.build();
      w.addBall(table.ballStart);
      message("PRESS F2 OR START", "TO PLAY", 1e9);
    }
  }
  if (msgTimer_ > 0 && msgTimer_ < 1e8) {
    msgTimer_ -= dt;
    if (msgTimer_ <= 0 && mode == MODE_PLAY) {
      if (inShooterLane_ && plungerReady()) message("PULL THE PLUNGER", "SPACE OR DOWN", 1e8);
      else message(std::string("RANK ") + rankName(), "", 1e8);
    }
  }
  if (mode == MODE_PLAY && !inShooterLane_ && msgTimer_ > 1e7 && msg[0] == "PULL THE PLUNGER")
    message(std::string("RANK ") + rankName(), "", 1e8);
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

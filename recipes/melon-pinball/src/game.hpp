// The rules of the Melon Planet table: scoring, lamps, missions, ranks, multiball, ball save, tilt.
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "table.hpp"

namespace pb {

enum Sfx {
  SFX_FLIP_UP, SFX_FLIP_DOWN, SFX_BUMPER, SFX_SLING, SFX_RUBBER, SFX_WALL, SFX_TARGET, SFX_DROP, SFX_ROLLOVER,
  SFX_LAUNCH, SFX_PULL, SFX_PORTAL_IN, SFX_PORTAL_OUT, SFX_RAMP, SFX_DRAIN, SFX_MISSION, SFX_EXTRA_BALL, SFX_TILT,
  SFX_JACKPOT, SFX_SAVE, SFX_LOOP, SFX_BONUS, SFX_GAME_OVER, SFX_START, SFX_MULTIBALL, SFX_NUDGE, SFX_WARNING,
  SFX_COUNT
};

struct Particle {
  V2 p, v;                  // table position and velocity (m, m/s)
  double z, vz;             // height
  float life, maxLife;
  unsigned color;
  int kind;                 // 0 seed, 1 spark, 2 gold flake (screen rain)
  float spin;
};

struct Popup {              // big text over the table
  std::string text;
  float t, dur;
  unsigned color;
};

enum Mode { MODE_ATTRACT, MODE_PLAY, MODE_BALL_END, MODE_GAME_OVER };

struct HiScore {
  std::string name;
  long long score;
};

class Game {
public:
  Table table;
  bool golden = false;                 // gauntlet survivor
  std::function<void(Sfx, float vol, float pan)> sound;

  // what the renderer shows
  float lamp[LA_COUNT] = {};
  float bumperFlash[3] = {};
  std::vector<Particle> particles;
  std::vector<Popup> popups;
  std::string msg[2], info[2];         // message boxes (two lines each)
  double shake = 0;                    // screen shake (nudges), decays
  V2 shakeDir;
  double goldRain = 0;                 // seconds of gold rain left (the gauntlet)
  double time = 0;

  Mode mode = MODE_ATTRACT;
  long long score = 0, bonus = 0, lastScore = 0;
  int ball = 1, ballsPerGame = 3, extraBalls = 0;
  int bonusMult = 1;
  bool tilted = false;
  std::vector<HiScore> hiscores;
  std::string playerName = "PLAYER";

  void init(bool golden);
  void newGame();
  void update(double dt);              // real time; runs the physics at 2 kHz
  void flipper(int side, bool down);
  void plunger(bool down);
  void nudge(V2 dir);                  // dir in table space, roughly unit
  int rankIndex() const;
  const char *rankName() const;
  double ballSaveLeft() const { return ballSave_; }
  int missionsDone() const { return missionsDone_; }
  int activeMission() const { return mission_; }
  bool plungerReady() const;

private:
  // per ball / per game state
  bool apk_[3] = {};
  bool melon_[5] = {};
  int dropsBanked_ = 0;
  int mission_ = -1;                   // active mission, -1 none
  int missionNext_ = 0;                // the one M-E-L-O-N will light
  bool missionReady_ = false;          // M-E-L-O-N spelled: the portal starts it
  int missionProgress_ = 0;
  int missionsDone_ = 0;               // bitmask
  int missionCount_ = 0;
  bool extraLit_ = false;
  bool multiball_ = false;
  bool jackpotLit_ = false;
  int skillLane_ = -1;
  double skillTime_ = 0;
  double ballSave_ = 0;
  bool ballSaveArmed_ = false;
  double tiltMeter_ = 0;
  int tiltWarnings_ = 0;
  double orbitClock_ = -1;
  double portalHold_ = -1;
  int portalBall_ = -1;
  double dropReset_ = -1;
  double endTimer_ = 0;
  double msgTimer_ = 0;
  double attractFlip_[2] = {0, 0};
  double accum_ = 0;
  int nudgeSteps_ = 0;
  V2 nudgeDir_;
  double lastHit_ = 0;
  bool inShooterLane_ = true;
  int bumperHits_ = 0;
  double launchQueue_ = -1;            // multiball: more balls to plunge automatically
  int pendingBalls_ = 0;
  double stillTime_ = 0;               // ball search: how long no ball has moved

  void resetBall();
  void serveBall(bool autoLaunch);
  void handle(const Event &e);
  void add(long long pts, long long bonusPts = 0);
  void message(const std::string &a, const std::string &b, double secs = 3);
  void popup(const std::string &s, unsigned color, float dur = 1.6f);
  void startMission(int m);
  void progressMission(int m, int amount = 1);
  void completeMission();
  void startMultiball();
  void ballDrained(int bi);
  void endOfBall();
  void updateLamps();
  void updateInfo();
  void seeds(V2 at, int n, unsigned color, double speed);
  void sfx(Sfx s, float vol = 1, double x = 0.254);
  void attractAI(double dt);
  void loadScores();
  void saveScores();
};

extern const char *kMissionNames[5];
extern const char *kMissionGoals[5];

}  // namespace pb

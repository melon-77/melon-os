// The rules of the Melon Planet table: scoring, lamps, missions, ranks, multiball, ball save, tilt,
// and the Harvest run (fields, pests, seeds, melons, the Seed Market).
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "run.hpp"
#include "table.hpp"

namespace pb {

enum Sfx {
  SFX_FLIP_UP, SFX_FLIP_DOWN, SFX_BUMPER, SFX_SLING, SFX_RUBBER, SFX_WALL, SFX_TARGET, SFX_DROP, SFX_ROLLOVER,
  SFX_LAUNCH, SFX_PULL, SFX_PORTAL_IN, SFX_PORTAL_OUT, SFX_RAMP, SFX_DRAIN, SFX_MISSION, SFX_EXTRA_BALL, SFX_TILT,
  SFX_JACKPOT, SFX_SAVE, SFX_LOOP, SFX_BONUS, SFX_GAME_OVER, SFX_START, SFX_MULTIBALL, SFX_NUDGE, SFX_WARNING,
  SFX_SPINNER, SFX_KICKBACK, SFX_MAGNET, SFX_COIN, SFX_BUY, SFX_SHATTER, SFX_UNLOCK,
  SFX_COUNT
};

// what a score came from (melons and pests act on these)
enum ScoreTag {
  T_OTHER, T_BUMPER, T_SLING, T_RUBBER, T_LANE, T_APK, T_INLANE, T_OUTLANE, T_LOOP, T_RAMP, T_LETTER, T_MELON, T_DROP,
  T_BANK, T_PORTAL, T_JACKPOT, T_MISSION, T_SKILL, T_SPINNER, T_BONUS, T_TAG_COUNT
};

struct Particle {
  V2 p, v;                  // table position and velocity (m, m/s)
  double z, vz;             // height
  float life, maxLife;
  unsigned color;
  int kind;                 // 0 seed, 1 spark
  float spin;
};

struct Popup {              // big text over the table
  std::string text;
  float t, dur;
  unsigned color;
};

enum Mode {
  MODE_ATTRACT, MODE_PLAY, MODE_BALL_END, MODE_GAME_OVER,
  MODE_PACK_SELECT, MODE_FIELD_WON, MODE_SHOP, MODE_RUN_OVER
};

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
  double shake = 0;
  V2 shakeDir;
  double goldRain = 0;
  double time = 0;
  double spinnerAngle = 0;             // the spinner's plate (radians)
  bool kickbackLit = false;
  double kickFire = 0;                 // kickback plunger animation
  bool magnetOn = false;

  Mode mode = MODE_ATTRACT;
  long long score = 0, bonus = 0, lastScore = 0;
  int ball = 1, ballsPerGame = 3, extraBalls = 0;
  int bonusMult = 1;
  bool tilted = false;
  std::vector<HiScore> hiscores;
  std::string playerName = "PLAYER";

  // Harvest
  bool harvest = false;
  Run run;
  Unlocks unlocks;
  std::vector<std::pair<std::string, int>> payout;   // seeds earned on the last field, line by line
  double modeTimer = 0;

  void init(bool golden);
  void newGame();                      // classic: three balls, high score
  void newRun();                       // Harvest: pick a seed pack (or start at once when only one is unlocked)
  void startRun(int pack);
  void toAttract();                    // back to the demo (leaving a pack choice or a finished run)
  void update(double dt);              // real time; runs the physics at 2 kHz
  void flipper(int side, bool down);
  void plunger(bool down);
  void nudge(V2 dir);
  int rankIndex() const;
  const char *rankName() const;
  bool plungerReady() const;
  int missionsDone() const { return missionsDone_; }
  int activeMission() const { return mission_; }
  double currentMult() const;          // the melons' multiplier right now (display)
  bool pestActive(int p) const;

  // the Seed Market
  bool buyMelon(int slot);
  bool buyGraft();
  bool sellMelon(int idx);
  bool reroll();
  void nextField();
  bool seasonDone() const { return run.won; }

private:
  bool apk_[3] = {};
  bool melon_[5] = {};
  int dropsBanked_ = 0;
  int mission_ = -1;
  int missionNext_ = 0;
  bool missionReady_ = false;
  int missionProgress_ = 0;
  int missionsDone_ = 0;
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
  double magnetHold_ = -1;
  int magnetBall_ = -1;
  double magnetCool_ = 0;
  double dropReset_ = -1;
  double endTimer_ = 0;
  double msgTimer_ = 0;
  double attractFlip_[3] = {0, 0, 0};
  double accum_ = 0;
  int nudgeSteps_ = 0;
  V2 nudgeDir_;
  bool inShooterLane_ = true;
  int bumperHits_ = 0;
  double launchQueue_ = -1;
  int pendingBalls_ = 0;
  double stillTime_ = 0;
  double spinnerOmega_ = 0;
  double spinnerHalf_ = 0;             // accumulated half turns (each scores)
  int spinCount_ = 0;
  double kickRelight_ = -1;
  bool insureNext_ = false;
  bool tagsHit_[T_TAG_COUNT] = {};
  double galePhase_ = 0;

  void resetBall();
  void serveBall(bool autoLaunch);
  void handle(const Event &e);
  void add(long long pts, long long bonusPts = 0, ScoreTag tag = T_OTHER);
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
  void lightLetter(int n);
  void spelled();
  // Harvest
  void startField();
  void applyMachine();                 // melons and pests change the machine's physics
  void fieldWon();
  void runOver();
  void stockShop();
  void unlock(int melonId);
  void unlockPack(int pack);
  void giveSeeds(int n, const char *why);
};

extern const char *kMissionNames[5];
extern const char *kMissionGoals[5];

}  // namespace pb

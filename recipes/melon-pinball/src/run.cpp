#include "run.hpp"

#include <SDL3/SDL.h>

#include <cmath>
#include <fstream>
#include <sstream>

namespace pb {

// name, what it does, rarity, price, rind/flesh colours, icon style, how to unlock (nullptr: from the start)
const MelonDef kMelons[ML_COUNT] = {
    {"Cantaloupe", "Pop bumpers score +2,000.", R_COMMON, 3, 0x4f8a3c, 0xf0a35e, 0, nullptr},
    {"Slice Shot", "Slingshots score +3,000.", R_COMMON, 3, 0x3a682e, 0xff8a4a, 1, nullptr},
    {"Whirligig", "Spinner spins score x5.", R_COMMON, 3, 0x6fbf4a, 0xfff0a0, 2, nullptr},
    {"Rind Runner", "Rind loops score x3.", R_COMMON, 4, 0x2c5a2c, 0x9be07a, 0, nullptr},
    {"Seedless", "+3 seeds after every field.", R_COMMON, 4, 0x88b85a, 0xffd6a8, 1, nullptr},
    {"Honey Trap", "Ball save lasts 12 seconds longer.", R_COMMON, 4, 0xc8d870, 0xeaf6c0, 2, nullptr},
    {"Steady Hands", "Nudging tilts half as fast.", R_COMMON, 3, 0x5a7a4a, 0xd8e8c0, 2, nullptr},
    {"APK Cache", "A-P-K lanes raise the bonus by 2x more and light 2 letters of M-E-L-O-N.", R_COMMON, 4, 0x4f8a3c,
     0xf0a35e, 1, nullptr},
    {"Overclock", "Flippers hit 15% harder.", R_UNCOMMON, 5, 0x3a5aa0, 0xa0c8ff, 0, nullptr},
    {"Kernel Panic", "Kernel ramp shots score x4.", R_UNCOMMON, 5, 0x6a2a2a, 0xff6a4a, 1, nullptr},
    {"Seed Money", "+1 seed every time the seed bank is cleared.", R_UNCOMMON, 5, 0x8a6a2a, 0xf0c060, 0, nullptr},
    {"Echo Bumper", "Pop bumpers throw 30% harder and score x1.5.", R_UNCOMMON, 6, 0x2a6a5a, 0x7ae0c8, 0, nullptr},
    {"Featherweight", "The ball is 20% lighter: slower and floatier.", R_UNCOMMON, 5, 0xd0e8d0, 0xffffff, 2, nullptr},
    {"Bitter Melon", "x2 every score, but the flippers are 12% weaker.", R_UNCOMMON, 6, 0x2a4a1a, 0xc8e060, 3,
     "Clear season 2"},
    {"Kickback Vine", "The kickback relights itself 15 seconds after it fires.", R_UNCOMMON, 5, 0x3a682e, 0xf0a35e, 3,
     nullptr},
    {"Compost Heap", "Interest pays up to 10 seeds instead of 5.", R_UNCOMMON, 6, 0x5a4a2a, 0xa08a5a, 0,
     "Hold 20 seeds at once"},
    {"Wild Growth", "+0.1x mult for each kind of target hit this ball.", R_UNCOMMON, 6, 0x4a9a3a, 0xe0ff90, 3,
     "Finish 3 missions in one run"},
    {"Crop Insurance", "The first ball lost down an outlane on each field comes back.", R_UNCOMMON, 5, 0x5a6a8a, 0xc0d0f0,
     2, nullptr},
    {"Magnet Seed", "A magnet catches passing balls and feeds them to the portal.", R_RARE, 7, 0x3a4a9a, 0x9ab8ff, 0,
     nullptr},
    {"Twin Seeds", "Every portal sink launches another ball.", R_RARE, 8, 0x4f8a3c, 0xf0a35e, 1, "Start a multiball"},
    {"Golden Rind", "x3 every score while two or more balls are in play.", R_RARE, 8, 0xc9a23c, 0xffe28a, 5,
     "Finish The Gauntlet mission"},
    {"Glass Melon", "x2.5 every score. 1 in 6 chance to shatter after each field.", R_RARE, 7, 0xa8d8c8, 0xe8fff8, 4,
     "Reach season 4"},
    {"Melon Baller", "Gains +0.5x mult every time you spell M-E-L-O-N.", R_RARE, 8, 0x6fbf4a, 0xf0a35e, 1,
     "Spell M-E-L-O-N 5 times in one run"},
    {"Harvest Moon", "Pests have no effect.", R_RARE, 8, 0x3a3a5a, 0xe8e0b0, 2, "Beat a pest field"},
    {"Tenth Seed", "Every 10th pop bumper hit scores x20.", R_RARE, 7, 0x6a5a2a, 0xffe0a0, 0,
     "Hit the pop bumpers 100 times on one field"},
    {"Portal Key", "Portal sinks score x5 and give a seed.", R_RARE, 8, 0x8a6a1e, 0xffd24a, 5,
     "Sink the portal 10 times in one run"},
    {"Last Seed", "x3 every score on the last ball of a field.", R_RARE, 7, 0x4a2a2a, 0xff9a7a, 1,
     "Win a field on its last ball"},
    {"Survivor's Rind", "x1.5 every score, and +0.5x more for each pest field beaten.", R_RARE, 9, 0xc9a23c,
     0xffd24a, 5, "Survive the installer's gauntlet"},
};

const GraftDef kGrafts[GR_COUNT] = {
    {"Fertilizer", "Next field's target is 25% lower.", 3, 0x8a6a2a},
    {"Extra Vine", "One more ball on the next field.", 4, 0x6fbf4a},
    {"Pollinator", "The next field starts with M-E-L-O-N spelled.", 3, 0xffd24a},
    {"Irrigation", "30 seconds of ball save on the next field.", 3, 0x7ab0e0},
    {"Sunlamp", "The next field starts with the bonus at 4x.", 2, 0xf0a35e},
    {"Trellis", "One more melon slot for the rest of the run.", 9, 0x9be07a},
};

const PestDef kPests[PE_COUNT] = {
    {"The Frost", "The flippers are 25% weaker.", "WEAK FLIPPERS"},
    {"The Drought", "Pop bumpers score nothing.", "BUMPERS SCORE 0"},
    {"The Gale", "A wind blows across the table.", "WIND"},
    {"The Lock", "The portal is sealed.", "PORTAL SEALED"},
    {"The Mole", "Drop targets pop straight back up.", "TARGETS POP UP"},
    {"The Heat", "The ball is 30% heavier.", "HEAVY BALL"},
    {"The Blight", "The slingshots are dead.", "DEAD SLINGSHOTS"},
    {"The Weevil", "No ball save, no kickback.", "NO SAVES"},
    {"The Fog", "The lamps are dark.", "LAMPS OUT"},
    {"The Magnet", "A magnet grabs the ball and drops it down the middle.", "MAGNET DRAIN"},
};

const PackDef kPacks[PK_COUNT] = {
    {"Cantaloupe Pack", "The standard harvest. 4 seeds to start.", nullptr},
    {"Honeydew Pack", "Start with 12 seeds.", "Clear season 2"},
    {"Watermelon Pack", "3 balls on every field, but the targets are 50% higher.", "Clear season 4"},
    {"Bitter Pack", "Start with a Bitter Melon. Every vine field has a pest too.", "Clear season 6"},
    {"Wild Pack", "Start with 2 random melons. Everything in the market costs 1 more.", "Win a harvest"},
    {"Gauntlet Pack", "Start with Survivor's Rind.", "Survive the installer's gauntlet"},
};

const char *fieldName(int field) {
  static const char *n[] = {"SPROUT FIELD", "VINE FIELD", "PEST FIELD"};
  return n[field % 3];
}

long long fieldTarget(int season, int field, int pack) {
  // doubles every season; the vine field wants 1.5x, the pest field 2x
  double base = 20000.0 * std::pow(2.0, season - 1);
  double f = field == 0 ? 1.0 : (field == 1 ? 1.5 : 2.0);
  if (pack == PK_WATERMELON) f *= 1.5;
  long long t = (long long)(base * f);
  long long step = t >= 1000000 ? 10000 : 1000;      // round numbers on the display
  return (t + step - 1) / step * step;
}

bool Run::has(int m) const {
  for (auto &o : melons)
    if (o.id == m) return true;
  return false;
}

int Run::sellValue(int idx) const {
  int p = price(melons[idx].id);
  return p / 2 > 0 ? p / 2 : 1;
}

static std::string unlockPath() {
  char *p = SDL_GetPrefPath("melon", "pinball");
  std::string s = p ? std::string(p) + "unlocks.txt" : "";
  SDL_free(p);
  return s;
}

void Unlocks::load() {
  std::ifstream f(unlockPath());
  std::string kind;
  int v;
  while (f >> kind >> v) {
    if (kind == "melon" && v >= 0 && v < ML_COUNT) melon[v] = true;
    if (kind == "pack" && v >= 0 && v < PK_COUNT) pack[v] = true;
    if (kind == "best") bestSeason = v;
  }
}

void Unlocks::save() const {
  std::string p = unlockPath();
  if (p.empty()) return;
  std::ofstream f(p);
  for (int i = 0; i < ML_COUNT; i++)
    if (melon[i]) f << "melon " << i << "\n";
  for (int i = 0; i < PK_COUNT; i++)
    if (pack[i]) f << "pack " << i << "\n";
  f << "best " << bestSeason << "\n";
}

}  // namespace pb

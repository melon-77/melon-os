// Harvest: a run of fields with rising score targets. Seeds earned on each field buy melons (modifiers that
// change the scoring and the machine) and grafts (one-field boosts) in the Seed Market between fields.
// Nothing carries over from one run to the next except what has been unlocked: more melons and seed packs.
#pragma once
#include <string>
#include <vector>

namespace pb {

enum MelonId {
  ML_CANTALOUPE, ML_SLICE_SHOT, ML_WHIRLIGIG, ML_RIND_RUNNER, ML_SEEDLESS, ML_HONEY_TRAP, ML_STEADY_HANDS, ML_APK_CACHE,
  ML_OVERCLOCK, ML_KERNEL_PANIC, ML_SEED_MONEY, ML_ECHO_BUMPER, ML_FEATHERWEIGHT, ML_BITTER, ML_KICKBACK_VINE,
  ML_COMPOST, ML_WILD_GROWTH, ML_CROP_INSURANCE,
  ML_MAGNET_SEED, ML_TWIN_SEEDS, ML_GOLDEN_RIND, ML_GLASS, ML_BALLER, ML_HARVEST_MOON, ML_TENTH_SEED, ML_PORTAL_KEY,
  ML_LAST_SEED, ML_SURVIVOR,
  ML_COUNT
};

enum GraftId { GR_FERTILIZER, GR_EXTRA_VINE, GR_POLLINATOR, GR_IRRIGATION, GR_SUNLAMP, GR_TRELLIS, GR_COUNT };

enum PestId { PE_FROST, PE_DROUGHT, PE_GALE, PE_LOCK, PE_MOLE, PE_HEAT, PE_BLIGHT, PE_WEEVIL, PE_FOG, PE_MAGNET, PE_COUNT };

enum PackId { PK_CANTALOUPE, PK_HONEYDEW, PK_WATERMELON, PK_BITTER, PK_WILD, PK_GAUNTLET, PK_COUNT };

enum Rarity { R_COMMON, R_UNCOMMON, R_RARE };

struct MelonDef {
  const char *name, *desc;
  int rarity, price;
  unsigned rind, flesh;     // icon colours
  int style;                // icon: 0 netted, 1 sliced, 2 smooth, 3 striped, 4 glass, 5 golden
  const char *unlock;       // how to unlock it; nullptr: available from the start
};
struct GraftDef {
  const char *name, *desc;
  int price;
  unsigned color;
};
struct PestDef {
  const char *name, *desc;
  const char *lcd;          // short form for the dot-matrix display (19 characters)
};
struct PackDef {
  const char *name, *desc, *unlock;
};

extern const MelonDef kMelons[ML_COUNT];
extern const GraftDef kGrafts[GR_COUNT];
extern const PestDef kPests[PE_COUNT];
extern const PackDef kPacks[PK_COUNT];

constexpr int kSeasons = 8;
const char *fieldName(int field);           // 0 sprout, 1 vine, 2 pest
long long fieldTarget(int season, int field, int pack);

// What has been unlocked on this computer (variety, never power): kept in the user's data folder.
struct Unlocks {
  bool melon[ML_COUNT] = {};
  bool pack[PK_COUNT] = {};
  int bestSeason = 0;
  bool survivor = false;    // /etc/melon/gauntlet-survivor: the survivor's melon and pack
  void load();
  void save() const;
  bool hasMelon(int m) const { return melon[m] || (m == ML_SURVIVOR && survivor) || !kMelons[m].unlock; }
  bool hasPack(int p) const { return p == PK_CANTALOUPE || pack[p] || (p == PK_GAUNTLET && survivor); }
};

struct OwnedMelon {
  int id;
  double counter = 0;       // scaling melons (Melon Baller)
};

struct Run {
  int pack = PK_CANTALOUPE;
  int season = 1, field = 0;
  int pest = -1;            // the current field's pest
  std::vector<int> pestsSeen;
  long long target = 0, fieldScore = 0;
  int ballsPerField = 2, ballsLeft = 0;
  int seeds = 4;
  int slots = 5;
  std::vector<OwnedMelon> melons;
  std::vector<int> grafts;  // bought for the next field
  std::vector<int> activeGrafts;
  // the market
  std::vector<int> shopMelons;   // -1: sold
  int shopGraft = -1;
  int rerollCost = 2;
  int priceBump = 0;
  // per run / per field counters (unlocks and scaling)
  int melonsSpelled = 0, portalSinks = 0, missionsDone = 0, pestsBeaten = 0, fieldBumpers = 0;
  bool insuranceUsed = false;
  bool won = false;
  std::vector<std::string> unlockedThisRun;

  bool has(int m) const;
  int count() const { return (int)melons.size(); }
  int sellValue(int idx) const;
  int price(int m) const { return kMelons[m].price + priceBump; }
};

}  // namespace pb

// The "Melon Planet" table: its geometry (walls, posts, flippers, switches) and its lamps.
// All sizes are in metres; the playfield is 20.25" x 42" like a standard 1990s machine.
#pragma once
#include <string>
#include <vector>

#include "physics.hpp"

namespace pb {

// Element ids reported in physics events.
enum Elem {
  E_NONE = -1,
  E_BUMPER1 = 0, E_BUMPER2, E_BUMPER3,
  E_SLING_L, E_SLING_R,
  E_LANE_A, E_LANE_P, E_LANE_K,              // top rollover lanes: A-P-K
  E_INLANE_L, E_INLANE_R, E_OUTLANE_L, E_OUTLANE_R,
  E_TARGET_M, E_TARGET_E, E_TARGET_L, E_TARGET_O, E_TARGET_N,   // M-E-L-O-N standups
  E_DROP1, E_DROP2, E_DROP3,                 // seed bank drop targets
  E_PORTAL,                                  // the gauntlet portal (kickout saucer)
  E_ORBIT_ENTER, E_ORBIT_TOP,                // rind loop switches
  E_SHOOTER,                                 // ball left the shooter lane
  E_RAMP_ENTER, E_RAMP_EXIT,                 // kernel ramp
  E_RUBBER,                                  // any plain rubber (small score)
  E_COUNT
};

enum LampShape { L_ROUND, L_ARROW, L_TRIANGLE, L_RECT, L_TEXT };

struct Lamp {
  std::string name;
  V2 p;                     // centre (m)
  double size;              // radius / half size (m)
  double angle;             // arrows point this way (radians)
  int shape;
  unsigned color;           // 0xRRGGBB when lit
  std::string label;        // text drawn on it
};

enum LampId {
  LA_LANE_A, LA_LANE_P, LA_LANE_K,
  LA_M, LA_E, LA_L, LA_O, LA_N,
  LA_DROP1, LA_DROP2, LA_DROP3,
  LA_MISSION1, LA_MISSION2, LA_MISSION3, LA_MISSION4, LA_MISSION5,   // around the portal
  LA_PORTAL, LA_EXTRA_BALL, LA_JACKPOT,
  LA_ORBIT_ARROW, LA_RAMP_ARROW, LA_DROP_ARROW, LA_PORTAL_ARROW,
  LA_MULT2, LA_MULT3, LA_MULT4, LA_MULT5,
  LA_SHOOT_AGAIN, LA_INLANE_L, LA_INLANE_R, LA_OUTLANE_L, LA_OUTLANE_R,
  LA_SKILL,
  LA_COUNT
};

struct Table {
  World world;
  std::vector<Lamp> lamps;
  std::vector<V2> outline;                   // the playfield's edge (inside the cabinet walls)
  std::vector<std::vector<V2>> wallChains;   // for the art: raised walls (closed polygons)
  std::vector<std::vector<V2>> guideChains;  // thin guides / rails (open)
  V2 ballStart;             // in the shooter lane, on the plunger
  int bumperCircle[3];      // indices into world.circles
  int dropSeg[3];           // indices into world.segs
  int flipperL = 0, flipperR = 1;
  V2 portal;

  void build();
  void resetTargets();
};

}  // namespace pb

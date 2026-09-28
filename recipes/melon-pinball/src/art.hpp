// The table's pre-rendered artwork: a top-down "image of 3D" (raised parts drawn with their sides and
// shadows) that the renderer then tilts into perspective, like the old Windows pinball.
#pragma once
#include <SDL3/SDL.h>

#include <vector>

#include "table.hpp"
#include "text.hpp"

namespace pb {

constexpr double kArtScale = 1400;   // texture pixels per metre (1.4 px/mm)
constexpr double kHeightK = 0.8;     // a raised part is drawn shifted up the table by this times its height

struct Sprite {
  SDL_Surface *surf = nullptr;
  int x = 0, y = 0;                  // top-left in table texture pixels
};

struct Art {
  int w = 0, h = 0;
  SDL_Surface *base = nullptr;       // the whole table, lamps off
  SDL_Surface *overlay = nullptr;    // the wire ramp (drawn over balls on the playfield)
  std::vector<Sprite> lamps;         // each lamp lit, by LampId
  Sprite bumperLit[3];
  SDL_Surface *ball = nullptr;       // chrome ball, 128 px
  SDL_Surface *glow = nullptr;       // white radial glow for lit lamps
  SDL_Surface *logo = nullptr;       // the panel's title box
  void free();
};

// `golden`: the player survived the installer's gauntlet (/etc/melon/gauntlet-survivor): gold trim, gold ball.
bool buildArt(Art &art, const Table &t, Text &text, bool golden, const char *dataDir);
SDL_Surface *buildLogo(Text &text, int w, int h, bool golden, const char *dataDir);

}  // namespace pb

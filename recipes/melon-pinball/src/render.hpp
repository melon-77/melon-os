// Draws a frame: the table tilted into perspective, flippers and balls, and the score panel.
#pragma once
#include <SDL3/SDL.h>

#include <map>
#include <string>
#include <vector>

#include "art.hpp"
#include "game.hpp"
#include "text.hpp"

namespace pb {

constexpr int kLogicalW = 1280, kLogicalH = 800;
constexpr int kMenuH = 26;

struct MenuItem {
  std::string label, key;   // key: the shortcut shown on the right
  bool checked = false, separator = false, enabled = true;
};
struct MenuState {
  int open = -1;            // which menu is open
  int hover = -1;           // hovered item
  std::vector<std::string> titles;
  std::vector<std::vector<MenuItem>> items;
  SDL_FRect titleRect[8];
  SDL_FRect itemRect[16];
};

class View {
public:
  bool init(SDL_Renderer *r, Art &art, Text &text);
  void shutdown();
  void draw(const Game &g, const MenuState &menu, bool paused, const std::string &overlay);
  // screen position of a table point (m) at height z (m), and pixels per metre there
  V2 project(double x, double y, double z = 0) const;
  double pxPerM(double x, double y) const;

private:
  SDL_Renderer *r_ = nullptr;
  Art *art_ = nullptr;
  Text *text_ = nullptr;
  SDL_Texture *base_ = nullptr, *overlay_ = nullptr, *target_ = nullptr, *ball_ = nullptr, *glow_ = nullptr, *logo_ = nullptr,
              *dot_ = nullptr;
  std::vector<SDL_Texture *> lamps_;
  SDL_Texture *bumperLit_[3] = {};
  std::vector<SDL_Vertex> mesh_;
  std::vector<int> meshIdx_;
  // camera
  double camH_ = 2.0, camD_ = 1.6, lookZ_ = 0.5;
  double fitS_ = 1, fitX_ = 0, fitY_ = 0;
  std::map<std::string, SDL_Texture *> textCache_;

  V2 rawProject(double x, double y, double z) const;
  void buildMesh();
  void drawTableTexture(const Game &g);
  void drawFlipper(const Flipper &f, bool golden);
  void drawPlunger(const Plunger &p);
  void drawDrops(const Game &g);
  void drawBalls(const Game &g, int layer);
  void drawParticles(const Game &g);
  void drawPopups(const Game &g);
  void drawPanel(const Game &g);
  void drawMenu(const MenuState &m);
  void drawCabinet();
  void dotText(const std::string &s, float x, float y, float pitch, SDL_Color on, int cols = 0, bool center = false);
  void dotBox(const SDL_FRect &rc, int cols, int rows, float pitch, SDL_Color on);
  SDL_Texture *textTex(const std::string &s, float px, FontFace face = FONT_SANS);
  void drawText(const std::string &s, float x, float y, float px, SDL_Color c, int align = -1, FontFace face = FONT_SANS);
  void bevel(const SDL_FRect &rc, bool sunken, SDL_Color fill);
  void poly(const std::vector<V2> &pts, SDL_FColor c);  // convex polygon in table texture pixels
};

}  // namespace pb

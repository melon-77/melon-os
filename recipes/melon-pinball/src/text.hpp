// Text: TrueType through SDL3_ttf when melon's fonts are installed, the dot-matrix font otherwise.
#pragma once
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <map>
#include <string>

namespace pb {

enum FontFace { FONT_SANS, FONT_SANS_ITALIC, FONT_MONO };

class Text {
public:
  bool init();
  void shutdown();
  // white text with alpha; the caller tints it. `px` is the cap-to-descender size in pixels.
  SDL_Surface *render(const std::string &s, float px, FontFace face = FONT_SANS);
  bool haveTrueType() const { return !paths_[FONT_SANS].empty(); }
  // width and height of `s` as rendered at `px` (unscaled surface pixels)
  bool size(const std::string &s, float px, FontFace face, int &w, int &h);

private:
  TTF_Font *font(float px, FontFace face);
  std::string paths_[3];
  std::map<std::pair<int, int>, TTF_Font *> cache_;
  bool ttf_ = false;
};

// Render with the 5x7 dot font into a white-on-transparent surface (each dot a square of `dot` pixels).
SDL_Surface *renderDotText(const std::string &s, int dot);

}  // namespace pb

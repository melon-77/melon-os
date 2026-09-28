#include "text.hpp"

#include <cstdlib>

#include "dotfont.hpp"

namespace pb {

static std::string findFont(std::initializer_list<const char *> names) {
  // melon's font packages (font-noto, font-hack), then the usual places on other systems
  const char *dirs[] = {"/usr/share/fonts/noto/", "/usr/share/fonts/hack/", "/usr/share/fonts/truetype/noto/",
                        "/usr/share/fonts/truetype/hack/", "/usr/share/fonts/TTF/", "/usr/share/fonts/truetype/dejavu/",
                        "/usr/share/fonts/dejavu/"};
  for (const char *n : names)
    for (const char *d : dirs) {
      std::string p = std::string(d) + n;
      SDL_PathInfo info;
      if (SDL_GetPathInfo(p.c_str(), &info) && info.type == SDL_PATHTYPE_FILE) return p;
    }
  return "";
}

bool Text::init() {
  ttf_ = TTF_Init();
  if (!ttf_) return false;
  paths_[FONT_SANS] = findFont({"NotoSans-Bold.ttf", "DejaVuSans-Bold.ttf", "Hack-Bold.ttf"});
  paths_[FONT_SANS_ITALIC] = findFont({"NotoSans-BoldItalic.ttf", "NotoSans-Bold.ttf", "DejaVuSans-BoldOblique.ttf"});
  paths_[FONT_MONO] = findFont({"Hack-Bold.ttf", "NotoSansMono-Bold.ttf", "DejaVuSansMono-Bold.ttf"});
  if (const char *f = std::getenv("MELON_PINBALL_FONT")) paths_[FONT_SANS] = paths_[FONT_SANS_ITALIC] = f;
  for (auto &p : paths_)
    if (p.empty()) p = paths_[FONT_SANS];
  return true;
}

void Text::shutdown() {
  for (auto &kv : cache_) TTF_CloseFont(kv.second);
  cache_.clear();
  if (ttf_) TTF_Quit();
  ttf_ = false;
}

SDL_Surface *Text::render(const std::string &s, float px, FontFace face) {
  if (s.empty()) return nullptr;
  if (!ttf_ || paths_[face].empty()) {
    int dot = std::max(1, (int)(px / 8.f + 0.5f));
    return renderDotText(s, dot);
  }
  int size = std::max(4, (int)(px + 0.5f));
  auto key = std::make_pair((int)face, size);
  TTF_Font *f = cache_[key];
  if (!f) {
    f = TTF_OpenFont(paths_[face].c_str(), (float)size);
    if (!f) return renderDotText(s, std::max(1, size / 8));
    cache_[key] = f;
  }
  SDL_Color white = {255, 255, 255, 255};
  return TTF_RenderText_Blended(f, s.c_str(), 0, white);
}

SDL_Surface *renderDotText(const std::string &s, int dot) {
  int w = (int)s.size() * 6 * dot, h = 7 * dot;
  if (w <= 0) return nullptr;
  SDL_Surface *out = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_ARGB8888);
  if (!out) return nullptr;
  SDL_FillSurfaceRect(out, nullptr, 0);
  for (size_t i = 0; i < s.size(); i++) {
    auto g = dotGlyph(s[i]);
    for (int r = 0; r < 7; r++)
      for (int c = 0; c < 5; c++)
        if (g[r][c] == '#') {
          SDL_Rect rc = {(int)(i * 6 + c) * dot, r * dot, dot, dot};
          SDL_FillSurfaceRect(out, &rc, 0xffffffff);
        }
  }
  return out;
}

}  // namespace pb

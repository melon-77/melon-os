// melon pinball: a table on the melon planet, in the spirit of the old Windows pinball.
//   melon-pinball [--fullscreen] [--golden] [--screenshot FILE [--seconds N] [--play]]
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

#include "art.hpp"
#include "audio.hpp"
#include "game.hpp"
#include "render.hpp"
#include "text.hpp"

#ifndef PINBALL_DATADIR
#define PINBALL_DATADIR "/usr/share/melon-pinball"
#endif
#ifndef PINBALL_VERSION
#define PINBALL_VERSION "1.0"
#endif

using namespace pb;

namespace {

const char *kHelp =
    "#How to play\n"
    "Flippers: Z / Left Shift / Left arrow and / / Right Shift / Right arrow (right also works the mini flipper)\n"
    "Plunger: hold Space, Down or Enter, then let go.   Nudge: X, . and Up (too much tilts)\n"
    "Gamepad: shoulders or triggers flip, A plunges, the D-pad nudges\n"
    "\n"
    "#The table\n"
    "Spell M-E-L-O-N on the angled bank (the right and mini flippers aim at it); every A-P-K lane\n"
    "lights a letter too. M-E-L-O-N lights the portal, and the portal starts the next mission.\n"
    "The spinner lights the kickback in the left outlane. Five missions raise your rank;\n"
    "finish them all for the Seed Storm multiball.\n"
    "\n"
    "#Harvest (F2)\n"
    "Each field has a score target and two balls. Reach it to harvest the field and earn seeds.\n"
    "Spend seeds in the Seed Market on melons, which change the scoring and the machine,\n"
    "and grafts, which help on the next field. The third field of every season has a pest.\n"
    "Eight seasons make a harvest. Unlock more melons and seed packs as you play.\n"
    "\n"
    "Esc or click to close";

const char *kAbout =
    "#melon pinball\n"
    "Version " PINBALL_VERSION ", made for melon\n"
    "A steel ball on a table tilted 6.5 degrees, simulated 2000 times a second:\n"
    "real flipper strokes, rubber and steel bounces, a wire ramp you have to earn.\n"
    "Art and sound are generated when the game starts.\n"
    "\n"
    "Esc or click to close";

struct Options {
  bool fullscreen = false, sound = true;
};

std::string prefFile() {
  char *p = SDL_GetPrefPath("melon", "pinball");
  std::string s = p ? std::string(p) + "options.txt" : "";
  SDL_free(p);
  return s;
}

Options loadOptions() {
  Options o;
  std::ifstream f(prefFile());
  std::string k;
  int v;
  while (f >> k >> v) {
    if (k == "fullscreen") o.fullscreen = v;
    if (k == "sound") o.sound = v;
  }
  return o;
}

void saveOptions(const Options &o) {
  std::string p = prefFile();
  if (p.empty()) return;
  std::ofstream f(p);
  f << "fullscreen " << o.fullscreen << "\nsound " << o.sound << "\n";
}

bool fileExists(const char *p) {
  SDL_PathInfo info;
  return SDL_GetPathInfo(p, &info) && info.type == SDL_PATHTYPE_FILE;
}

enum Cmd {
  CMD_NONE, CMD_NEW, CMD_CLASSIC, CMD_PAUSE, CMD_COLLECTION, CMD_SCORES, CMD_QUIT, CMD_FULLSCREEN, CMD_SOUND, CMD_HELP,
  CMD_ABOUT
};

struct App {
  SDL_Window *win = nullptr;
  SDL_Renderer *ren = nullptr;
  Text text;
  Art art;
  View view;
  Game game;
  Audio audio;
  bool audioOk = false;
  Options opt;
  MenuState menu;
  UiState ui;
  std::vector<std::vector<Cmd>> menuCmds;
  bool paused = false, running = true;
  std::string overlay;
  bool keyL[3] = {}, keyR[3] = {}, keyP[3] = {};
  SDL_Gamepad *pad = nullptr;
  float padTrigL = 0, padTrigR = 0;

  void buildMenu() {
    menu.titles = {"Game", "Options", "Help"};
    menu.items = {
        {{"New Harvest Run", "F2"}, {"Classic Game", "F5"}, {"Pause / Resume", "F3"}, {"", "", false, true},
         {"Melon Collection", "F6"}, {"High Scores", ""}, {"", "", false, true}, {"Quit", "Ctrl+Q"}},
        {{"Full Screen", "F4", opt.fullscreen}, {"Sound", "M", opt.sound}},
        {{"How to Play", "F1"}, {"About melon pinball", ""}}};
    menuCmds = {{CMD_NEW, CMD_CLASSIC, CMD_PAUSE, CMD_NONE, CMD_COLLECTION, CMD_SCORES, CMD_NONE, CMD_QUIT},
                {CMD_FULLSCREEN, CMD_SOUND},
                {CMD_HELP, CMD_ABOUT}};
    float x = 4;
    for (size_t i = 0; i < menu.titles.size(); i++) {
      float w = 24 + 9.5f * (float)menu.titles[i].size();
      menu.titleRect[i] = {x, 0, w, (float)kMenuH};
      x += w;
    }
  }

  void layoutItems() {
    if (menu.open < 0) return;
    float x = menu.titleRect[menu.open].x, y = kMenuH + 2;
    for (size_t i = 0; i < menu.items[menu.open].size(); i++) {
      float h = menu.items[menu.open][i].separator ? 10.f : 26.f;
      menu.itemRect[i] = {x + 2, y, 250, h};
      y += h;
    }
  }

  // the Harvest screens: shop, seed packs, collection
  bool screen() const { return ui.collection || game.mode == MODE_SHOP || game.mode == MODE_PACK_SELECT; }
  std::vector<UiItem> items() {
    if (ui.collection) return view.collectionItems();
    if (game.mode == MODE_SHOP) return view.shopItems(game);
    if (game.mode == MODE_PACK_SELECT) return view.packItems(game);
    return {};
  }

  void activate(const UiItem &it) {
    switch (it.act) {
      case UA_BUY_MELON: game.buyMelon(it.index); ui.sellArmed = -1; break;
      case UA_BUY_GRAFT: game.buyGraft(); ui.sellArmed = -1; break;
      case UA_SELL:
        if (ui.sellArmed == it.index) {
          game.sellMelon(it.index);
          ui.sellArmed = -1;
          int n = (int)items().size();
          ui.focus = std::min(ui.focus, n - 1);
        } else {
          ui.sellArmed = it.index;
        }
        break;
      case UA_REROLL: game.reroll(); ui.sellArmed = -1; break;
      case UA_NEXT: game.nextField(); ui.sellArmed = -1; ui.focus = 0; break;
      case UA_PACK:
        if (game.unlocks.hasPack(it.index)) game.startRun(it.index);
        break;
      case UA_MELON_INFO: ui.collFocus = it.index; break;
      default: break;
    }
  }

  // move the focus to the nearest item in a direction
  void moveFocus(int dx, int dy) {
    auto v = items();
    if (v.empty()) return;
    int &f = ui.collection ? ui.collFocus : ui.focus;
    f = std::clamp(f, 0, (int)v.size() - 1);
    float cx = v[f].r.x + v[f].r.w / 2, cy = v[f].r.y + v[f].r.h / 2;
    int best = -1;
    float bestD = 1e9f;
    for (size_t i = 0; i < v.size(); i++) {
      if ((int)i == f) continue;
      float x = v[i].r.x + v[i].r.w / 2 - cx, y = v[i].r.y + v[i].r.h / 2 - cy;
      float along = x * dx + y * dy, across = std::fabs(x * dy) + std::fabs(y * dx);
      if (along <= 1) continue;
      float d = along + across * 2.5f;
      if (d < bestD) { bestD = d; best = (int)i; }
    }
    if (best >= 0) f = best;
    ui.sellArmed = -1;
  }

  void activateFocused() {
    auto v = items();
    int f = ui.collection ? ui.collFocus : ui.focus;
    if (f >= 0 && f < (int)v.size()) activate(v[f]);
  }

  void run(Cmd c) {
    switch (c) {
      case CMD_NEW:
        overlay.clear(); paused = false; ui = UiState();
        game.newRun();
        break;
      case CMD_CLASSIC: overlay.clear(); paused = false; ui = UiState(); game.newGame(); break;
      case CMD_PAUSE: if (overlay.empty()) paused = !paused; break;
      case CMD_COLLECTION:
        ui.collection = !ui.collection;
        paused = ui.collection || !overlay.empty();
        break;
      case CMD_SCORES: {
        overlay = "#High scores (classic)\n";
        if (game.hiscores.empty()) overlay += "Nobody yet. Be the first!\n";
        int n = 1;
        for (auto &h : game.hiscores) overlay += std::to_string(n++) + ".  " + h.name + "   " + std::to_string(h.score) + "\n";
        overlay += "\nBest harvest: season " + std::to_string(game.unlocks.bestSeason) + "\n";
        overlay += "\nEsc or click to close";
        paused = true;
        break;
      }
      case CMD_QUIT: running = false; break;
      case CMD_FULLSCREEN:
        opt.fullscreen = !opt.fullscreen;
        SDL_SetWindowFullscreen(win, opt.fullscreen);
        saveOptions(opt);
        buildMenu();
        break;
      case CMD_SOUND:
        opt.sound = !opt.sound;
        audio.setMuted(!opt.sound);
        saveOptions(opt);
        buildMenu();
        break;
      case CMD_HELP: overlay = kHelp; paused = true; break;
      case CMD_ABOUT: overlay = kAbout; paused = true; break;
      default: break;
    }
  }

  void closeOverlay() {
    if (ui.collection) { ui.collection = false; paused = !overlay.empty(); return; }
    if (!overlay.empty()) {
      overlay.clear();
      paused = false;
    }
  }

  void flipInput() {
    bool l = keyL[0] || keyL[1] || keyL[2] || padTrigL > 0.35f, r = keyR[0] || keyR[1] || keyR[2] || padTrigR > 0.35f;
    bool p = keyP[0] || keyP[1] || keyP[2];
    if (paused || screen()) return;
    game.flipper(0, l);
    game.flipper(1, r);
    game.plunger(p);
  }

  // keys on the Harvest screens; true when used
  bool screenKey(SDL_Keycode k) {
    if (!screen()) return false;
    switch (k) {
      case SDLK_LEFT: case SDLK_A: moveFocus(-1, 0); return true;
      case SDLK_RIGHT: case SDLK_D: moveFocus(1, 0); return true;
      case SDLK_UP: case SDLK_W: moveFocus(0, -1); return true;
      case SDLK_DOWN: case SDLK_S: moveFocus(0, 1); return true;
      case SDLK_RETURN: case SDLK_SPACE: case SDLK_KP_ENTER: activateFocused(); return true;
      case SDLK_R: if (game.mode == MODE_SHOP && !ui.collection) game.reroll(); return true;
      case SDLK_N: if (game.mode == MODE_SHOP && !ui.collection) { game.nextField(); ui.focus = 0; } return true;
      case SDLK_ESCAPE:
        if (ui.collection) closeOverlay();
        else if (game.mode == MODE_PACK_SELECT) game.toAttract();
        else return false;
        return true;
      default: return false;
    }
  }

  void key(SDL_Keycode k, bool down, bool repeat) {
    if (down && menu.open < 0 && screenKey(k)) return;
    if (repeat) return;
    if (!screen()) {
      switch (k) {
        case SDLK_Z: case SDLK_LSHIFT: case SDLK_LEFT: keyL[k == SDLK_Z ? 0 : (k == SDLK_LSHIFT ? 1 : 2)] = down; break;
        case SDLK_SLASH: case SDLK_RSHIFT: case SDLK_RIGHT: keyR[k == SDLK_SLASH ? 0 : (k == SDLK_RSHIFT ? 1 : 2)] = down; break;
        case SDLK_SPACE: case SDLK_DOWN: case SDLK_RETURN: keyP[k == SDLK_SPACE ? 0 : (k == SDLK_DOWN ? 1 : 2)] = down; break;
        default: break;
      }
    } else {
      for (int i = 0; i < 3; i++) keyL[i] = keyR[i] = keyP[i] = false;
    }
    if (down) {
      switch (k) {
        case SDLK_X: if (!paused) game.nudge(V2(-1, -0.4)); break;
        case SDLK_PERIOD: if (!paused) game.nudge(V2(1, -0.4)); break;
        case SDLK_UP: if (!paused) game.nudge(V2(0, -1)); break;
        case SDLK_F1: run(CMD_HELP); break;
        case SDLK_F2: run(CMD_NEW); break;
        case SDLK_F5: run(CMD_CLASSIC); break;
        case SDLK_F6: run(CMD_COLLECTION); break;
        case SDLK_F3: run(CMD_PAUSE); break;
        case SDLK_F4: case SDLK_F11: run(CMD_FULLSCREEN); break;
        case SDLK_M: run(CMD_SOUND); break;
        case SDLK_RETURN:
          if (game.mode == MODE_RUN_OVER) run(CMD_NEW);
          break;
        case SDLK_ESCAPE:
          if (menu.open >= 0) menu.open = -1;
          else if (!overlay.empty() || ui.collection) closeOverlay();
          else { menu.open = 0; menu.hover = -1; layoutItems(); if (game.mode == MODE_PLAY) paused = true; }
          break;
        default: break;
      }
    }
    flipInput();
  }

  void padButton(Uint8 b, bool down) {
    if (screen()) {
      if (!down) return;
      switch (b) {
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT: moveFocus(-1, 0); break;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: moveFocus(1, 0); break;
        case SDL_GAMEPAD_BUTTON_DPAD_UP: moveFocus(0, -1); break;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN: moveFocus(0, 1); break;
        case SDL_GAMEPAD_BUTTON_SOUTH: activateFocused(); break;
        case SDL_GAMEPAD_BUTTON_NORTH: if (game.mode == MODE_SHOP) game.reroll(); break;
        case SDL_GAMEPAD_BUTTON_START: if (game.mode == MODE_SHOP) { game.nextField(); ui.focus = 0; } break;
        case SDL_GAMEPAD_BUTTON_EAST: screenKey(SDLK_ESCAPE); break;
        default: break;
      }
      return;
    }
    switch (b) {
      case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: keyL[2] = down; break;
      case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: keyR[2] = down; break;
      case SDL_GAMEPAD_BUTTON_SOUTH: case SDL_GAMEPAD_BUTTON_DPAD_DOWN: keyP[2] = down; break;
      case SDL_GAMEPAD_BUTTON_DPAD_LEFT: case SDL_GAMEPAD_BUTTON_WEST: if (down && !paused) game.nudge(V2(-1, -0.4)); break;
      case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: case SDL_GAMEPAD_BUTTON_EAST: if (down && !paused) game.nudge(V2(1, -0.4)); break;
      case SDL_GAMEPAD_BUTTON_DPAD_UP: case SDL_GAMEPAD_BUTTON_NORTH: if (down && !paused) game.nudge(V2(0, -1)); break;
      case SDL_GAMEPAD_BUTTON_START:
        if (down) {
          if (!overlay.empty()) closeOverlay();
          else if (game.mode == MODE_PLAY) run(CMD_PAUSE);
          else run(CMD_NEW);
        }
        break;
      case SDL_GAMEPAD_BUTTON_BACK: if (down) run(CMD_PAUSE); break;
      default: break;
    }
    flipInput();
  }

  void mouse(float x, float y, bool click) {
    int over = -1;
    for (size_t i = 0; i < menu.titles.size(); i++) {
      auto &r = menu.titleRect[i];
      if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) over = (int)i;
    }
    if (over >= 0 && (click || menu.open >= 0)) {
      if (click && menu.open == over) menu.open = -1;
      else { menu.open = over; layoutItems(); }
      menu.hover = -1;
      if (menu.open >= 0 && game.mode == MODE_PLAY) paused = true;
      return;
    }
    if (menu.open >= 0) {
      menu.hover = -1;
      for (size_t i = 0; i < menu.items[menu.open].size(); i++) {
        auto &r = menu.itemRect[i];
        if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) menu.hover = (int)i;
      }
      if (click) {
        int m = menu.open, h = menu.hover;
        menu.open = -1;
        if (h >= 0 && !menu.items[m][h].separator) {
          Cmd c = menuCmds[m][h];
          if (c != CMD_PAUSE && overlay.empty() && game.mode == MODE_PLAY && c != CMD_SCORES && c != CMD_HELP &&
              c != CMD_ABOUT && c != CMD_COLLECTION)
            paused = false;
          run(c);
        } else if (overlay.empty() && !ui.collection) {
          paused = false;
        }
      }
      return;
    }
    if (screen()) {                                  // hover focuses, click activates
      auto v = items();
      for (size_t i = 0; i < v.size(); i++) {
        auto &r = v[i].r;
        if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) {
          int &f = ui.collection ? ui.collFocus : ui.focus;
          if (f != (int)i && !ui.collection) ui.sellArmed = -1;
          f = ui.collection ? v[i].index : (int)i;
          if (click) activate(v[i]);
          return;
        }
      }
      return;
    }
    if (click && !overlay.empty()) closeOverlay();
  }
};

}  // namespace

int main(int argc, char **argv) {
  const char *shot = nullptr;
  double shotSecs = 6;
  bool forceGold = false, noGold = false, play = false, fullscreenArg = false;
  bool harvest = false, shopShot = false, packShot = false, collShot = false, lazy = false;
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "--screenshot") && i + 1 < argc) shot = argv[++i];
    else if (!std::strcmp(argv[i], "--seconds") && i + 1 < argc) shotSecs = std::atof(argv[++i]);
    else if (!std::strcmp(argv[i], "--golden")) forceGold = true;
    else if (!std::strcmp(argv[i], "--no-golden")) noGold = true;
    else if (!std::strcmp(argv[i], "--play")) play = true;
    else if (!std::strcmp(argv[i], "--harvest")) harvest = true;
    else if (!std::strcmp(argv[i], "--lazy")) lazy = true;           // the demo never flips (reach the end fast)
    else if (!std::strcmp(argv[i], "--shop")) shopShot = true;
    else if (!std::strcmp(argv[i], "--packs")) packShot = true;
    else if (!std::strcmp(argv[i], "--collection")) collShot = true;
    else if (!std::strcmp(argv[i], "--fullscreen")) fullscreenArg = true;
    else if (!std::strcmp(argv[i], "--version")) { std::printf("melon-pinball %s\n", PINBALL_VERSION); return 0; }
    else {
      std::printf("usage: melon-pinball [--fullscreen] [--golden]\n"
                  "       melon-pinball --screenshot FILE.png [--seconds N] [--play [--harvest]] [--shop] [--packs]\n"
                  "                     [--collection]   (render one frame and exit)\n");
      return std::strcmp(argv[i], "--help") ? 1 : 0;
    }
  }
  const char *dataDir = std::getenv("MELON_PINBALL_DATA");
  if (!dataDir) dataDir = PINBALL_DATADIR;

  SDL_SetAppMetadata("melon pinball", PINBALL_VERSION, "org.melon.pinball");
  SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_PREFER_LIBDECOR, "0");
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS)) {
    std::fprintf(stderr, "melon-pinball: %s\n", SDL_GetError());
    return 1;
  }
  App app;
  app.opt = loadOptions();
  if (fullscreenArg) app.opt.fullscreen = true;
  if (shot) app.opt.fullscreen = false;
  SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | (app.opt.fullscreen ? SDL_WINDOW_FULLSCREEN : 0);
  if (!SDL_CreateWindowAndRenderer("melon pinball", kLogicalW, kLogicalH, flags, &app.win, &app.ren)) {
    std::fprintf(stderr, "melon-pinball: %s\n", SDL_GetError());
    return 1;
  }
  SDL_SetRenderLogicalPresentation(app.ren, kLogicalW, kLogicalH, SDL_LOGICAL_PRESENTATION_LETTERBOX);
  SDL_SetRenderVSync(app.ren, 1);
  SDL_SetRenderDrawBlendMode(app.ren, SDL_BLENDMODE_BLEND);
  if (SDL_Surface *icon = IMG_Load((std::string(dataDir) + "/icon.svg").c_str())) {
    SDL_SetWindowIcon(app.win, icon);
    SDL_DestroySurface(icon);
  }
  app.text.init();

  // a first frame while the table is painted
  SDL_SetRenderDrawColor(app.ren, 12, 16, 14, 255);
  SDL_RenderClear(app.ren);
  if (SDL_Surface *s = app.text.render("painting the melon planet...", 26, FONT_SANS_ITALIC)) {
    SDL_Texture *t = SDL_CreateTextureFromSurface(app.ren, s);
    SDL_FRect rc = {(kLogicalW - s->w) / 2.f, (kLogicalH - s->h) / 2.f, (float)s->w, (float)s->h};
    SDL_SetTextureColorMod(t, 111, 191, 74);
    SDL_RenderTexture(app.ren, t, nullptr, &rc);
    SDL_DestroyTexture(t);
    SDL_DestroySurface(s);
  }
  SDL_RenderPresent(app.ren);

  bool golden = forceGold || (!noGold && fileExists("/etc/melon/gauntlet-survivor"));
  app.game.init(golden);
  if (!buildArt(app.art, app.game.table, app.text, golden, dataDir) || !app.view.init(app.ren, app.art, app.text)) {
    std::fprintf(stderr, "melon-pinball: could not build the table: %s\n", SDL_GetError());
    return 1;
  }
  app.audioOk = !shot && app.audio.init();
  app.audio.setMuted(!app.opt.sound);
  app.game.sound = [&app](Sfx s, float v, float p) { if (app.audioOk) app.audio.play(s, v, p); };
  app.buildMenu();

  if (shot) {
    // render the attract mode (or a game played by the demo) for a while, save one frame
    if (play && harvest) app.game.startRun(PK_CANTALOUPE);
    else if (play) app.game.newGame();
    if (shopShot) {                                  // straight to the Seed Market after a won field
      app.game.startRun(PK_CANTALOUPE);
      app.game.run.fieldScore = app.game.run.target;
    }
    if (packShot) app.game.newRun();
    if (collShot) app.ui.collection = true;
    double t = 0;
    while (t < shotSecs) {
      app.game.update(1.0 / 60);
      if (play && (app.game.mode == MODE_SHOP)) app.game.nextField();   // the demo buys nothing
      if (play) {                                    // let the demo's flipper logic play the real game
        static double pull = -1, cool = 0;
        auto &pl = app.game.table.world.plunger;
        cool -= 1.0 / 60;
        if (pull < 0 && cool <= 0 && app.game.plungerReady() && !pl.pulling && pl.pos == 0) { app.game.plunger(true); pull = 0.45; }
        if (pull >= 0 && (pull -= 1.0 / 60) < 0) { app.game.plunger(false); cool = 1.5; }
        for (int s = 0; s < 2; s++) {
          bool want = false;
          for (int k : s == 0 ? std::vector<int>{0} : std::vector<int>{1, 2}) {
            auto &f = app.game.table.world.flippers[k];
            for (auto &b : app.game.table.world.balls) {
              if (!b.alive || b.layer) continue;
              V2 d = b.p - f.pivot, ax(std::cos(f.rest), std::sin(f.rest));
              if (dot(d, ax) > 0.02 && dot(d, ax) < f.length + 0.01 && std::fabs(cross(ax, d)) < 0.03) want = true;
            }
          }
          app.game.flipper(s, want && !lazy);
        }
      }
      t += 1.0 / 60;
    }
    if (shopShot) app.ui.focus = 1;
    app.view.draw(app.game, app.menu, false, "", app.ui);
    SDL_Surface *s = SDL_RenderReadPixels(app.ren, nullptr);
    std::string sp = shot;
    bool png = sp.size() > 4 && sp.substr(sp.size() - 4) == ".png";
    if (!s || !(png ? IMG_SavePNG(s, shot) : SDL_SaveBMP(s, shot))) {
      std::fprintf(stderr, "melon-pinball: screenshot failed: %s\n", SDL_GetError());
      return 1;
    }
    std::printf("mode %d, score %lld, ball %d, season %d field %d (%lld of %lld), seeds %d\n", (int)app.game.mode,
                app.game.score, app.game.ball, app.game.run.season, app.game.run.field, app.game.run.fieldScore,
                app.game.run.target, app.game.run.seeds);
    SDL_DestroySurface(s);
    return 0;
  }

  Uint64 last = SDL_GetPerformanceCounter();
  while (app.running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
        case SDL_EVENT_QUIT: app.running = false; break;
        case SDL_EVENT_KEY_DOWN:
          if ((e.key.mod & SDL_KMOD_CTRL) && e.key.key == SDLK_Q) app.running = false;
          else app.key(e.key.key, true, e.key.repeat);
          break;
        case SDL_EVENT_KEY_UP: app.key(e.key.key, false, false); break;
        case SDL_EVENT_MOUSE_MOTION:
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
          SDL_ConvertEventToRenderCoordinates(app.ren, &e);
          bool click = e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT;
          float x = click ? e.button.x : e.motion.x, y = click ? e.button.y : e.motion.y;
          app.mouse(x, y, click);
          break;
        }
        case SDL_EVENT_GAMEPAD_ADDED:
          if (!app.pad) app.pad = SDL_OpenGamepad(e.gdevice.which);
          break;
        case SDL_EVENT_GAMEPAD_REMOVED:
          if (app.pad && SDL_GetGamepadID(app.pad) == e.gdevice.which) { SDL_CloseGamepad(app.pad); app.pad = nullptr; }
          break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN: app.padButton(e.gbutton.button, true); break;
        case SDL_EVENT_GAMEPAD_BUTTON_UP: app.padButton(e.gbutton.button, false); break;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
          if (e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER) app.padTrigL = e.gaxis.value / 32767.f;
          if (e.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) app.padTrigR = e.gaxis.value / 32767.f;
          app.flipInput();
          break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
          if (app.game.mode == MODE_PLAY) app.paused = true;
          break;
        default: break;
      }
    }
    Uint64 now = SDL_GetPerformanceCounter();
    double dt = (double)(now - last) / SDL_GetPerformanceFrequency();
    last = now;
    if (!app.paused) app.game.update(dt);
    // the rolling rumble follows the fastest ball on the playfield
    float roll = 0;
    for (auto &b : app.game.table.world.balls)
      if (b.alive && b.captured < 0 && b.layer == 0) roll = std::max(roll, (float)len(b.v));
    app.audio.setRolling(app.paused || app.game.mode == MODE_ATTRACT ? 0 : roll);
    app.view.draw(app.game, app.menu, app.paused, app.overlay, app.ui);
    SDL_RenderPresent(app.ren);
  }
  app.audio.shutdown();
  app.view.shutdown();
  app.art.free();
  app.text.shutdown();
  if (app.pad) SDL_CloseGamepad(app.pad);
  SDL_DestroyRenderer(app.ren);
  SDL_DestroyWindow(app.win);
  SDL_Quit();
  return 0;
}

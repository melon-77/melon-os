// Sound: every effect is synthesised at start-up (no sound files), mixed in SDL's audio callback,
// plus the rumble of rolling balls.
#pragma once
#include <SDL3/SDL.h>

#include <vector>

#include "game.hpp"

namespace pb {

class Audio {
public:
  bool init();
  void shutdown();
  void play(Sfx s, float vol, float pan);
  void setRolling(float speed);        // m/s of the fastest ball on the playfield
  void setMuted(bool m) { muted_ = m; }
  bool muted() const { return muted_; }

private:
  struct Voice {
    const std::vector<float> *buf;
    double pos, rate;
    float vol, pan;
  };
  SDL_AudioStream *stream_ = nullptr;
  std::vector<float> sfx_[SFX_COUNT];
  std::vector<Voice> voices_;
  float roll_ = 0, rollNow_ = 0, rollLp_ = 0, rollLp2_ = 0;
  unsigned rng_ = 12345;
  bool muted_ = false;
  static void SDLCALL callback(void *ud, SDL_AudioStream *s, int additional, int total);
  void mix(float *out, int frames);
  void synth();
};

}  // namespace pb

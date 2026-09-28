#include "audio.hpp"

#include <algorithm>
#include <cmath>

namespace pb {

static const int kRate = 48000;

namespace {
struct Buf {
  std::vector<float> s;
  unsigned seed = 1;
  float noise() {
    seed = seed * 1664525u + 1013904223u;
    return ((seed >> 9) & 0x7fff) / 16384.f - 1.f;
  }
  void ensure(size_t n) {
    if (s.size() < n) s.resize(n, 0.f);
  }
  // a tone gliding from f0 to f1 (exponentially), with attack and exponential decay
  void tone(double at, double dur, double f0, double f1, float amp, double decay, int wave = 0, double attack = 0.002) {
    size_t a = (size_t)(at * kRate), n = (size_t)(dur * kRate);
    ensure(a + n);
    double ph = 0;
    for (size_t i = 0; i < n; i++) {
      double t = (double)i / kRate, k = (double)i / n;
      double f = f0 * std::pow(f1 / f0, k);
      ph += f / kRate;
      double x = ph - std::floor(ph);
      double w;
      switch (wave) {
        case 1: w = x < 0.5 ? 1 : -1; w *= 0.6; break;             // square
        case 2: w = 4 * std::fabs(x - 0.5) - 1; break;               // triangle
        default: w = std::sin(2 * kPi * x);
      }
      double env = std::min(1.0, t / attack) * std::exp(-t / decay);
      if (i + 200 > n) env *= (n - i) / 200.0;                        // no click at the end
      s[a + i] += (float)(w * env * amp);
    }
  }
  // noise through a one-pole band (lp - lower lp), with decay
  void hiss(double at, double dur, double lo, double hi, float amp, double decay, double attack = 0.001) {
    size_t a = (size_t)(at * kRate), n = (size_t)(dur * kRate);
    ensure(a + n);
    double l1 = 0, l2 = 0;
    double ah = 1 - std::exp(-2 * kPi * hi / kRate), al = 1 - std::exp(-2 * kPi * lo / kRate);
    for (size_t i = 0; i < n; i++) {
      double t = (double)i / kRate;
      double x = noise();
      l1 += ah * (x - l1);
      l2 += al * (l1 - l2);
      double env = std::min(1.0, t / attack) * std::exp(-t / decay);
      if (i + 200 > n) env *= (n - i) / 200.0;
      s[a + i] += (float)((l1 - l2) * env * amp);
    }
  }
  // noise swept between two band centres
  void sweep(double at, double dur, double f0, double f1, float amp) {
    size_t a = (size_t)(at * kRate), n = (size_t)(dur * kRate);
    ensure(a + n);
    double l1 = 0, l2 = 0;
    for (size_t i = 0; i < n; i++) {
      double k = (double)i / n, f = f0 * std::pow(f1 / f0, k);
      double ah = 1 - std::exp(-2 * kPi * f * 1.5 / kRate), al = 1 - std::exp(-2 * kPi * f * 0.6 / kRate);
      double x = noise();
      l1 += ah * (x - l1);
      l2 += al * (l1 - l2);
      double env = std::sin(kPi * k);
      s[a + i] += (float)((l1 - l2) * env * amp);
    }
  }
};
}  // namespace

void Audio::synth() {
  auto note = [](int semis) { return 440.0 * std::pow(2.0, semis / 12.0); };   // semitones from A4
  Buf b[SFX_COUNT];
  b[SFX_FLIP_UP].hiss(0, 0.05, 900, 5000, 0.9f, 0.008);
  b[SFX_FLIP_UP].tone(0, 0.09, 110, 70, 0.8f, 0.025);
  b[SFX_FLIP_DOWN].hiss(0, 0.04, 600, 3000, 0.4f, 0.006);
  b[SFX_FLIP_DOWN].tone(0, 0.06, 90, 60, 0.4f, 0.02);
  b[SFX_BUMPER].tone(0, 0.25, 210, 120, 0.9f, 0.07);
  b[SFX_BUMPER].hiss(0, 0.05, 1500, 7000, 0.7f, 0.006);
  b[SFX_BUMPER].tone(0, 0.35, 1175, 1170, 0.18f, 0.12);        // a bright ring
  b[SFX_BUMPER].tone(0, 0.35, 1760, 1750, 0.10f, 0.09);
  b[SFX_SLING].hiss(0, 0.05, 1200, 8000, 0.9f, 0.007);
  b[SFX_SLING].tone(0, 0.12, 260, 140, 0.7f, 0.035);
  b[SFX_RUBBER].tone(0, 0.12, 420, 300, 0.35f, 0.035);
  b[SFX_RUBBER].hiss(0, 0.03, 800, 3000, 0.2f, 0.005);
  b[SFX_WALL].tone(0, 0.1, 140, 90, 0.5f, 0.03);
  b[SFX_WALL].hiss(0, 0.04, 300, 2500, 0.3f, 0.008);
  b[SFX_TARGET].tone(0, 0.08, 760, 700, 0.5f, 0.02, 1);
  b[SFX_TARGET].hiss(0, 0.03, 1000, 6000, 0.4f, 0.005);
  b[SFX_DROP].hiss(0, 0.12, 400, 4000, 0.7f, 0.03);
  b[SFX_DROP].tone(0, 0.12, 320, 160, 0.6f, 0.04);
  b[SFX_ROLLOVER].tone(0, 0.5, note(10), note(10), 0.35f, 0.14);   // G5 bell
  b[SFX_ROLLOVER].tone(0, 0.5, note(22), note(22), 0.12f, 0.08);
  b[SFX_LAUNCH].tone(0, 0.35, 190, 85, 0.7f, 0.12);
  b[SFX_LAUNCH].hiss(0, 0.08, 500, 5000, 0.6f, 0.015);
  for (int i = 0; i < 5; i++) b[SFX_PULL].hiss(i * 0.045, 0.02, 1500, 7000, 0.35f, 0.004);
  b[SFX_PORTAL_IN].sweep(0, 0.6, 3000, 200, 0.8f);
  b[SFX_PORTAL_IN].tone(0.05, 0.6, 70, 45, 0.6f, 0.25);
  for (int i = 0; i < 6; i++) b[SFX_PORTAL_IN].tone(0.25 + i * 0.06, 0.5, note(15 + (i % 3) * 4 + (i / 3) * 12), note(15 + (i % 3) * 4 + (i / 3) * 12), 0.12f, 0.15);
  b[SFX_PORTAL_OUT].tone(0, 0.15, 150, 70, 0.8f, 0.04);
  b[SFX_PORTAL_OUT].hiss(0, 0.06, 500, 6000, 0.7f, 0.01);
  b[SFX_RAMP].tone(0, 0.35, 280, 880, 0.25f, 0.3, 2);
  b[SFX_RAMP].sweep(0, 0.35, 400, 2400, 0.3f);
  b[SFX_DRAIN].tone(0, 0.8, 330, 110, 0.4f, 0.35, 2);
  b[SFX_DRAIN].tone(0.1, 0.8, 262, 88, 0.3f, 0.35, 2);
  {
    int arp[] = {3, 7, 10, 15, 19, 22};                          // C E G C E G (from A4)
    for (int i = 0; i < 6; i++) b[SFX_MISSION].tone(i * 0.08, 0.4, note(arp[i]), note(arp[i]), 0.3f, 0.15, 2);
    b[SFX_MISSION].tone(0.5, 0.7, note(15), note(15), 0.25f, 0.3, 2);
    b[SFX_MISSION].tone(0.5, 0.7, note(19), note(19), 0.2f, 0.3, 2);
    b[SFX_MISSION].tone(0.5, 0.7, note(22), note(22), 0.2f, 0.3, 2);
  }
  for (int r = 0; r < 2; r++)
    for (int i = 0; i < 4; i++) b[SFX_EXTRA_BALL].tone(r * 0.35 + i * 0.07, 0.3, note(7 + i * 4), note(7 + i * 4), 0.3f, 0.12, 2);
  b[SFX_TILT].tone(0, 0.9, 110, 100, 0.4f, 0.5, 1);
  b[SFX_TILT].tone(0, 0.9, 116, 105, 0.3f, 0.5, 1);
  {
    int ch[] = {3, 7, 10, 15, 19, 22, 27};
    for (int i = 0; i < 7; i++) b[SFX_JACKPOT].tone(i * 0.06, 1.0, note(ch[i]), note(ch[i]), 0.22f, 0.4, 2);
    for (int i = 0; i < 12; i++) b[SFX_JACKPOT].tone(0.45 + i * 0.04, 0.3, note(27 + (i % 4) * 5), note(27 + (i % 4) * 5), 0.08f, 0.1);
    b[SFX_JACKPOT].hiss(0, 0.2, 300, 6000, 0.5f, 0.05);
  }
  b[SFX_SAVE].tone(0, 0.15, note(3), note(3), 0.3f, 0.08, 2);
  b[SFX_SAVE].tone(0.12, 0.3, note(15), note(15), 0.3f, 0.12, 2);
  b[SFX_LOOP].sweep(0, 0.4, 300, 3000, 0.5f);
  b[SFX_LOOP].tone(0.1, 0.3, note(10), note(22), 0.15f, 0.2, 2);
  for (int i = 0; i < 10; i++) b[SFX_BONUS].tone(i * 0.11, 0.08, note(10 + i), note(10 + i), 0.2f, 0.03, 1);
  {
    int dn[] = {10, 6, 3, -2, -5};
    for (int i = 0; i < 5; i++) b[SFX_GAME_OVER].tone(i * 0.22, 0.5, note(dn[i]), note(dn[i]), 0.3f, 0.25, 2);
  }
  {
    int up[] = {-2, 3, 7, 10, 15};
    for (int i = 0; i < 5; i++) b[SFX_START].tone(i * 0.09, 0.4, note(up[i]), note(up[i]), 0.28f, 0.18, 2);
  }
  for (int i = 0; i < 16; i++) b[SFX_MULTIBALL].tone(i * 0.08, 0.2, note(3 + (i % 4) * 4 + (i / 8) * 12), note(3 + (i % 4) * 4 + (i / 8) * 12), 0.22f, 0.07, 1);
  b[SFX_NUDGE].tone(0, 0.12, 70, 45, 0.6f, 0.05);
  b[SFX_NUDGE].hiss(0, 0.05, 100, 800, 0.4f, 0.02);
  b[SFX_WARNING].tone(0, 0.15, 880, 880, 0.3f, 0.2, 1);
  b[SFX_WARNING].tone(0.2, 0.15, 880, 880, 0.3f, 0.2, 1);
  // even out the levels: loud mechanical hits, quieter small noises, tunes in between
  for (int i = 0; i < SFX_COUNT; i++) {
    float target = 0.7f;
    switch (i) {
      case SFX_FLIP_UP: case SFX_BUMPER: case SFX_SLING: case SFX_PORTAL_OUT: target = 0.85f; break;
      case SFX_PULL: case SFX_FLIP_DOWN: target = 0.35f; break;
      case SFX_RUBBER: case SFX_WALL: target = 0.45f; break;
      case SFX_ROLLOVER: case SFX_TARGET: case SFX_WARNING: target = 0.55f; break;
      default: break;
    }
    float pk = 0;
    for (float v : b[i].s) pk = std::max(pk, std::fabs(v));
    if (pk > 0)
      for (float &v : b[i].s) v *= target / pk;
    sfx_[i] = std::move(b[i].s);
  }
}

bool Audio::init() {
  synth();
  if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) return false;
  SDL_AudioSpec spec = {SDL_AUDIO_F32, 2, kRate};
  stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, callback, this);
  if (!stream_) return false;
  SDL_ResumeAudioStreamDevice(stream_);
  return true;
}

void Audio::shutdown() {
  if (stream_) SDL_DestroyAudioStream(stream_);
  stream_ = nullptr;
}

void Audio::play(Sfx s, float vol, float pan) {
  if (!stream_ || muted_ || sfx_[s].empty()) return;
  SDL_LockAudioStream(stream_);
  if (voices_.size() >= 24) voices_.erase(voices_.begin());
  // a little pitch variation so repeated hits don't sound identical
  rng_ = rng_ * 1103515245u + 12345u;
  double rate = 1.0 + (((rng_ >> 16) & 255) / 255.0 - 0.5) * 0.06;
  voices_.push_back({&sfx_[s], 0, rate, std::clamp(vol, 0.f, 1.f), std::clamp(pan, -1.f, 1.f)});
  SDL_UnlockAudioStream(stream_);
}

void Audio::setRolling(float speed) { roll_ = muted_ ? 0 : std::min(1.f, speed / 3.f); }

void SDLCALL Audio::callback(void *ud, SDL_AudioStream *s, int additional, int total) {
  (void)total;
  Audio *a = (Audio *)ud;
  int frames = additional / (int)(2 * sizeof(float));
  if (frames <= 0) return;
  std::vector<float> buf((size_t)frames * 2);
  a->mix(buf.data(), frames);
  SDL_PutAudioStreamData(s, buf.data(), (int)(buf.size() * sizeof(float)));
}

void Audio::mix(float *out, int frames) {
  std::fill(out, out + frames * 2, 0.f);
  for (auto &v : voices_) {
    float gl = v.vol * std::sqrt(0.5f * (1 - v.pan * 0.6f)), gr = v.vol * std::sqrt(0.5f * (1 + v.pan * 0.6f));
    const auto &b = *v.buf;
    for (int i = 0; i < frames; i++) {
      size_t k = (size_t)v.pos;
      if (k + 1 >= b.size()) { v.pos = (double)b.size(); break; }
      float f = (float)(v.pos - k);
      float x = b[k] * (1 - f) + b[k + 1] * f;
      out[2 * i] += x * gl;
      out[2 * i + 1] += x * gr;
      v.pos += v.rate;
    }
  }
  voices_.erase(std::remove_if(voices_.begin(), voices_.end(), [](const Voice &v) { return v.pos >= v.buf->size() - 1; }),
                voices_.end());
  // rolling: low rumble whose loudness follows the ball's speed
  for (int i = 0; i < frames; i++) {
    rollNow_ += (roll_ - rollNow_) * 0.0005f;
    rng_ = rng_ * 1664525u + 1013904223u;
    float n = ((rng_ >> 9) & 0x7fff) / 16384.f - 1.f;
    rollLp_ += 0.02f * (n - rollLp_);
    rollLp2_ += 0.01f * (rollLp_ - rollLp2_);
    float x = (rollLp_ - rollLp2_ * 0.5f) * rollNow_ * 1.4f;
    out[2 * i] += x;
    out[2 * i + 1] += x;
  }
  for (int i = 0; i < frames * 2; i++) out[i] = std::tanh(out[i] * 0.8f);   // soft limit
}

}  // namespace pb

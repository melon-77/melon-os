// Pinball physics in real units: metres, seconds, a 27 mm steel ball on a table tilted 6.5 degrees.
// The table is a 2D plane seen from above: x to the right, y down the table towards the player.
#pragma once
#include <cmath>
#include <vector>

namespace pb {

struct V2 {
  double x = 0, y = 0;
  V2() = default;
  V2(double x_, double y_) : x(x_), y(y_) {}
  V2 operator+(V2 o) const { return {x + o.x, y + o.y}; }
  V2 operator-(V2 o) const { return {x - o.x, y - o.y}; }
  V2 operator*(double s) const { return {x * s, y * s}; }
  V2 operator/(double s) const { return {x / s, y / s}; }
  V2 &operator+=(V2 o) { x += o.x; y += o.y; return *this; }
  V2 &operator-=(V2 o) { x -= o.x; y -= o.y; return *this; }
};
inline double dot(V2 a, V2 b) { return a.x * b.x + a.y * b.y; }
inline double cross(V2 a, V2 b) { return a.x * b.y - a.y * b.x; }
inline double len(V2 a) { return std::sqrt(dot(a, a)); }
inline V2 norm(V2 a) { double l = len(a); return l > 1e-12 ? a / l : V2(0, -1); }
inline V2 perp(V2 a) { return {-a.y, a.x}; }
inline V2 rot(V2 a, double ang) { double c = std::cos(ang), s = std::sin(ang); return {a.x * c - a.y * s, a.x * s + a.y * c}; }

// Constants of a real machine.
constexpr double kPi = 3.14159265358979323846;
constexpr double kBallR = 0.0135;                      // 1 1/16" ball
constexpr double kSlopeDeg = 6.5;                      // playfield pitch
constexpr double kG = 9.81;
// A rolling solid sphere accelerates at 5/7 g sin(pitch) down the slope.
inline double slopeAccel() { return 5.0 / 7.0 * kG * std::sin(kSlopeDeg * kPi / 180.0); }
constexpr double kRollingResistance = 0.002;           // steel ball on a waxed playfield, times g cos(pitch)
constexpr double kMaxSpeed = 9.0;                      // m/s; nothing on a table goes faster
constexpr double kStep = 1.0 / 2000.0;                 // physics step, s

// Surfaces: coefficient of restitution and of friction against a rolling steel ball.
enum Mat { MAT_WALL, MAT_METAL, MAT_RUBBER, MAT_POST, MAT_FLIPPER, MAT_TARGET, MAT_PLUNGER, MAT_COUNT };
struct Material { double e, mu; };
extern const Material kMaterials[MAT_COUNT];

// What a collider does besides bouncing.
enum Kind { K_STATIC, K_BUMPER, K_SLING, K_DROP, K_STANDUP, K_GATE };

struct Seg {                // a wall: capsule of radius r around segment a-b
  V2 a, b;
  double r = 0;
  int mat = MAT_WALL;
  Kind kind = K_STATIC;
  int id = -1;              // element id reported in events (-1: none)
  bool active = true;       // drop targets that are down, gates that are open
  bool oneway = false;      // only blocks balls on the side of `side`, moving against it
  V2 side;
  double height = 0.022;    // how tall it looks (art only)
  int layer = 0;            // 0 playfield, 1 wire ramp
};

struct Circle {             // posts and pop bumpers
  V2 c;
  double r;
  int mat = MAT_POST;
  Kind kind = K_STATIC;
  int id = -1;
  bool active = true;
  double cooldown = 0;      // pop bumper solenoid recharge
};

struct Sensor {             // rollover switch: the ball's centre crossing segment a-b
  V2 a, b;
  int id;
  int layer = 0;
};

struct Hole {               // saucer / kickout hole: a shallow cup that captures slow balls
  V2 c;
  double r;                 // cup radius (the ball feels its slope inside this)
  int id;
  bool enabled = true;
};

struct Flipper {
  V2 pivot;
  double length = 0.076, r0 = 0.0125, r1 = 0.0068;       // 3" bat
  double rest = 0, up = 0;                                // angles (radians, y down = clockwise)
  double angle = 0, omega = 0;
  bool pressed = false;
  // Solenoid: a 50-60 degree stroke in about 30 ms; the return spring is weaker.
  double accelUp = 3200, omegaUp = 38, accelDown = 900, omegaDown = 22;
  V2 tip() const { return pivot + V2(std::cos(angle), std::sin(angle)) * length; }
};

struct Plunger {
  double x0, x1;            // lane walls
  double restY;             // y of the plunger tip at rest
  double pos = 0;           // how far it is pulled back (m, down the table)
  double vel = 0;           // d(pos)/dt
  double maxPull = 0.055;
  bool pulling = false;
  double springW = 36;      // sqrt(k/m): launch speed = pull * springW (about 2 m/s full pull)
  double autoPower = -1;    // >= 0: launch at this fraction without the player (ball save, multiball)
};

struct Ball {
  V2 p, v;
  bool alive = true;
  int captured = -1;        // hole id while held in a saucer
  double spin = 0;          // visual roll angle (texture rotation)
  V2 roll;                  // accumulated rolling distance (for the texture)
  double lift = 0;          // height above the table (m) while on a ramp
  int layer = 0;            // 0 playfield, 1 riding the wire ramp
  double rampS = 0, rampU = 0;  // on the ramp: distance along it and speed along it
};

// A wire ramp. The ball rides it as a bead on a wire: its speed along the centreline follows from energy
// (the table's pitch plus the ramp's own rise, and a little rolling loss on the wires).
struct Ramp {
  std::vector<V2> pts;      // centreline, entrance first
  std::vector<double> h;    // height above the playfield at each point (m)
  std::vector<double> cum;  // arc length at each point
  double mouthHalf = 0.022; // half width of the entrance
  int enterId = -1, exitId = -1;
  void finish();            // computes cum
  double total() const { return cum.empty() ? 0 : cum.back(); }
  void at(double s, V2 &p, V2 &t, double &height, double &dh) const;
};

enum EvType { EV_HIT, EV_SENSOR, EV_HOLE, EV_DRAIN, EV_FLIP, EV_LAUNCH, EV_RAMP_FAIL };
struct Event {
  EvType type;
  int id;                   // element id
  int ball;
  double speed;             // impact speed (normal), m/s
  int dir;                  // sensors: +1 crossed going up the table (towards y-), -1 down
};

class World {
public:
  std::vector<Seg> segs;
  std::vector<Circle> circles;
  std::vector<Sensor> sensors;
  std::vector<Hole> holes;
  std::vector<Flipper> flippers;
  std::vector<Ball> balls;
  std::vector<Ramp> ramps;
  Plunger plunger{};
  bool hasPlunger = false;
  double drainY = 1.10;     // balls below this are gone
  double width = 0.508, length = 1.066;
  V2 nudge;                 // table acceleration from a nudge this step (m/s^2)
  std::vector<Event> events;
  double time = 0;

  void step();              // advance by kStep
  void addBall(V2 p, V2 v = {});
  void eject(int ball, V2 vel);
  int liveBalls() const;

private:
  void moveFlippers();
  void movePlunger();
  void collide(int bi);
  void rideRamp(int bi);
  void contact(Ball &b, int bi, V2 n, double pen, V2 surfVel, int mat, Kind kind, int id, double kick);
};

// Closest point on segment a-b to p.
inline V2 closestOnSeg(V2 p, V2 a, V2 b) {
  V2 d = b - a;
  double t = dot(p - a, d) / std::max(dot(d, d), 1e-18);
  t = t < 0 ? 0 : (t > 1 ? 1 : t);
  return a + d * t;
}

}  // namespace pb

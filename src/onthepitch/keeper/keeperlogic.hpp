// Goalkeeper layer (spec 2026-09-29 §8, #31).
// Pure, headless gameplay maths for the keeper: no Match/rendering dependency, so it can be
// reasoned about (and exercised) on its own. Owned by Match; see keeperlogic.cpp for the bodies.

#ifndef _HPP_KEEPERLOGIC
#define _HPP_KEEPERLOGIC

#include "base/math/vector3.hpp"

#include <vector>

using namespace blunted;

// Host-sim gameplay state of a team's goalkeeper, kept next to Match::ballRetainer.
//   None      - under AI control (the default; the pre-#31 behaviour)
//   Hands     - holding the ball; box-clamped; player switching locked
//   Outfield  - field-player mode: an incoming/held backpass at his feet
//   Returning - lost the ball and is running back towards his goal, under AI
enum e_KeeperState {
  e_KeeperState_None,
  e_KeeperState_Hands,
  e_KeeperState_Outfield,
  e_KeeperState_Returning
};

// Penalty dive zones (spec 2026-09-29 §9, #33). L/R are screen-relative, resolved to world signs by
// the taker's side; three height bands per side (Low / Mid / High). `OffTarget` means "no side zone"
// (the ball misses the goal); it is never picked.
enum e_KeeperDiveZone {
  e_KeeperDiveZone_LowL,
  e_KeeperDiveZone_MidL,
  e_KeeperDiveZone_HighL,
  e_KeeperDiveZone_LowR,
  e_KeeperDiveZone_MidR,
  e_KeeperDiveZone_HighR,
  e_KeeperDiveZone_Center,
  e_KeeperDiveZone_OffTarget
};

namespace keeperlogic {

// Hard-clamp a position to the keeper's own penalty area. `side` is Team::GetSide(); the box
// extends `depth` metres from the goal line and `halfWidth` metres either side of the centre.
Vector3 ClampToBox(const Vector3 &position, signed int side, float depth, float halfWidth);

// Where a ballistic ball lands on the ground plane (z = 0), ignoring spin and drag. Returns the
// ball's current 2D position when it is not falling (or never reaches the ground).
Vector3 PredictLandingPoint(const Vector3 &ballPos, const Vector3 &ballVelocity, float gravity);

// Pick the teammate best matching a stick direction and a charge-scaled distance: `candidates`
// are positions, `origin` the keeper's position, `aimDirection` the stick direction (2D). The
// desired distance lerps from `nearDist` to `farDist` with `charge` (0..1); scoring is the
// direction dot minus the distance-band penalty (spec §8.8 "direction x distance band").
// Returns the index, or -1 when nobody is in front of the keeper.
int SelectDistributionTarget(const std::vector<Vector3> &candidates, const Vector3 &origin,
                             const Vector3 &aimDirection, float nearDist, float farDist,
                             float charge);

// Penalty dive zone from the stick direction (spec §9). `stickX` is screen-right positive, `stickY`
// screen-up positive; a length below `deadzone` reads CENTER, otherwise the vertical band reads
// LOW (down) / MID (neutral) / HIGH (up). L/R are screen-relative.
e_KeeperDiveZone StickToDiveZone(float stickX, float stickY, float deadzone);

// Symmetric weighted AI zone: low / mid / high per side, plus centre. Uses the deterministic
// `random()` (seeded by the caller). Weights live in gamedefines.hpp.
e_KeeperDiveZone RandomDiveZone();

// Screen-relative zone of a ball crossing the defended goal line at (`worldLateral`, `height`).
// `takerSide` converts the world lateral sign to the taker's camera frame (screen-right == world
// +y when takerSide == 1). Three height bands split by `midThreshold` (low/mid) and `highThreshold`
// (mid/high). Off-target crossings read OffTarget.
e_KeeperDiveZone BallFlightZone(float worldLateral, float height, int takerSide,
                                float goalHalfWidth, float goalHeight,
                                float centerBand, float midThreshold, float highThreshold);

// Representative point of `zone` beside the keeper, in world space. `planeX` is the keeper's
// lateral plane; side zones sit at `(planeX, +-takerSide*lateral, lowY/midY/highY)`, CENTER at
// `(planeX, 0, centerY)`.
Vector3 DiveZoneTarget(e_KeeperDiveZone zone, float planeX, int takerSide,
                       float lateral, float lowY, float midY, float highY, float centerY);

} // namespace keeperlogic

#endif

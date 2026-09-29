// Shot launch planning shared by the human and AI controllers (spec 2026-09-29 §2/§7).
// Pure functions: no Match/Player/rendering dependencies, so they can run headless.

#ifndef _HPP_SETPIECELOGIC
#define _HPP_SETPIECELOGIC

#include "../../gamedefines.hpp"

#include "base/math/vector3.hpp"

using namespace blunted;

namespace setpiecelogic {

// A shot charge below this is a short tap: driven flat along the ground.
bool IsGroundShot(float charge);

// Height of the aim point on the attacker's goal plane for a shot charge. Blends from
// _default_Shot_AimYMin to goalHeight + _default_Shot_OverLift; a ground shot uses
// _default_Shot_GroundAimY.
float CalculateAimHeight(float charge);

// Lateral (y) coordinate where the horizontal shot direction crosses the attacker's goal
// line (x = side * -pitchHalfW). Clamped to a margin around the goal.
float CalculateAimLateral(float fromX, float fromY, float directionX, float directionY, int side);

// Ballistic launch velocity from `from` toward `aim`, capped by `maxLift`. A ground shot
// has zero vertical speed; otherwise vz = dz / t + 0.5 * g * t. `fallbackDirection` is used
// when the aim point lies (almost) straight above/below the launch point.
Vector3 CalculateShotVelocity(const Vector3 &from, const Vector3 &aim, const Vector3 &fallbackDirection, float horizontalSpeed, bool ground, float maxLift);

// The planned shot intent; the controllers copy these into TouchInfo.
struct ShotPlan {
  Vector3    desiredDirection;
  float      desiredPower;
  float      aimHeight;
  e_ShotType shotType; // curl/chip plan
};

// Plans a shot from a desired direction and charge. One path for human and AI.
ShotPlan PlanShot(const Vector3 &desiredDirection, float charge, e_ShotType shotType);

// Copies a plan into the shot command's TouchInfo (shared by the human and AI controllers).
void ApplyShotPlan(TouchInfo &touchInfo, const ShotPlan &plan);

}

#endif

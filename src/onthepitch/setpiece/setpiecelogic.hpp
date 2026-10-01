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
  float      curl;     // planned curl out-direction sign (-1/+1); 0 = none
};

// Plans a shot from the ball position and the desired direction/charge. One path for human and
// AI: the plan carries the launch height and the curl direction derived from the aim (never the
// random body-touch sign), so GetShotVector is a pure consumer of TouchInfo.
ShotPlan PlanShot(const Vector3 &from, int side, const Vector3 &desiredDirection, float charge, e_ShotType shotType);

// Copies a plan into the shot command's TouchInfo (shared by the human and AI controllers).
void ApplyShotPlan(TouchInfo &touchInfo, const ShotPlan &plan);

// Set-piece aiming (spec 2026-09-29 §6). A world-space heading is kept as an angle offset from the
// per-type base heading; stick-X turns it around the ball and a per-type arc clamps it. The model
// is pure so the human controller (and, later, the AI taker) can run it headless.
struct SetPieceAim {
  float angle; // signed offset from the base heading (world CCW when seen from above, rad)
};

// Free kick, corner and goal kick use the aim model; throw-in, kickoff and penalty do not.
bool SetPieceAimingUsed(e_SetPiece type);

// Arc the heading may deviate from its base: a full turn on a free kick, a forward sector
// (CORNER_AIM_ARC / GK_AIM_ARC) on a corner or goal kick.
float SetPieceAimArc(e_SetPiece type);

// Start heading from the ball spot: free kick -> goal centre, corner -> the penalty spot,
// goal kick -> straight up the pitch. `side` is the taker's team side.
Vector3 SetPieceBaseHeading(e_SetPiece type, const Vector3 &spot, int side);

// Turns the aim by stick-X this tick. The window is clamped to the per-type arc, so the aim can
// never leave it; released stick input simply stops turning (the aim is never reset).
SetPieceAim RotateSetPieceAim(const SetPieceAim &aim, float stickX, float dt, e_SetPiece type);

// World-space unit heading for the current aim.
Vector3 SetPieceAimHeading(const Vector3 &base, const SetPieceAim &aim);

// Peak height (m) a set-piece pass/cross is lofted to from the vertical stick (-1..1). Stick down
// lifts the ball, stick up drives it lower; a cross is always airborne (never below the waist),
// a short/long pass sits on the ground until the stick is pulled down.
float SetPiecePassHeight(e_FunctionType functionType, float stickY);

// The planned set-piece kick, ready to be copied into the command's TouchInfo.
struct SetPieceKickPlan {
  Vector3    desiredDirection; // world-space horizontal heading (free-kick shot: goal-magnet mix)
  float      desiredPower;     // pass: 0..1 hold ratio; shot: 0..1 charge
  float      aimHeight;        // pass/cross peak height (m)
  bool       useAimHeight;
  float      curl;             // accumulated curl, signed -1..+1
  bool       useLaunch;        // free-kick shot: `launch` is the planned 3D velocity
  Vector3    launch;           // free-kick shot launch velocity (m/s)
};

// Plans the kick from the aimed heading. A free-kick Shot derives speed and elevation from the
// charge and pulls toward the goal centre by _default_SetPiece_GoalMagnet; a pass/cross takes its
// height from stick-Y and its power from the hold. Chip is never used on a set piece (the plan is
// always a normal shot).
SetPieceKickPlan PlanSetPieceKick(e_SetPiece type, const Vector3 &heading, const Vector3 &spot,
                                  int side, e_FunctionType functionType, float charge,
                                  float stickY, float curlAccum);

// Penalty reticle (spec 2026-09-29 §7): a 2D aim point (lateral, height) on the attacker's
// goal plane. Kept separate from PlanShot: the reticle is presentation for the human taker,
// not a shared shot plan.
struct PenaltyAim {
  float lateral; // y offset from the goal centre (m)
  float height;  // above the ground (m)
};

// The reticle's start state: centred, at _default_Pen_ReticleStartY.
PenaltyAim DefaultPenaltyAim();

// Advances the reticle by dt seconds. The stick axes are screen-right/up positive and are
// already deadzone-filtered by the caller: `hasInput` false drifts the reticle back to the
// centre at _default_Pen_ReticleReturn per second, otherwise it moves at _default_Pen_ReticleSpeed.
// The point stays within _default_Pen_AimOverhang past the frame. `side` is the taker's team
// side: the penalty camera looks from the spot toward the attacked goal (x = -side*pitchHalfW),
// so screen-right is world +y when side == 1 and world -y when side == -1 (flips after the
// half-time side swap).
PenaltyAim UpdatePenaltyAim(const PenaltyAim &aim, float stickLateral, float stickHeight, bool hasInput, float dt, int side);

// The launch point actually struck: the frozen reticle plus a random sample from a spread
// disc whose radius grows with the charge, and the charge-scaled launch speed.
struct PenaltyShotPlan {
  float lateral; // y offset from the goal centre (m)
  float height;  // above the ground (m)
  float speed;   // horizontal launch speed (m/s)
};

PenaltyShotPlan PlanPenaltyShot(const PenaltyAim &aim, float charge);

}

#endif

#include "setpiecelogic.hpp"

#include <algorithm>
#include <cmath>

using namespace blunted;

namespace setpiecelogic {

bool IsGroundShot(float charge) {
  return charge < _default_Shot_GroundChargeMax;
}

float CalculateAimHeight(float charge) {
  if (IsGroundShot(charge)) return _default_Shot_GroundAimY;
  float fullAimHeight = goalHeight + _default_Shot_OverLift;
  return _default_Shot_AimYMin +
         (fullAimHeight - _default_Shot_AimYMin) * clamp(charge, 0.0f, 1.0f);
}

float CalculateAimLateral(float fromX, float fromY, float directionX, float directionY, int side) {
  float goalLineX = side * -pitchHalfW;
  float lateral = 0.0f;
  if (fabs(directionX) > 0.001f) {
    float t = (goalLineX - fromX) / directionX;
    if (t > 0.0f) lateral = fromY + directionY * t;
  }
  float margin = goalHalfWidth * _default_Shot_AimLateralMargin;
  return clamp(lateral, -margin, margin);
}

Vector3 CalculateShotVelocity(const Vector3 &from, const Vector3 &aim, const Vector3 &fallbackDirection, float horizontalSpeed, bool ground, float maxLift) {
  Vector3 aimHoriz(aim.coords[0] - from.coords[0], aim.coords[1] - from.coords[1], 0.0f);
  float aimDist = std::max(aimHoriz.GetLength(), 1.0f);
  Vector3 aimDir = aimHoriz.GetNormalized(fallbackDirection.Get2D());
  float speed = std::max(horizontalSpeed, 0.001f);

  if (ground) return aimDir * speed;

  float flightTime = std::max(aimDist / speed, 0.001f);
  float verticalSpeed = (aim.coords[2] - from.coords[2]) / flightTime + 0.5f * _default_Shot_Gravity * flightTime;
  verticalSpeed = clamp(verticalSpeed, 0.0f, maxLift);
  return aimDir * speed + Vector3(0.0f, 0.0f, verticalSpeed);
}

ShotPlan PlanShot(const Vector3 &from, int side, const Vector3 &desiredDirection, float charge, e_ShotType shotType) {
  ShotPlan plan;
  plan.desiredDirection = desiredDirection;
  plan.desiredPower = charge;
  plan.aimHeight = CalculateAimHeight(charge);
  plan.shotType = shotType;
  // Curl bends around into the far corner: aim further outside the crossing point, then spin back
  // in. The out-direction follows the aim (never the random body-touch angle).
  plan.curl = 0.0f;
  if (shotType == e_ShotType_Curl) {
    Vector3 dir2D = desiredDirection.Get2D();
    float aimLateral = CalculateAimLateral(from.coords[0], from.coords[1], dir2D.coords[0], dir2D.coords[1], side);
    float lateral = (fabs(aimLateral) > 0.05f) ? aimLateral : from.coords[1];
    plan.curl = (float)signSide(lateral);
  }
  return plan;
}

void ApplyShotPlan(TouchInfo &touchInfo, const ShotPlan &plan) {
  touchInfo.desiredDirection = plan.desiredDirection;
  touchInfo.desiredPower = plan.desiredPower;
  touchInfo.aimHeight = plan.aimHeight;
  touchInfo.useAimHeight = true;
  touchInfo.shotType = plan.shotType;
  touchInfo.curl = plan.curl;
}

bool SetPieceAimingUsed(e_SetPiece type) {
  return type == e_SetPiece_FreeKick || type == e_SetPiece_Corner || type == e_SetPiece_GoalKick;
}

float SetPieceAimArc(e_SetPiece type) {
  switch (type) {
    case e_SetPiece_FreeKick: return _default_SetPiece_FreeKickAimArc;
    case e_SetPiece_Corner:   return _default_SetPiece_CornerAimArc;
    case e_SetPiece_GoalKick: return _default_SetPiece_GoalKickAimArc;
    default: return 0.0f;
  }
}

Vector3 SetPieceBaseHeading(e_SetPiece type, const Vector3 &spot, int side) {
  Vector3 forward(-side, 0, 0); // straight up the pitch, toward the attacked goal
  Vector3 target;
  switch (type) {
    case e_SetPiece_FreeKick: target = Vector3(-side * pitchHalfW, 0, 0); break;      // goal centre
    case e_SetPiece_Corner:   target = Vector3(-side * (pitchHalfW - 11.0f), 0, 0); break; // penalty spot
    case e_SetPiece_GoalKick: return forward;
    default: return forward;
  }
  Vector3 dir = (target - spot);
  return dir.Get2D().GetNormalized(forward);
}

SetPieceAim RotateSetPieceAim(const SetPieceAim &aim, float stickX, float dt, e_SetPiece type) {
  // Turning right on screen (positive stick-X) rotates the heading clockwise from above, which is
  // a negative world angle in GF's x-y plane; the base heading already carries the team's side.
  float angle = aim.angle - stickX * _default_SetPiece_AimSpeed * dt;
  SetPieceAim result;
  result.angle = clamp(angle, -SetPieceAimArc(type), SetPieceAimArc(type));
  return result;
}

Vector3 SetPieceAimHeading(const Vector3 &base, const SetPieceAim &aim) {
  return base.GetRotated2D(aim.angle).GetNormalized(base);
}

float SetPiecePassHeight(e_FunctionType functionType, float stickY) {
  // A cross (high pass) is lofted even at the neutral stick; a short/long pass is driven along the
  // ground until the player pushes the stick up.
  float neutral = (functionType == e_FunctionType_HighPass) ? _default_SetPiece_PassHeightNeutral : 0.0f;
  float fraction = neutral + clamp(stickY, -1.0f, 1.0f) * _default_SetPiece_PassHeightSpan;
  return clamp(fraction, 0.0f, 1.0f) * _default_SetPiece_PassHeightMax;
}

SetPieceKickPlan PlanSetPieceKick(e_SetPiece type, const Vector3 &heading, const Vector3 &spot,
                                  int side, e_FunctionType functionType, float charge,
                                  float stickY, float curlAccum) {
  SetPieceKickPlan plan;
  float hold = clamp(charge, 0.0f, 1.0f);
  plan.desiredDirection = heading;
  plan.desiredPower = hold;
  plan.aimHeight = 0.0f;
  plan.useAimHeight = false;
  plan.useLaunch = false;
  plan.launch = Vector3(0);
  plan.curl = clamp(curlAccum * _default_SetPiece_CurlScale, -_default_SetPiece_CurlMax, _default_SetPiece_CurlMax);

  if (functionType == e_FunctionType_Shot && type == e_SetPiece_FreeKick) {
    // The charge drives two axes at once: launch speed and elevation.
    Vector3 toGoal = (Vector3(-side * pitchHalfW, 0, 0) - spot).Get2D().GetNormalized(heading.Get2D());
    Vector3 dir = (heading.Get2D() * (1.0f - _default_SetPiece_GoalMagnet) + toGoal * _default_SetPiece_GoalMagnet).GetNormalized(heading.Get2D());
    float speed = _default_SetPiece_FreeKickShotSpeedMin + (_default_SetPiece_FreeKickShotSpeedMax - _default_SetPiece_FreeKickShotSpeedMin) * hold;
    float elevation = _default_SetPiece_FreeKickShotElevMin + (_default_SetPiece_FreeKickShotElevMax - _default_SetPiece_FreeKickShotElevMin) * hold;
    plan.desiredDirection = dir;
    plan.launch = dir * (speed * std::cos(elevation)) + Vector3(0.0f, 0.0f, speed * std::sin(elevation));
    plan.useLaunch = true;
  } else {
    plan.aimHeight = SetPiecePassHeight(functionType, stickY);
    plan.useAimHeight = true;
  }
  return plan;
}

PenaltyAim DefaultPenaltyAim() {
  return PenaltyAim{ 0.0f, _default_Pen_ReticleStartY };
}

PenaltyAim UpdatePenaltyAim(const PenaltyAim &aim, float stickLateral, float stickHeight, bool hasInput, float dt, int side) {
  PenaltyAim result = aim;
  if (hasInput) {
    // The stick points at the screen: map it to the goal plane through the camera's heading, whose
    // right axis is world +y for side == 1 and -y for side == -1.
    result.lateral += stickLateral * side * _default_Pen_ReticleSpeed * dt;
    result.height += stickHeight * _default_Pen_ReticleSpeed * dt;
  } else {
    float returnFactor = clamp(_default_Pen_ReticleReturn * dt, 0.0f, 1.0f);
    result.lateral += (0.0f - result.lateral) * returnFactor;
    result.height += (_default_Pen_ReticleStartY - result.height) * returnFactor;
  }
  result.lateral = clamp(result.lateral, -(goalHalfWidth + _default_Pen_AimOverhang), goalHalfWidth + _default_Pen_AimOverhang);
  result.height = clamp(result.height, 0.0f, goalHeight + _default_Pen_AimOverhang);
  return result;
}

PenaltyShotPlan PlanPenaltyShot(const PenaltyAim &aim, float charge) {
  float ratio = clamp(charge, 0.0f, 1.0f);
  float spread = _default_Pen_SpreadMinR + (_default_Pen_SpreadMaxR - _default_Pen_SpreadMinR) * ratio;
  float angle = random(0.0f, 2.0f * pi);
  float radius = spread * std::sqrt(random(0.0f, 1.0f));
  PenaltyShotPlan plan;
  plan.lateral = aim.lateral + std::cos(angle) * radius;
  plan.height = clamp(aim.height + std::sin(angle) * radius, 0.0f, goalHeight + _default_Pen_AimOverhang);
  plan.speed = _default_Pen_PowerMinSpeed + (_default_Pen_PowerMaxSpeed - _default_Pen_PowerMinSpeed) * ratio;
  return plan;
}

}

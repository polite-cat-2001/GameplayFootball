#include "setpiecelogic.hpp"

#include <algorithm>

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

ShotPlan PlanShot(const Vector3 &desiredDirection, float charge, e_ShotType shotType) {
  ShotPlan plan;
  plan.desiredDirection = desiredDirection;
  plan.desiredPower = charge;
  plan.aimHeight = CalculateAimHeight(charge);
  plan.shotType = shotType;
  return plan;
}

void ApplyShotPlan(TouchInfo &touchInfo, const ShotPlan &plan) {
  touchInfo.desiredDirection = plan.desiredDirection;
  touchInfo.desiredPower = plan.desiredPower;
  touchInfo.aimHeight = plan.aimHeight;
  touchInfo.useAimHeight = true;
  touchInfo.shotType = plan.shotType;
}

}

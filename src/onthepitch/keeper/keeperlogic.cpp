// Goalkeeper layer (spec 2026-09-29 §8, #31). Pure gameplay maths; see keeperlogic.hpp.

#include "keeperlogic.hpp"

#include <algorithm>
#include <cmath>

#include "base/math/bluntmath.hpp"
#include "../../gamedefines.hpp"

namespace keeperlogic {

Vector3 ClampToBox(const Vector3 &position, signed int side, float depth, float halfWidth) {
  Vector3 result = position;
  // Own goal line sits at x = side * pitchHalfW; the box reaches depth metres out in front.
  result.coords[0] = clamp(result.coords[0] * side, pitchHalfW - depth, pitchHalfW) * side;
  result.coords[1] = clamp(result.coords[1], -halfWidth, halfWidth);
  return result;
}

Vector3 PredictLandingPoint(const Vector3 &ballPos, const Vector3 &ballVelocity, float gravity) {
  if (gravity <= 0.0f) return ballPos.Get2D();

  const float z = ballPos.coords[2];
  const float vz = ballVelocity.coords[2];
  if (vz >= 0.0f && z <= 0.0f) return ballPos.Get2D(); // already on the ground and not lifted

  // z + vz*t - (gravity/2)*t^2 = 0  ->  t = (vz + sqrt(vz^2 + 2*gravity*z)) / gravity
  const float discriminant = vz * vz + 2.0f * gravity * z;
  if (discriminant < 0.0f) return ballPos.Get2D(); // never reaches the ground
  const float t = (vz + std::sqrt(discriminant)) / gravity;
  if (t < 0.0f) return ballPos.Get2D();

  Vector3 result = ballPos + ballVelocity * t;
  result.coords[2] = 0.0f;
  return result;
}

int SelectDistributionTarget(const std::vector<Vector3> &candidates, const Vector3 &origin,
                             const Vector3 &aimDirection, float nearDist, float farDist,
                             float charge) {
  const Vector3 aim = aimDirection.Get2D().GetNormalized(Vector3(0, -1, 0));
  const float wantDist = nearDist + (farDist - nearDist) * clamp(charge, 0.0f, 1.0f);
  const float halfBand = std::max((farDist - nearDist) * 0.5f, 0.01f);

  int best = -1;
  float bestScore = -1.0f;
  for (int i = 0; i < (signed int)candidates.size(); i++) {
    Vector3 to = candidates.at(i) - origin;
    to.coords[2] = 0.0f;
    const float dist = to.GetLength();
    if (dist < std::max(wantDist - halfBand, 0.0f) || dist > wantDist + halfBand) continue;

    const float directionFit = to.GetNormalized(aim).GetDotProduct(aim); // 1 == straight ahead
    if (directionFit <= 0.0f) continue; // behind the keeper: never a distribution target
    const float distanceFit = 1.0f - std::fabs(dist - wantDist) / halfBand;
    const float score = directionFit * distanceFit; // spec §8.8: direction x distance band

    if (score > bestScore) {
      bestScore = score;
      best = i;
    }
  }
  return best;
}

e_KeeperDiveZone StickToDiveZone(float stickX, float stickY, float deadzone) {
  if (std::sqrt(stickX * stickX + stickY * stickY) < deadzone) return e_KeeperDiveZone_Center;
  const bool right = stickX >= 0.0f;
  if (right) return stickY > deadzone ? e_KeeperDiveZone_HighR : (stickY < -deadzone ? e_KeeperDiveZone_LowR : e_KeeperDiveZone_MidR);
  return stickY > deadzone ? e_KeeperDiveZone_HighL : (stickY < -deadzone ? e_KeeperDiveZone_LowL : e_KeeperDiveZone_MidL);
}

e_KeeperDiveZone RandomDiveZone() {
  const float r = random(0.0f, 1.0f);
  float acc = 0.0f;
  acc += penaltyKeeperZoneWeightLow;
  if (r < acc) return e_KeeperDiveZone_LowL;
  acc += penaltyKeeperZoneWeightMid;
  if (r < acc) return e_KeeperDiveZone_MidL;
  acc += penaltyKeeperZoneWeightHigh;
  if (r < acc) return e_KeeperDiveZone_HighL;
  acc += penaltyKeeperZoneWeightLow;
  if (r < acc) return e_KeeperDiveZone_LowR;
  acc += penaltyKeeperZoneWeightMid;
  if (r < acc) return e_KeeperDiveZone_MidR;
  acc += penaltyKeeperZoneWeightHigh;
  if (r < acc) return e_KeeperDiveZone_HighR;
  acc += penaltyKeeperZoneWeightCenter;
  if (r < acc) return e_KeeperDiveZone_Center;
  return e_KeeperDiveZone_Center; // float remainder (weights sum to 1)
}

e_KeeperDiveZone BallFlightZone(float worldLateral, float height, int takerSide,
                                float goalHalfWidth, float goalHeight,
                                float centerBand, float midThreshold, float highThreshold) {
  if (std::fabs(worldLateral) > goalHalfWidth || height < 0.0f || height > goalHeight) {
    return e_KeeperDiveZone_OffTarget;
  }
  if (std::fabs(worldLateral) <= centerBand) return e_KeeperDiveZone_Center;
  const bool right = (worldLateral * takerSide) > 0.0f;
  const bool high = height > highThreshold;
  const bool mid = !high && height > midThreshold;
  if (right) return high ? e_KeeperDiveZone_HighR : (mid ? e_KeeperDiveZone_MidR : e_KeeperDiveZone_LowR);
  return high ? e_KeeperDiveZone_HighL : (mid ? e_KeeperDiveZone_MidL : e_KeeperDiveZone_LowL);
}

Vector3 DiveZoneTarget(e_KeeperDiveZone zone, float planeX, int takerSide,
                       float lateral, float lowY, float midY, float highY, float centerY) {
  float worldY = 0.0f;
  float height = centerY;
  switch (zone) {
    case e_KeeperDiveZone_LowL: worldY = -takerSide * lateral; height = lowY; break;
    case e_KeeperDiveZone_MidL: worldY = -takerSide * lateral; height = midY; break;
    case e_KeeperDiveZone_HighL: worldY = -takerSide * lateral; height = highY; break;
    case e_KeeperDiveZone_LowR: worldY = takerSide * lateral; height = lowY; break;
    case e_KeeperDiveZone_MidR: worldY = takerSide * lateral; height = midY; break;
    case e_KeeperDiveZone_HighR: worldY = takerSide * lateral; height = highY; break;
    case e_KeeperDiveZone_Center:
    case e_KeeperDiveZone_OffTarget:
    default: worldY = 0.0f; height = centerY; break;
  }
  return Vector3(planeX, worldY, height);
}

} // namespace keeperlogic

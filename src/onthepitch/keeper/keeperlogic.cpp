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
                             const Vector3 &aimDirection, float minDist, float maxDist) {
  const Vector3 aim = aimDirection.Get2D().GetNormalized(Vector3(0, -1, 0));
  const float midDist = (minDist + maxDist) * 0.5f;
  const float halfBand = std::max((maxDist - minDist) * 0.5f, 0.001f);

  int best = -1;
  float bestScore = -1.0f;
  for (int i = 0; i < (signed int)candidates.size(); i++) {
    Vector3 to = candidates.at(i) - origin;
    to.coords[2] = 0.0f;
    const float dist = to.GetLength();
    if (dist < minDist || dist > maxDist) continue;

    const float directionFit = to.GetNormalized(aim).GetDotProduct(aim); // 1 == straight ahead
    if (directionFit <= 0.0f) continue; // behind the keeper: never a distribution target
    const float distanceFit = 1.0f - std::fabs(dist - midDist) / halfBand;
    const float score = directionFit * distanceFit;

    if (score > bestScore) {
      bestScore = score;
      best = i;
    }
  }
  return best;
}

} // namespace keeperlogic

#include "onthepitch/setpiece/setpiecelogic.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace setpiecelogic;

namespace {

bool Near(float actual, float expected) {
  return std::fabs(actual - expected) < 0.001f;
}

void Check(bool condition, const char *message) {
  if (!condition) {
    std::fprintf(stderr, "setpiecelogictest failed: %s\n", message);
    std::abort();
  }
}

void SetPieceTakerAimMirrorsStrongFootAndCurrentSide() {
  PenaltyAim rightFootFirstChoice = PlanAIPenaltyAim(false, 1, 0.30f, 0.10f);
  PenaltyAim rightFootSwappedSide = PlanAIPenaltyAim(false, -1, 0.30f, 0.10f);
  PenaltyAim leftFootFirstChoice = PlanAIPenaltyAim(true, 1, 0.30f, 0.10f);
  PenaltyAim leftFootSwappedSide = PlanAIPenaltyAim(true, -1, 0.30f, 0.10f);

  Check(rightFootFirstChoice.lateral < 0.0f, "right-footed taker prefers kicker-left");
  Check(rightFootSwappedSide.lateral > 0.0f, "right-footed target mirrors after halftime");
  Check(leftFootFirstChoice.lateral > 0.0f, "left-footed taker prefers kicker-right");
  Check(leftFootSwappedSide.lateral < 0.0f, "left-footed target mirrors after halftime");
  Check(Near(rightFootFirstChoice.height, rightFootSwappedSide.height), "right-footed height is side invariant");
  Check(Near(leftFootFirstChoice.height, leftFootSwappedSide.height), "left-footed height is side invariant");

  PenaltyAim center = PlanAIPenaltyAim(true, -1, 0.10f, 0.90f);
  Check(Near(center.lateral, 0.0f), "center penalty aim stays on the goal centreline");
  Check(Near(center.height, 1.25f), "the central target sometimes uses middle height");
  Check(Near(PlanAIPenaltyAim(true, 1, 0.10f, 0.50f).height, 0.30f),
        "the central target is more often low");
  Check(Near(PlanAIPenaltyAim(true, 1, 0.10f, 0.98f).height, 1.75f),
        "the central target can rarely be high");
}

void SetPieceTakerPowerFavorsThreeQuarterStrength() {
  Check(Near(PlanAIPenaltyPower(0.50f), 0.75f), "three-quarter power is most frequent");
  Check(Near(PlanAIPenaltyPower(0.75f), 1.0f), "maximum power is occasional");
  Check(Near(PlanAIPenaltyPower(0.95f), 0.5f), "half power remains possible");
}

void PenaltySpreadCanBePlannedFromExplicitRolls() {
  PenaltyShotPlan plan = PlanPenaltyShot(PenaltyAim{ 0.0f, 0.0f }, 0.75f, 0.0f, 0.25f);
  Check(Near(plan.lateral, 0.61875f), "the explicit spread rolls produce a stable aim point");
  Check(Near(plan.height, 0.0f), "zero angle keeps the sample on the lateral axis");
  Check(Near(plan.speed, 39.0f), "penalty speed follows the supplied charge");
}

void AIPenaltySpreadStaysInsideTheGoalFrame() {
  PenaltyAim highSideAim{ _default_AI_PenaltyAimLateral, _default_AI_PenaltyHighAimY };
  PenaltyShotPlan lateralExtreme = PlanAIPenaltyShot(highSideAim, 1.0f, 0.0f, 1.0f);
  PenaltyShotPlan heightExtreme = PlanAIPenaltyShot(highSideAim, 1.0f, 0.25f, 1.0f);

  Check(lateralExtreme.lateral < goalHalfWidth - 0.11f,
        "the maximum AI spread keeps the ball inside the post");
  Check(heightExtreme.height < goalHeight - 0.11f,
        "the maximum AI spread keeps the ball below the crossbar");
}

}

int main() {
  SetPieceTakerAimMirrorsStrongFootAndCurrentSide();
  SetPieceTakerPowerFavorsThreeQuarterStrength();
  PenaltySpreadCanBePlannedFromExplicitRolls();
  AIPenaltySpreadStaysInsideTheGoalFrame();
}

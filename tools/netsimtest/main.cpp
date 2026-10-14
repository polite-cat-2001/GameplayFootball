#include "onthepitch/net/netsim.hpp"

#include "gamedefines.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace blunted;

namespace {

void Check(bool condition, const char *message) {
  if (!condition) {
    std::fprintf(stderr, "netsimtest failed: %s\n", message);
    std::abort();
  }
}

// The module never reads game constants, so the harness wires the NET_* values here.
netsim::Params DefaultParams() {
  netsim::Params p;
  p.gravity = NET_GRAVITY;
  p.damping = NET_DAMPING;
  p.stiffness = NET_SPRING_STIFFNESS;
  p.shapeReturn = NET_SHAPE_RETURN;
  p.ballRadius = NET_BALL_RADIUS;
  p.ballVelScale = NET_BALL_VEL_SCALE;
  p.ballForce = NET_BALL_FORCE;
  p.ballMinSpeed = NET_BALL_MIN_SPEED;
  p.constraintIterations = NET_CONSTRAINT_ITERATIONS;
  p.constraintStiffness = NET_STIFFNESS;
  return p;
}

netsim::Net BuildDefaultNet() {
  return netsim::BuildBoxNet(goalHalfWidth * 2.0f, goalHeight, NET_DEPTH,
                             NET_WIDTH_DIV, NET_HEIGHT_DIV, NET_DEPTH_DIV, NET_SLACK);
}

void BoxNetCountsMatchTheReference() {
  netsim::Net net = BuildDefaultNet();
  Check(net.pos.size() == 1274, "expected 1274 nodes");
  Check(net.edges.size() == 2398 * 2, "expected 2398 edges (two indices each)");
  int pins = 0;
  for (unsigned char pinned : net.pinned) pins += pinned ? 1 : 0;
  Check(pins == 284, "expected 284 pinned nodes");
}

void PinnedOuterRingNeverLeavesTheFrame() {
  netsim::Net net = BuildDefaultNet();
  const netsim::Params p = DefaultParams();
  for (int step = 0; step < 60; ++step) {
    netsim::Integrate(net, Vector3(0.0f, 1.2f, 1.0f), 30.0f, p, NET_SIM_STEP);
  }
  for (std::size_t i = 0; i < net.pos.size(); ++i) {
    if (!net.pinned[i]) continue;
    Check(net.pos[i] == net.rest[i], "pinned node drifted off the frame");
  }
}

void SimulationIsReproducibleAtFixedDt() {
  netsim::Net a = BuildDefaultNet();
  netsim::Net b = BuildDefaultNet();
  const netsim::Params p = DefaultParams();
  for (int step = 0; step < 90; ++step) {
    const float t = static_cast<float>(step) * NET_SIM_STEP;
    const Vector3 ball(std::sin(t) * 2.0f, 0.6f + 0.5f * std::cos(t), 1.5f);
    const float speed = 12.0f + 8.0f * std::cos(t);
    netsim::Integrate(a, ball, speed, p, NET_SIM_STEP);
    netsim::Integrate(b, ball, speed, p, NET_SIM_STEP);
  }
  for (std::size_t i = 0; i < a.pos.size(); ++i) {
    Check(a.pos[i] == b.pos[i], "two identical runs diverged");
  }
}

void FastBallPushesTheNetButAVelocityGatedBallDoesNot() {
  netsim::Net gated = BuildDefaultNet();
  netsim::Net fast = BuildDefaultNet();
  const netsim::Params p = DefaultParams();
  const Vector3 ball(0.0f, 1.2f, NET_DEPTH - 0.1f);
  for (int step = 0; step < 20; ++step) {
    netsim::Integrate(gated, ball, NET_BALL_MIN_SPEED, p, NET_SIM_STEP);
    netsim::Integrate(fast, ball, 30.0f, p, NET_SIM_STEP);
  }
  // The back panel is built first: 25 columns x 13 rows; node (12, 6) sits under the ball.
  const std::size_t centre = 6 * (NET_WIDTH_DIV + 1) + (NET_WIDTH_DIV / 2);
  const float pushed = (fast.pos[centre] - gated.pos[centre]).GetLength();
  Check(pushed > 0.05f, "a fast ball should push the net; a velocity-gated ball should not");
  Check(netsim::MaxDisplacement(gated) < netsim::MaxDisplacement(fast),
        "the fast ball must displace the net further than the velocity-gated one");
}

} // namespace

int main() {
  BoxNetCountsMatchTheReference();
  PinnedOuterRingNeverLeavesTheFrame();
  SimulationIsReproducibleAtFixedDt();
  FastBallPushesTheNetButAVelocityGatedBallDoesNot();
}

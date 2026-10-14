// Goal-net soft body (NetSim, spec 2026-09-29 §13/#37).
// A 1:1 C++ port of the open_football `net_sim.gd` model (via the HTML prototype): a procedural
// box of four independent panels (back / top / left / right; bottom open). Each panel's outer ring
// is pinned to the frame, `slack` shortens the rest edge lengths, and a step is Verlet integration
// plus soft position-based constraints.
//
// Pure maths: this module never reads game constants, it takes everything as parameters, so it can
// run (and be tested) headless. Presentation-only: the ball drives the net one way, nothing feeds
// back into the simulation. Owned by whoever owns the rendered net; see netsim.cpp for the bodies.

#ifndef _HPP_NETSIM
#define _HPP_NETSIM

#include "base/math/vector3.hpp"

#include <vector>

using namespace blunted;

namespace netsim {

// Tuning of one step; the values live in gamedefines.hpp as NET_* and are supplied by the caller.
struct Params {
  float gravity;             // NET_GRAVITY
  float damping;             // NET_DAMPING
  float stiffness;           // NET_SPRING_STIFFNESS (legacy edge spring)
  float shapeReturn;         // NET_SHAPE_RETURN
  float ballRadius;          // NET_BALL_RADIUS (influence radius, not the ball's own radius)
  float ballVelScale;        // NET_BALL_VEL_SCALE
  float ballForce;           // NET_BALL_FORCE
  float ballMinSpeed;        // NET_BALL_MIN_SPEED (velocity gate)
  int constraintIterations;  // NET_CONSTRAINT_ITERATIONS
  float constraintStiffness; // NET_STIFFNESS (PBD stiffness, the main knob)
};

// A procedurally built goal-net mesh. `edges` holds index pairs; `restLen[k]` is the rest length
// of edge k (already scaled by slack). `pinned[i]` is 1 for nodes on a panel's outer ring.
struct Net {
  std::vector<Vector3> pos;
  std::vector<Vector3> rest;
  std::vector<Vector3> prev;
  std::vector<Vector3> normal;
  std::vector<unsigned char> pinned;
  std::vector<int> edges;
  std::vector<float> restLen;
};

// Builds the four-panel net. `width`/`height`/`depth` are the box dimensions; `wDiv`/`hDiv`/`dDiv`
// are the cell counts across width / height / depth. `slack` (< 1) shortens every rest edge length
// so the cloth has spare material and sags/wobbles; rest positions stay taut (the idle look).
Net BuildBoxNet(float width, float height, float depth, int wDiv, int hDiv, int dDiv, float slack);

// Advances the net by one step. `ballPos`/`ballSpeed` are the ball's current position and speed;
// the ball pushes nearby nodes along their panel normals (velocity-gated, no feedback). Mutates
// `net.pos` / `net.prev`.
void Integrate(Net &net, const Vector3 &ballPos, float ballSpeed, const Params &params, float dt);

// Largest |len - restLen| / restLen over the edges (a wobble/tension readout).
float MaxStrain(const Net &net);

// Largest distance of any node from its rest position.
float MaxDisplacement(const Net &net);

// Snaps every node back to its rest position (and clears momentum).
void ResetToRest(Net &net);

} // namespace netsim

#endif

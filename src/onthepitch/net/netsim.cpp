// Goal-net soft body (NetSim, spec 2026-09-29 §13/#37). See netsim.hpp.

#include "netsim.hpp"

#include <algorithm>
#include <cmath>
#include <functional>

namespace netsim {

namespace {

float Lerp(float a, float b, float t) {
  return a + (b - a) * t;
}

// Matches the prototype's clamped Hermite smoothstep; used for the ball's influence falloff.
float SmoothStep(float edge0, float edge1, float x) {
  float t = (x - edge0) / (edge1 - edge0);
  t = std::max(0.0f, std::min(1.0f, t));
  return t * t * (3.0f - 2.0f * t);
}

float Length(const Vector3 &v) {
  return std::sqrt(v.GetSquaredLength());
}

void AddEdge(Net &net, int a, int b) {
  net.edges.push_back(a);
  net.edges.push_back(b);
  net.restLen.push_back(Length(net.pos[a] - net.pos[b]));
}

// Adds one uDiv x vDiv cell grid. `point(su, sv)` gives a node from normalised coords in [0, 1];
// nodes on the grid border are pinned to the frame.
void AddPanel(Net &net, int uDiv, int vDiv, const Vector3 &normal,
              const std::function<Vector3(float, float)> &point) {
  const int base = static_cast<int>(net.pos.size());
  const int cols = uDiv + 1;
  const int rows = vDiv + 1;
  for (int j = 0; j < rows; ++j) {
    for (int i = 0; i < cols; ++i) {
      const Vector3 p = point(static_cast<float>(i) / static_cast<float>(uDiv),
                              static_cast<float>(j) / static_cast<float>(vDiv));
      net.pos.push_back(p);
      net.rest.push_back(p);
      net.prev.push_back(p);
      net.normal.push_back(normal);
      const bool onEdge = (i == 0 || i == uDiv || j == 0 || j == vDiv);
      net.pinned.push_back(onEdge ? 1 : 0);
    }
  }
  // Structural springs to the right and upper neighbours.
  for (int j = 0; j < rows; ++j) {
    for (int i = 0; i < cols; ++i) {
      const int idx = base + j * cols + i;
      if (i < cols - 1) AddEdge(net, idx, idx + 1);
      if (j < rows - 1) AddEdge(net, idx, idx + cols);
    }
  }
}

} // namespace

Net BuildBoxNet(float width, float height, float depth, int wDiv, int hDiv, int dDiv, float slack) {
  Net net;
  const float hw = width * 0.5f;
  // Back: plane z=depth, u=x across width, v=y up, normal +z.
  AddPanel(net, wDiv, hDiv, Vector3(0, 0, 1), [hw, height, depth](float su, float sv) {
    return Vector3(Lerp(-hw, hw, su), Lerp(0.0f, height, sv), depth);
  });
  // Top: plane y=height, u=x across width, v=z across depth, normal +y.
  AddPanel(net, wDiv, dDiv, Vector3(0, 1, 0), [hw, height, depth](float su, float sv) {
    return Vector3(Lerp(-hw, hw, su), height, Lerp(0.0f, depth, sv));
  });
  // Left: plane x=-hw, u=z across depth, v=y up, normal -x.
  AddPanel(net, dDiv, hDiv, Vector3(-1, 0, 0), [hw, height, depth](float su, float sv) {
    return Vector3(-hw, Lerp(0.0f, height, sv), Lerp(0.0f, depth, su));
  });
  // Right: plane x=hw, u=z across depth, v=y up, normal +x.
  AddPanel(net, dDiv, hDiv, Vector3(1, 0, 0), [hw, height, depth](float su, float sv) {
    return Vector3(hw, Lerp(0.0f, height, sv), Lerp(0.0f, depth, su));
  });
  // Slack shortens the rest lengths only; rest positions stay taut so the idle net looks flat.
  if (slack != 1.0f) {
    for (float &restLen : net.restLen) restLen *= slack;
  }
  return net;
}

void Integrate(Net &net, const Vector3 &ballPos, const Vector3 &ballVelocity, const Params &p, float dt) {
  const int n = static_cast<int>(net.pos.size());
  const int e = static_cast<int>(net.edges.size()) / 2;

  std::vector<Vector3> force(n, Vector3(0));

  // Per-node forces: gravity, pull back to the rest shape, and the ball impulse.
  for (int i = 0; i < n; ++i) {
    if (net.pinned[i]) continue;
    Vector3 f(0.0f, -p.gravity, 0.0f);
    f += (net.rest[i] - net.pos[i]) * p.shapeReturn;
    // The ball presses a panel only while it approaches it: its normal velocity points at the
    // panel from whichever side the ball is on (vn and signedDist have opposite signs). The push
    // is an impulse scaled by the normal speed and capped, so grazing/through balls barely move
    // the net while a head-on shot punches a proportionate pocket.
    const Vector3 rel = ballPos - net.pos[i];
    const float vn = ballVelocity.GetDotProduct(net.normal[i]);
    const float signedDist = rel.GetDotProduct(net.normal[i]);
    const float distance = Length(rel);
    const float prox = 1.0f - SmoothStep(p.ballRadius, p.ballRadius * 1.15f, distance);
    if (prox > 0.0f && vn * signedDist < 0.0f && std::fabs(vn) > p.ballMinSpeed) {
      const float impact = std::min(std::fabs(vn), p.ballMaxImpact);
      const float dir = (vn > 0.0f) ? 1.0f : -1.0f;
      f += net.normal[i] * (dir * prox * impact * p.ballVelScale * p.ballForce);
    }
    force[i] = f;
  }

  // Edge springs (symmetric onto both ends).
  for (int k = 0; k < e; ++k) {
    const int a = net.edges[k * 2];
    const int b = net.edges[k * 2 + 1];
    const Vector3 sv = net.pos[a] - net.pos[b];
    const float length = Length(sv);
    if (length > 0.00001f) {
      const Vector3 sf = -sv * (p.stiffness * (1.0f - net.restLen[k] / length));
      if (!net.pinned[a]) force[a] += sf;
      if (!net.pinned[b]) force[b] -= sf;
    }
  }

  // Semi-implicit Verlet: pos += (pos - prev) * damping + force * dt^2.
  const float dt2 = dt * dt;
  for (int i = 0; i < n; ++i) {
    if (net.pinned[i]) {
      net.prev[i] = net.pos[i];
      continue;
    }
    const Vector3 temp = net.pos[i];
    net.pos[i] = net.pos[i] + (net.pos[i] - net.prev[i]) * p.damping + force[i] * dt2;
    net.prev[i] = temp;
  }

  // Position-based constraints (Jacobsen): relax every edge back to its rest length a few times so
  // the cloth behaves like a dense fabric (cells do not stretch) yet can wobble and sag. Only pos
  // is corrected, not prev, so the correction is implicitly damped out of the Verlet velocity.
  for (int iteration = 0; iteration < p.constraintIterations; ++iteration) {
    for (int k = 0; k < e; ++k) {
      const int a = net.edges[k * 2];
      const int b = net.edges[k * 2 + 1];
      const bool aPinned = net.pinned[a] == 1;
      const bool bPinned = net.pinned[b] == 1;
      if (aPinned && bPinned) continue;
      const Vector3 cd = net.pos[a] - net.pos[b];
      const float distance = Length(cd);
      if (distance <= 0.00001f) continue;
      const Vector3 corr = cd * (((distance - net.restLen[k]) / distance) * p.constraintStiffness);
      if (aPinned) {
        net.pos[b] = net.pos[b] + corr;
      } else if (bPinned) {
        net.pos[a] = net.pos[a] - corr;
      } else {
        net.pos[a] = net.pos[a] - corr * 0.5f;
        net.pos[b] = net.pos[b] + corr * 0.5f;
      }
    }
  }
}

float MaxStrain(const Net &net) {
  float result = 0.0f;
  const int e = static_cast<int>(net.edges.size()) / 2;
  for (int k = 0; k < e; ++k) {
    const int a = net.edges[k * 2];
    const int b = net.edges[k * 2 + 1];
    const float length = Length(net.pos[a] - net.pos[b]);
    const float strain = std::fabs((length - net.restLen[k]) / net.restLen[k]);
    if (strain > result) result = strain;
  }
  return result;
}

float MaxDisplacement(const Net &net) {
  float result = 0.0f;
  const int n = static_cast<int>(net.pos.size());
  for (int i = 0; i < n; ++i) {
    const float distance = Length(net.pos[i] - net.rest[i]);
    if (distance > result) result = distance;
  }
  return result;
}

void ResetToRest(Net &net) {
  net.pos = net.rest;
  net.prev = net.rest;
}

} // namespace netsim

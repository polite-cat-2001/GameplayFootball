#ifndef _HPP_MATCHSNAPSHOT
#define _HPP_MATCHSNAPSHOT

#include <vector>

#include "base/math/vector3.hpp"
#include "base/math/quaternion.hpp"

#include "netbuffer.hpp"

class Match;

namespace blunted { class Animation; }

// Compact per-frame render state of a single humanoid. The client only needs to
// reproduce the visual pose, so an animation id (index into the host table) plus
// the current frame and world transform is enough.
// Identified by team + slot rather than a global PlayerBase id: both processes
// build the same squads in the same order, while the id counter is process-global
// and may be offset (e.g. after a local match). team 0/1 = Home/Away squad slot;
// team 2 = officials (referee, linesman north, linesman south).
struct SnapshotPlayer {
  SnapshotPlayer() : team(-1), slot(-1), ownerId(-1), animID(-1), frameNum(0), noPos(false), orientation(0) {}

  int team;
  int slot;
  int ownerId; // peer controlling this player (-1 = AI)
  int animID;
  int frameNum;
  bool noPos;
  blunted::Vector3 position;
  float orientation;
};

// Host -> client snapshot: everything the thin client needs to present a frame
// without running the simulation. Scores/time/phase form a small header.
struct Snapshot {
  Snapshot() {
    matchTime_ms = 0;
    actualTime_ms = 0;
    score[0] = 0;
    score[1] = 0;
    matchPhase = 0;
    inPlay = false;
    inSetPiece = false;
    pause = false;
    bestPossessionTeamID = -1;
    goalScored = false;
    goalScoredTimer = 0;
    maxRtt_ms = 0;
    cameraFOV = 0.0f;
    cameraNearCap = 0.0f;
    cameraFarCap = 0.0f;
  }

  unsigned long matchTime_ms;
  unsigned long actualTime_ms;
  int score[2];
  int matchPhase;
  bool inPlay;
  bool inSetPiece;
  bool pause;
  int bestPossessionTeamID; // lets the client's camera pan like the host's
  bool goalScored; // synced so the goal replay triggers on every peer
  unsigned long goalScoredTimer;
  int maxRtt_ms; // host's max client RTT, so clients mirror the input delay

  std::vector<SnapshotPlayer> players;
  std::vector<SnapshotPlayer> officials;

  blunted::Vector3 ballPosition;
  blunted::Quaternion ballOrientation;

  // Host-computed camera, so every peer shows the exact same view.
  blunted::Quaternion cameraOrientation;
  blunted::Quaternion cameraNodeOrientation;
  blunted::Vector3 cameraNodePosition;
  float cameraFOV;
  float cameraNearCap;
  float cameraFarCap;
};

Snapshot CaptureSnapshot(Match *match);
void WriteSnapshot(NetBuffer &buffer, const Snapshot &snapshot);
Snapshot ReadSnapshot(NetBuffer &buffer);

// Applies poses to the client's Match. Missing ids / unresolved animations are
// skipped rather than fatal, so a partial snapshot still presents something.
// Returns the number of poses actually applied (diagnostics).
int ApplySnapshot(Match *match, const Snapshot &snapshot, const std::vector<blunted::Animation*> &animTable);

#endif

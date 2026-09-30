#include "setpiecepresentation.hpp"

#include "../match.hpp"
#include "../team.hpp"
#include "../player/player.hpp"

#include "../../main.hpp" // SetYellowDebugPilon

#include <vector>

using namespace blunted;

SetPiecePresentation::SetPiecePresentation(Match *match) : match(match) {
  type = e_SetPiece_None;
  taker = 0;
  penaltyAimActive = false;
  penaltyAimFrozen = false;
  penaltyAim = setpiecelogic::DefaultPenaltyAim();
  chargeRatio = 0.0f;
  lastPenaltyAimTime_ms = 0;

  camActive = false;
  camFrozen = false;
  camType = e_SetPiece_None;
  camSpot = Vector3(0);
  camForward = Vector3(0, -1, 0);
  camSide = 1.0f;
  camReleaseTime_ms = 0;
}

void SetPiecePresentation::Process() {
  // Match::setPieceType is set by Referee from RefereeBuffer.desiredSetPiece when the whistle
  // blows (StartSetPiece); the taker is resolved by PrepareSetPiece. Both are read here, not owned.
  type = match->GetSetPieceType();
  taker = 0;
  if (type != e_SetPiece_None) taker = match->GetRefereeBuffer().taker;

  // The reticle is presentation for the local penalty taker only: drop it as soon as the penalty
  // is over, or the taker is no longer a local human, even if no controller ticked it this frame.
  if (!IsPenalty() || GetRole(taker) != e_SetPieceRole_Kicker) EndPenaltyAim();
}

void SetPiecePresentation::SetRemoteIdentity(e_SetPiece newType, int takerTeam, int takerSlot) {
  type = newType;
  taker = 0;
  if (type != e_SetPiece_None && takerTeam >= 0 && takerTeam < 2 && takerSlot >= 0) {
    const std::vector<Player*> &all = match->GetTeam(takerTeam)->GetAllPlayers();
    if (takerSlot < (int)all.size()) taker = all.at(takerSlot);
  }
  // Role-based cleanup is host-only (it needs the live ownership); the client only knows the type.
  if (!IsPenalty()) EndPenaltyAim();
}

e_SetPiecePhase SetPiecePresentation::GetPhase() const {
  if (IsSetPiece()) return e_SetPiecePhase_Active; // StartSetPiece fired at startTime: ball live
  if (match->IsRemotePresentation()) return e_SetPiecePhase_None; // client only syncs the type
  const RefereeBuffer &buffer = match->GetRefereeBuffer();
  if (!buffer.active) return e_SetPiecePhase_None; // buffer.active is raised at stopTime
  if (match->GetActualTime_ms() < buffer.prepareTime) return e_SetPiecePhase_Waiting;
  // prepareTime..startTime: players positioned, before the whistle flips the type to Active
  return e_SetPiecePhase_Preparing;
}

bool SetPiecePresentation::IsLocalPlayer(Player *player) const {
  if (!player) return false;
  int owner = match->IsRemotePresentation() ? player->GetRemoteOwnerId()
                                            : player->GetTeam()->GetControllingPeerId(player->GetID());
  return owner >= 0 && owner == match->GetLocalPeerId();
}

bool SetPiecePresentation::AllowsKeeper() const {
  // The role set differs by type: a penalty can hand the defending keeper a role, every other set
  // piece only ever offers the Kicker role (spec §2.2). Type dispatch stays a plain switch here;
  // there is no class-per-standard.
  switch (type) {
    case e_SetPiece_Penalty: return true;
    case e_SetPiece_None:
    case e_SetPiece_KickOff:
    case e_SetPiece_GoalKick:
    case e_SetPiece_FreeKick:
    case e_SetPiece_Corner:
    case e_SetPiece_ThrowIn: return false;
  }
  return false;
}

e_SetPieceRole SetPiecePresentation::GetRole(Player *player) const {
  if (!player || !IsSetPiece() || !IsLocalPlayer(player)) return e_SetPieceRole_None;
  if (player == taker) return e_SetPieceRole_Kicker;
  // Keeper only exists on a penalty, and only for the defending team's goalie.
  if (AllowsKeeper() && taker && player->GetTeamID() != taker->GetTeamID() &&
      player == player->GetTeam()->GetGoalie()) {
    return e_SetPieceRole_Keeper;
  }
  return e_SetPieceRole_None;
}

SetPieceHudState SetPiecePresentation::GetHudState(Player *player) const {
  SetPieceHudState state;
  state.role = GetRole(player);
  state.type = type;
  if (state.role == e_SetPieceRole_Kicker) {
    if (IsPenalty()) state.aimPoint = PenaltyAimWorldPoint();
    state.chargeRatio = chargeRatio;
  }
  return state;
}

Vector3 SetPiecePresentation::PenaltyAimWorldPoint() const {
  if (!taker) return Vector3(0, 0, 0);
  float side = taker->GetTeam()->GetSide();
  return Vector3(-side * pitchHalfW, penaltyAim.lateral, penaltyAim.height);
}

void SetPiecePresentation::UpdatePenaltyAim(float stickLateral, float stickHeight, bool hasInput, bool shotPressed, float chargeRatio, unsigned long now) {
  if (!penaltyAimActive) {
    penaltyAim = setpiecelogic::DefaultPenaltyAim();
    penaltyAimFrozen = false;
    penaltyAimActive = true;
    lastPenaltyAimTime_ms = now;
  }

  // commit the aim the moment the shot button is pressed
  if (!penaltyAimFrozen && shotPressed) penaltyAimFrozen = true;

  float dt = (now - lastPenaltyAimTime_ms) / 1000.0f;
  lastPenaltyAimTime_ms = now;
  if (dt <= 0.0f || dt > 0.2f) dt = 0.01f;

  if (!penaltyAimFrozen) penaltyAim = setpiecelogic::UpdatePenaltyAim(penaltyAim, stickLateral, stickHeight, hasInput, dt);
  this->chargeRatio = chargeRatio;

  DrawPenaltyReticle();
}

bool SetPiecePresentation::HasSetPieceCamera(e_SetPiece type) {
  // Only these four get a camera; throw-in and kickoff keep the normal camera (spec §4).
  switch (type) {
    case e_SetPiece_Penalty:
    case e_SetPiece_FreeKick:
    case e_SetPiece_Corner:
    case e_SetPiece_GoalKick: return true;
    case e_SetPiece_None:
    case e_SetPiece_KickOff:
    case e_SetPiece_ThrowIn: return false;
  }
  return false;
}

Player *SetPiecePresentation::CameraTaker() const {
  // Host: the referee buffer knows the taker from prepareTime on (before the whistle flips
  // Match::setPieceType, which is why the camera reads the buffer itself). Client: the snapshot
  // taker, resolved in SetRemoteIdentity.
  if (match->IsRemotePresentation()) return taker;
  return match->GetRefereeBuffer().taker;
}

bool SetPiecePresentation::LocalRoleOwnsCamera(e_SetPiece camTypeNow, Player *camTaker) const {
  if (!camTaker) return false;
  if (IsLocalPlayer(camTaker)) return true; // Kicker
  // On a penalty the defending human follows the taker automatically, without having to select the
  // keeper: they see the same camera behind the kicker (spec §4). Picking the keeper for control is
  // a separate effort, so it must not gate the camera.
  if (camTypeNow == e_SetPiece_Penalty) {
    const std::vector<Player*> &defenders = match->GetTeam(abs(camTaker->GetTeamID() - 1))->GetAllPlayers();
    for (unsigned int i = 0; i < defenders.size(); i++) {
      if (IsLocalPlayer(defenders.at(i))) return true;
    }
  }
  return false;
}

Vector3 SetPiecePresentation::CameraSpot() const {
  Vector3 spot = match->IsRemotePresentation() ? match->GetBall()->GetStatePosition()
                                               : match->GetRefereeBuffer().restartPos;
  spot.coords[2] = 0.0f;
  return spot;
}

Vector3 SetPiecePresentation::CameraBaseForward(const Vector3 &spot, float side) const {
  // Base heading is from the spot to the centre of the goal the taker attacks; the aim (spec §6)
  // will rotate it for free kicks, corners and goal kicks. Penalty keeps this fixed.
  Vector3 forward(-side * pitchHalfW - spot.coords[0], -spot.coords[1], 0.0f);
  if (forward.GetLength() < 0.001f) forward = Vector3(-side, 0, 0);
  else forward.Normalize();
  return forward;
}

void SetPiecePresentation::ApplyCamera() {
  float back = 0.0f, height = 0.0f, lookY = 0.0f;
  switch (camType) {
    case e_SetPiece_Penalty:
      back = _default_SetPiece_CamPenaltyBack;
      height = _default_SetPiece_CamPenaltyHeight;
      lookY = _default_SetPiece_CamPenaltyLookY;
      break;
    case e_SetPiece_FreeKick:
      back = _default_SetPiece_CamFreeKickBack;
      height = _default_SetPiece_CamFreeKickHeight;
      lookY = _default_SetPiece_CamFreeKickLookY;
      break;
    case e_SetPiece_Corner:
      back = _default_SetPiece_CamCornerBack;
      height = _default_SetPiece_CamCornerHeight;
      lookY = _default_SetPiece_CamCornerLookY;
      break;
    case e_SetPiece_GoalKick:
      back = _default_SetPiece_CamGoalKickBack;
      height = _default_SetPiece_CamGoalKickHeight;
      lookY = _default_SetPiece_CamGoalKickLookY;
      break;
    default: return;
  }

  Vector3 eye = camSpot - camForward * back + Vector3(0, 0, height);
  // The penalty looks at the fixed goal centre; the other set pieces look ahead along the heading.
  Vector3 look = (camType == e_SetPiece_Penalty)
                     ? Vector3(-camSide * pitchHalfW, 0, lookY)
                     : camSpot + camForward * _default_SetPiece_CamAhead + Vector3(0, 0, lookY);
  match->SetSetPieceCamera(eye, look, _default_SetPiece_CamFov, _default_SetPiece_CamNear, _default_SetPiece_CamFar);
}

void SetPiecePresentation::ReleaseCamera() {
  if (!camActive) return;
  camActive = false;
  camFrozen = false;
  camReleaseTime_ms = 0;
  match->SetAutoUpdateIngameCamera(true);
}

void SetPiecePresentation::NotifyKickerCommitted(bool isShot) {
  // The kick is committed the moment the action button is released (spec §4), not on ball touch:
  // freeze the view, then schedule the release by type / action.
  if (!camActive || camFrozen) return;
  camFrozen = true;
  unsigned long now = match->GetActualTime_ms();
  if (camType == e_SetPiece_Penalty) {
    camReleaseTime_ms = now + _default_SetPiece_CamHold_ms; // always watch the flight
  } else if (camType == e_SetPiece_FreeKick) {
    camReleaseTime_ms = isShot ? now + _default_SetPiece_CamHold_ms : now; // strike vs pass/cross
  } else {
    camReleaseTime_ms = now; // corner and goal kick hand back on release
  }
}

void SetPiecePresentation::UpdateCamera() {
  // Scorer cam wins unconditionally (spec §4); kickoff, which shares the scorer moment, has no
  // set-piece camera anyway.
  if (camActive && match->IsGoalScored()) { ReleaseCamera(); return; }

  const RefereeBuffer &buffer = match->GetRefereeBuffer();
  e_SetPiece camTypeNow;
  if (match->IsRemotePresentation()) {
    camTypeNow = GetType(); // the client only learns the type once the ball is live
  } else {
    camTypeNow = (buffer.active && match->GetActualTime_ms() >= buffer.prepareTime)
                     ? buffer.desiredSetPiece : match->GetSetPieceType();
  }
  Player *camTaker = CameraTaker();

  if (camActive) {
    unsigned long now = match->GetActualTime_ms();
    if (camFrozen) {
      if (now >= camReleaseTime_ms) { ReleaseCamera(); return; }
    } else if (!HasSetPieceCamera(camTypeNow) || !LocalRoleOwnsCamera(camTypeNow, camTaker)) {
      // The set piece ended without a local commit: the local peer is the defending keeper on a
      // penalty and the taker is the opponent. Watch the flight as if the strike had been committed.
      if (camType == e_SetPiece_Penalty) {
        camFrozen = true;
        camReleaseTime_ms = now + _default_SetPiece_CamHold_ms;
      } else {
        ReleaseCamera();
        return;
      }
    }
    ApplyCamera();
    return;
  }

  // Activate at prepareTime (taker known, restartPos fixed) and only when a local role owns it.
  if (!HasSetPieceCamera(camTypeNow)) return;
  e_SetPiecePhase phase = GetPhase();
  if (phase != e_SetPiecePhase_Preparing && phase != e_SetPiecePhase_Active) return;
  if (!LocalRoleOwnsCamera(camTypeNow, camTaker)) return;

  camActive = true;
  camFrozen = false;
  camType = camTypeNow;
  camSpot = CameraSpot();
  camSide = camTaker->GetTeam()->GetSide();
  camForward = CameraBaseForward(camSpot, camSide);
  camReleaseTime_ms = 0;
  ApplyCamera();
}

void SetPiecePresentation::EndPenaltyAim() {
  if (penaltyAimActive) HidePenaltyMarker();
  penaltyAimActive = false;
  penaltyAimFrozen = false;
  penaltyAim = setpiecelogic::DefaultPenaltyAim();
  chargeRatio = 0.0f;
  lastPenaltyAimTime_ms = 0;
}

void SetPiecePresentation::DrawPenaltyReticle() {
  // Drawing is separate from the state above and gated on the role: only a locally controlled
  // Kicker gets the reticle (spec §2.6).
  SetPieceHudState hud = GetHudState(taker);
  if (hud.role != e_SetPieceRole_Kicker || !IsPenalty()) {
    HidePenaltyMarker();
    return;
  }
  SetYellowDebugPilon(hud.aimPoint);
}

void SetPiecePresentation::HidePenaltyMarker() {
  SetYellowDebugPilon(Vector3(0, 0, -100));
}

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

// Set-piece presentation carrier (spec 2026-09-29 §2). One object owned by Match that reads the
// set-piece type and taker from RefereeBuffer (host) or from the snapshot (thin client) and derives
// the roles of the local human players. It owns no set-piece lifecycle and no state machine of its
// own: the phase is derived from RefereeBuffer.active + Match::IsInSetPiece() + the referee timers,
// and Referee still ends a set piece via the taker's TouchAnim().

#ifndef _HPP_SETPIECEPRESENTATION
#define _HPP_SETPIECEPRESENTATION

#include "../../gamedefines.hpp"
#include "setpiecelogic.hpp"

using namespace blunted;

class Match;
class Player;

// A local human player's part in the current set piece. There is deliberately no Wall role: the
// wall is AI defenders.
enum e_SetPieceRole {
  e_SetPieceRole_None = 0,
  e_SetPieceRole_Kicker,
  e_SetPieceRole_Keeper,
};

// Presentation phase, computed on demand (never stored): the referee timers only mark the
// transitions, the carrier derives which side of them it is on.
enum e_SetPiecePhase {
  e_SetPiecePhase_None = 0,  // not in a set piece
  e_SetPiecePhase_Waiting,   // buffer active, before prepareTime (film referee / cards window)
  e_SetPiecePhase_Preparing, // prepareTime..startTime, players being positioned
  e_SetPiecePhase_Active,    // startTime passed: ball live, waiting for the taker's touch
};

// HUD-facing state for a local player. Draw the overlay only while `role != e_SetPieceRole_None`.
// `aimPoint` is the reticle on the attacking goal line (penalty), zero otherwise; `chargeRatio` is
// the action charge (0 when not charging).
struct SetPieceHudState {
  SetPieceHudState() : role(e_SetPieceRole_None), type(e_SetPiece_None), aimPoint(), chargeRatio(0.0f) {}
  e_SetPieceRole role;
  e_SetPiece type;
  Vector3 aimPoint;
  float chargeRatio;
};

class SetPiecePresentation {

  public:
    SetPiecePresentation(Match *match);

    // Host tick: refresh the identity from RefereeBuffer and drop the reticle once the penalty is
    // over (or was never taken).
    void Process();
    // Thin client: the identity arrives in the snapshot (type + actual taker team/slot).
    void SetRemoteIdentity(e_SetPiece type, int takerTeam, int takerSlot);

    e_SetPiece GetType() const { return type; }
    Player *GetTaker() const { return taker; }
    bool IsSetPiece() const { return type != e_SetPiece_None; }

    // Phase derived from RefereeBuffer.active + IsInSetPiece() + stopTime/prepareTime/startTime.
    // On a thin client only the type is synced, so a non-None phase is Active.
    e_SetPiecePhase GetPhase() const;

    // Role of `player` in the current set piece: None for AI and for remote peers.
    e_SetPieceRole GetRole(Player *player) const;
    SetPieceHudState GetHudState(Player *player) const;

    // Penalty reticle, owned here so the HUD state can expose it (spec §2.6). The local kicker's
    // controller feeds the raw stick input; the carrier stores, advances and draws the aim.
    const setpiecelogic::PenaltyAim &GetPenaltyAim() const { return penaltyAim; }
    void UpdatePenaltyAim(float stickLateral, float stickHeight, bool hasInput, bool shotPressed, float chargeRatio, unsigned long now);

  protected:
    bool IsPenalty() const { return type == e_SetPiece_Penalty; }
    bool IsLocalPlayer(Player *player) const;
    bool AllowsKeeper() const;
    Vector3 PenaltyAimWorldPoint() const;
    void EndPenaltyAim();
    void DrawPenaltyReticle();
    void HidePenaltyMarker();

    Match *match;
    e_SetPiece type;
    Player *taker;

    bool penaltyAimActive;
    bool penaltyAimFrozen;
    setpiecelogic::PenaltyAim penaltyAim;
    float chargeRatio;
    unsigned long lastPenaltyAimTime_ms;
};

#endif

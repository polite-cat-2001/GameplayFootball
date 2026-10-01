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
class Team;
class IHIDevice;

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

    // True when this peer's human controls the actual taker (#30): only then may the taker menu
    // open. On a thin client the selected player carries the remote owner id (set in #29).
    bool IsLocalTaker() const;
    // Device of the local human who controls the taker (host: the taker's own controller; thin
    // client: its local HID device), or null. Drives the taker-menu open/cancel/hint (#30).
    IHIDevice *LocalKickerDevice() const;
    // Any of the four kick buttons (pass / through / cross / shot) is held. On keyboard these are the
    // W/A/S/D keys, so a keyboard taker menu is navigated with the arrows and closed with these.
    static bool IsKickButtonPressed(IHIDevice *device);
    // Select edge with no action button held: the taker-menu open trigger (#30). Shared by the host
    // HumanController path and the thin-client NetMatchSession path.
    static bool IsTakerMenuSelect(IHIDevice *device);

    // Penalty reticle, owned here so the HUD state can expose it (spec §2.6). The local kicker's
    // controller feeds the raw stick input; the carrier stores, advances and draws the aim.
    const setpiecelogic::PenaltyAim &GetPenaltyAim() const { return penaltyAim; }
    void UpdatePenaltyAim(float stickLateral, float stickHeight, bool hasInput, bool shotPressed, float chargeRatio, unsigned long now);

    // Set-piece kick aiming (spec §6), owned here for the same reason as the penalty reticle: the
    // intent is local to the human taker (on a thin client the host applies it from the input
    // channel). Stick-X turns the heading before the kick is charged; while charging it feeds the
    // curl accumulator instead, from the press until the ball is struck.
    void UpdateSetPieceAim(float stickX, bool hasInput, bool charging, unsigned long now);
    // Plans the kick from the current aim/curl. Returns false if this set piece has no aim model.
    bool PlanSetPieceKick(e_FunctionType functionType, float charge, float stickY, setpiecelogic::SetPieceKickPlan &plan) const;

    // Set-piece camera (spec §4). Presentation-only: while a local human owns the Kicker (or the
    // defending Keeper on a penalty) role, the camera sits behind the taker and turns the auto
    // camera off. `UpdateCamera()` is called every tick; the kicker's controller calls
    // `NotifyKickerCommitted()` the moment its action button commits, which freezes the view and
    // schedules the release.
    void UpdateCamera();
    void NotifyKickerCommitted(bool isShot);

    // Thin client: the local HID device, so the penalty reticle can be driven here. On a thin client
    // HumanController never ticks (Match::Process early-returns), so nothing else feeds the aim.
    void SetLocalHIDDevice(IHIDevice *device) { localDevice = device; }

  protected:
    bool IsPenalty() const { return type == e_SetPiece_Penalty; }
    bool IsLocalPlayer(Player *player) const;
    bool AllowsKeeper() const;
    Vector3 PenaltyAimWorldPoint() const;
    void EndPenaltyAim();
    void DrawPenaltyReticle();
    void HidePenaltyMarker();

    // Camera helpers (spec §4). Pose is derived from a frozen spot + horizontal heading so the
    // set-piece camera never follows the ball. The type set that gets a camera is fixed: penalty,
    // free kick, corner and goal kick (throw-in and kickoff keep the normal camera).
    static bool HasSetPieceCamera(e_SetPiece type);
    void UpdateRemotePenaltyAim();
    void UpdateRemoteSetPieceAim();
    // One set piece's kick aim: a new type (or leaving the set piece) restarts it. Shared by the
    // host tick (Process) and the thin client (SetRemoteIdentity).
    void RefreshSetPieceAimIdentity();
    Player *CameraTaker() const;
    bool TeamHasLocalHuman(Team *team) const;
    bool LocalRoleOwnsCamera(e_SetPiece type, Player *taker) const;
    Vector3 CameraSpot() const;
    Vector3 CameraBaseForward(const Vector3 &spot, float side) const;
    Vector3 CameraForwardForType(e_SetPiece type, const Vector3 &spot, float side) const;
    void ApplyCamera();
    void ReleaseCamera();

    Match *match;
    e_SetPiece type;
    Player *taker;

    bool penaltyAimActive;
    bool penaltyAimFrozen;
    setpiecelogic::PenaltyAim penaltyAim;
    float chargeRatio;
    unsigned long lastPenaltyAimTime_ms;

    // Set-piece kick aiming (spec §6).
    e_SetPiece setPieceAimType;
    setpiecelogic::SetPieceAim setPieceAim;
    bool setPieceAimInitialized;
    float setPieceCurl;
    unsigned long lastSetPieceAimTime_ms;
    void ResetSetPieceAim();

    IHIDevice *localDevice; // thin client only: drives the reticle locally
    bool remoteShotHeld;
    bool remoteKickHeld;    // thin client: a set-piece kick button was down last tick
    bool remoteKickWasShot; // ...and it was the shot button

    bool camActive;
    bool camFrozen;              // pose fixed at the kick, watching the flight
    bool camSuppressed;          // released for this set piece: don't re-engage until it is gone
    e_SetPiece camType;
    Vector3 camSpot;            // ball spot at activation (z = 0), frozen
    Vector3 camForward;         // horizontal unit heading toward the target
    float camSide;              // side of the taker's team at activation
    unsigned long camReleaseTime_ms; // when to hand the camera back after a commit
};

#endif

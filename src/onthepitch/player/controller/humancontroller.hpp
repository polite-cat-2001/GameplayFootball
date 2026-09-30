// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#ifndef _HPP_FOOTBALL_ONTHEPITCH_HUMANCONTROLLER
#define _HPP_FOOTBALL_ONTHEPITCH_HUMANCONTROLLER

#include "playercontroller.hpp"

#include "../../../hid/ihidevice.hpp"

class Player;

class HumanController : public PlayerController {

  public:
    HumanController(Match *match, IHIDevice *hid);
    virtual ~HumanController();

    virtual void SetPlayer(PlayerBase *player);

    virtual void RequestCommand(PlayerCommandQueue &commandQueue);
    virtual void Process();
    virtual Vector3 GetDirection();
    virtual float GetFloatVelocity();

    virtual int GetReactionTime_ms();

    IHIDevice *GetHIDevice() { return hid; }

    int GetActionMode() { return actionMode; }

    // 0..1 charge of the action in progress (spec §2.6 HUD state); 0 when not charging.
    float GetChargeRatio() const;

    // shot variant fixed at the moment Shot was pressed (curl/chip modifier)
    bool IsChargingChip() { return actionMode == 2 && actionButton == e_ButtonFunction_Shot && pendingShotType == e_ShotType_Chip; }
    bool IsChargingShotVariant() { return actionMode == 2 && actionButton == e_ButtonFunction_Shot && pendingShotType != e_ShotType_Normal; }

    virtual void Reset();

  protected:

    void _GetHidInput(Vector3 &rawInputDirection, float &rawInputVelocityFloat);
    e_ShotType _SampleShotType();

    // penalty reticle (spec 2026-09-29 §7); the aim itself lives in SetPiecePresentation
    bool _IsPenaltyTaker();
    void _UpdatePenaltyAim();

    IHIDevice *hid;

    // set when a contextual button (example: pass/defend button) is pressed
    // once this is set and the button stays pressed, it stays the same
    // 0: undefined, 1: off-the-ball button active, 2: on-the-ball button active/action queued
    int actionMode;

    e_ButtonFunction actionButton;
    int actionBufferTime_ms;
    int gauge_ms;

    e_ShotType pendingShotType;

    // double-tap Sprint (knock-on) tracking
    int lastSprintTapTime_ms;
    bool knockOnArmed;
    int knockOnExpireTime_ms;

    // stuff to keep track of analog stick (or keys even) so that we can use a direction once it's been pointed in for a while, instead of directly
    Vector3 previousDirection;
    Vector3 steadyDirection;
    int lastSteadyDirectionSnapshotTime_ms;

};

#endif

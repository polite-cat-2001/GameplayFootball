// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#include "humancontroller.hpp"

#include <algorithm>
#include <cmath>

#include "../../AIsupport/AIfunctions.hpp"
#include "../../keeper/keeperlogic.hpp"
#include "../../setpiece/setpiecelogic.hpp"
#include "../../setpiece/setpiecepresentation.hpp"

#include "../../../main.hpp"

HumanController::HumanController(Match *match, IHIDevice *hid) : PlayerController(match), hid(hid) {
  Reset();
}

HumanController::~HumanController() {
}

void HumanController::SetPlayer(PlayerBase *player) {
  lastSwitchTime_ms = match->GetActualTime_ms();

  PlayerController::SetPlayer(player);
}

void HumanController::RequestCommand(PlayerCommandQueue &commandQueue) {

  CastPlayer()->SetDesiredTimeToBall_ms(0);

  _Preprocess(); // calculate some variables


  // human input

  Vector3 rawInputDirection;
  float rawInputVelocityFloat = 0;
  _GetHidInput(rawInputDirection, rawInputVelocityFloat);
  _SetInput(rawInputDirection, rawInputVelocityFloat);

  // Keeper foot clear (#32): the ball was dropped on the button; once it has settled at the
  // keeper's feet, execute the intent through the normal kick path.
  if (keeperClearArmed) {
    actionMode = 0;
    gauge_ms = 0;
    actionBufferTime_ms = 0;
    if (_IsKeeperOutfield() && CastPlayer()->HasPossession()) {
      _KeeperClearKickCommand(commandQueue);
      keeperClearArmed = false;
      return;
    }
    if ((int)match->GetActualTime_ms() - keeperClearArmedTime_ms > 2000) {
      keeperClearArmed = false; // the ball never settled (lost it): give up and play normally
    } else {
      PlayerCommand command;
      command.desiredFunctionType = e_FunctionType_Movement;
      command.useDesiredMovement = true;
      command.desiredDirection = CastPlayer()->GetDirectionVec();
      command.desiredVelocityFloat = idleVelocity;
      command.useDesiredLookAt = true;
      command.desiredLookAt = CastPlayer()->GetPosition() + CastPlayer()->GetDirectionVec() * 10.0f;
      commandQueue.push_back(command);
      return;
    }
  }


  // clear buffer?

  e_FunctionType functionType = CastPlayer()->GetCurrentFunctionType();
  if (actionMode == 2 &&
      (functionType == e_FunctionType_ShortPass ||
       functionType == e_FunctionType_LongPass ||
       functionType == e_FunctionType_HighPass ||
       functionType == e_FunctionType_Shot) && !CastPlayer()->TouchPending()) {
    actionMode = 0;
    gauge_ms = 0;
    actionBufferTime_ms = 0;
  }

  if (actionMode == 1 &&
      (functionType == e_FunctionType_Sliding ||
       functionType == e_FunctionType_Interfere) && !CastPlayer()->TouchPending()) {
    actionMode = 0;
    gauge_ms = 0;
    actionBufferTime_ms = 0;
  }


  // cancels

  // shot cancel
  if (actionMode == 2 && actionButton == e_ButtonFunction_Shot && hid->GetButton(e_ButtonFunction_ShortPass) && !match->IsInSetPiece()) {
    actionMode = 0;
    gauge_ms = 0;
    actionBufferTime_ms = 0;
  }

  // high pass cancel
  if (actionMode == 2 && actionButton == e_ButtonFunction_HighPass && hid->GetButton(e_ButtonFunction_ShortPass) && !match->IsInSetPiece()) {
    actionMode = 0;
    gauge_ms = 0;
    actionBufferTime_ms = 0;
  }

  // cancel action buffer
  if (actionMode == 2 && !match->IsInSetPiece() &&
      (actionBufferTime_ms > 2000 ||
       CastPlayer()->GetTimeNeededToGetToBall_ms() > CastPlayer()->GetTimeNeededToGetToBall_previous_ms() + 700 ||
      (CastPlayer()->GetCurrentFunctionType() == e_FunctionType_Interfere))) {
    actionMode = 0;
    gauge_ms = 0;
    actionBufferTime_ms = 0;
  }

  // cancel pressure and such
  if (actionMode == 1 && actionBufferTime_ms > 1000) {
    actionMode = 0;
    gauge_ms = 0;
    actionBufferTime_ms = 0;
  }


  // execute buffer?

  if (actionMode == 2) {

    if (!hid->GetButton(actionButton) ||
        (hid->GetButton(actionButton) && gauge_ms > 500) || // allow anim to kick in before queue is complete (before button is released), it will usually touch ball after the remaining time anyway, so we still have time to add more power, yet still respond as fast as possible
        (!CastPlayer()->HasPossession() && !match->IsInSetPiece() && actionBufferTime_ms > 0)) {

      // shots charge over KICK_CHARGE_MAX_TIME; other actions keep the 1 s gauge
      float gaugeFactor = GetChargeRatio();

      // Keeper distribution (spec §8, #32): the hand throw (ShortPass) plays the existing throw
      // clip on release; the directed foot clear (HighPass) drops the ball and arms a kick that
      // fires once the ball has settled at the feet.
      if (_IsKeeperHands() && actionButton == e_ButtonFunction_ShortPass) {
        _KeeperDistributionCommand(commandQueue);
        actionMode = 0;
        gauge_ms = 0;
        actionBufferTime_ms = 0;
        return;
      }
      if (_IsKeeperHands() && actionButton == e_ButtonFunction_HighPass) {
        match->KeeperDropToFeet(team->GetID());
        keeperClearArmed = true;
        keeperClearButton = e_ButtonFunction_HighPass;
        keeperClearCharge = _KeeperChargeRatio();
        keeperClearAim = inputDirection.Get2D().GetNormalized(CastPlayer()->GetDirectionVec().Get2D());
        keeperClearArmedTime_ms = match->GetActualTime_ms();
        actionMode = 0;
        gauge_ms = 0;
        actionBufferTime_ms = 0;
        PlayerCommand idle;
        idle.desiredFunctionType = e_FunctionType_Movement;
        idle.useDesiredMovement = true;
        idle.desiredDirection = CastPlayer()->GetDirectionVec();
        idle.desiredVelocityFloat = idleVelocity;
        idle.useDesiredLookAt = true;
        idle.desiredLookAt = CastPlayer()->GetPosition() + CastPlayer()->GetDirectionVec() * 10.0f;
        commandQueue.push_back(idle);
        return;
      }

      // action button released!

      // The set-piece camera watches by action type from this point (spec §4).
      if (team->GetController()->GetPieceTaker() == player) {
        match->GetSetPiecePresentation()->NotifyKickerCommitted(actionButton == e_ButtonFunction_Shot);
      }

      // force set piece methods
      if (match->IsInSetPiece() && team->GetController()->GetPieceTaker() == player && team->GetController()->GetSetPieceType() == e_SetPiece_KickOff) {

        PlayerCommand command;

        command.desiredFunctionType = e_FunctionType_ShortPass;
        command.touchInfo.autoDirectionBias = 1.0f;
        command.touchInfo.autoPowerBias = 1.0f;
        command.touchInfo.inputDirection = player->GetDirectionVec(); // dud
        command.touchInfo.inputPower = 0.1f; // dud

        Vector3 desiredTargetPosition = player->GetPosition() + player->GetDirectionVec() * 1.0f;
        command.touchInfo.forcedTargetPlayer = AI_GetClosestPlayer(team, desiredTargetPosition, false, CastPlayer());

        AI_GetPass(CastPlayer(), command.desiredFunctionType, command.touchInfo.inputDirection, command.touchInfo.inputPower, command.touchInfo.autoDirectionBias, command.touchInfo.autoPowerBias, command.touchInfo.desiredDirection, command.touchInfo.desiredPower, command.touchInfo.targetPlayer, command.touchInfo.forcedTargetPlayer);

        commandQueue.push_back(command);


      } else if (_IsSetPieceKicker()) {

        // free kick / corner / goal kick: the aimed heading, planned height and accumulated curl
        // replace the movement-stick input and the auto-assist (spec §6)
        _SetPieceKickCommand(commandQueue);


      } else if (actionButton == e_ButtonFunction_ShortPass) {

        PlayerCommand command;
        command.desiredFunctionType = e_FunctionType_ShortPass;
        command.useDesiredMovement = false;
        command.useDesiredLookAt = false;

        float inputPower = clamp(pow(gaugeFactor, 0.7f), 0.01f, 1.0f);
        command.touchInfo.inputDirection = inputDirection;
        command.touchInfo.inputPower = inputPower;
        command.touchInfo.autoDirectionBias = GetConfiguration()->GetReal("gameplay_shortpass_autodirection", _default_ShortPass_AutoDirection);
        command.touchInfo.autoPowerBias = GetConfiguration()->GetReal("gameplay_shortpass_autopower", _default_ShortPass_AutoPower);
        AI_GetPass(CastPlayer(), command.desiredFunctionType, command.touchInfo.inputDirection, command.touchInfo.inputPower, command.touchInfo.autoDirectionBias, command.touchInfo.autoPowerBias, command.touchInfo.desiredDirection, command.touchInfo.desiredPower, command.touchInfo.targetPlayer);

        commandQueue.push_back(command);


      } else if (actionButton == e_ButtonFunction_LongPass) {

        PlayerCommand command;
        command.desiredFunctionType = e_FunctionType_LongPass;
        command.useDesiredMovement = false;
        command.useDesiredLookAt = false;

        float inputPower = clamp(pow(gaugeFactor, 0.65f), 0.01f, 1.0f);
        command.touchInfo.inputDirection = inputDirection;
        command.touchInfo.inputPower = inputPower;
        command.touchInfo.autoDirectionBias = GetConfiguration()->GetReal("gameplay_throughpass_autodirection", _default_ThroughPass_AutoDirection);
        command.touchInfo.autoPowerBias = GetConfiguration()->GetReal("gameplay_throughpass_autopower", _default_ThroughPass_AutoPower);
        AI_GetPass(CastPlayer(), command.desiredFunctionType, command.touchInfo.inputDirection, command.touchInfo.inputPower, command.touchInfo.autoDirectionBias, command.touchInfo.autoPowerBias, command.touchInfo.desiredDirection, command.touchInfo.desiredPower, command.touchInfo.targetPlayer);

        commandQueue.push_back(command);


      } else if (actionButton == e_ButtonFunction_HighPass) {

        PlayerCommand command;
        command.desiredFunctionType = e_FunctionType_HighPass;
        command.useDesiredMovement = false;
        command.useDesiredLookAt = false;

        float inputPower = clamp(pow(gaugeFactor, 0.55f), 0.01f, 1.0f);
        command.touchInfo.inputDirection = inputDirection;
        command.touchInfo.inputPower = inputPower;
        command.touchInfo.autoDirectionBias = GetConfiguration()->GetReal("gameplay_highpass_autodirection", _default_HighPass_AutoDirection);
        command.touchInfo.autoPowerBias = GetConfiguration()->GetReal("gameplay_highpass_autopower", _default_HighPass_AutoPower);
        AI_GetPass(CastPlayer(), command.desiredFunctionType, command.touchInfo.inputDirection, command.touchInfo.inputPower, command.touchInfo.autoDirectionBias, command.touchInfo.autoPowerBias, command.touchInfo.desiredDirection, command.touchInfo.desiredPower, command.touchInfo.targetPlayer);

        commandQueue.push_back(command);


      } else if (actionButton == e_ButtonFunction_Shot) {

        PlayerCommand command;
        command.desiredFunctionType = e_FunctionType_Shot;
        command.useDesiredMovement = false;
        command.useDesiredLookAt = false;
        command.desiredVelocityFloat = inputVelocityFloat; // this is so we can use sprint/dribble buttons as shot modifiers
        command.touchInfo.inputDirection = inputDirection;
        command.touchInfo.autoDirectionBias = GetConfiguration()->GetReal("gameplay_shot_autodirection", _default_Shot_AutoDirection);
        if (GetHIDevice()->GetDeviceType() == e_HIDeviceType_Keyboard) command.touchInfo.autoDirectionBias = 1.0f;
        command.touchInfo.desiredPower = clamp(pow(gaugeFactor, 0.6f), 0.01f, 1.0f);
        command.touchInfo.shotType = pendingShotType;

        if (_IsPenaltyTaker()) {
          // penalty: aim at the reticle instead of auto-aim. The struck point is the frozen
          // reticle plus a random sample from the spread disc that grows with the charge.
          float charge = clamp(gaugeFactor, 0.0f, 1.0f);
          setpiecelogic::PenaltyShotPlan plan = setpiecelogic::PlanPenaltyShot(match->GetSetPiecePresentation()->GetPenaltyAim(), charge);
          float side = CastPlayer()->GetTeam()->GetSide();
          Vector3 target(-side * pitchHalfW, plan.lateral, plan.height);
          command.touchInfo.desiredDirection = (target - CastPlayer()->GetPosition()).GetNormalized(CastPlayer()->GetDirectionVec());
          command.touchInfo.useAimTarget = true;
          command.touchInfo.aimLateral = plan.lateral;
          command.touchInfo.aimHeight = plan.height;
          command.touchInfo.useAimHeight = true;
          command.touchInfo.aimSpeed = plan.speed;
        } else {
          command.touchInfo.desiredDirection = AI_GetShotDirection(CastPlayer(), command.touchInfo.inputDirection, command.touchInfo.autoDirectionBias);
          setpiecelogic::ApplyShotPlan(command.touchInfo, setpiecelogic::PlanShot(match->GetBall()->Predict(0), CastPlayer()->GetTeam()->GetSide(), command.touchInfo.desiredDirection, command.touchInfo.desiredPower, pendingShotType));
        }

        commandQueue.push_back(command);

      }

    }


  } else if (actionMode == 1) {

    if (hid->GetButton(actionButton)) {

      if (actionButton == e_ButtonFunction_Sliding) {

        PlayerCommand command;
        command.desiredFunctionType = e_FunctionType_Sliding;
        command.useDesiredMovement = true;
        command.desiredDirection = inputDirection;
        command.desiredVelocityFloat = inputVelocityFloat;
        command.useDesiredLookAt = true;
        command.desiredLookAt = CastPlayer()->GetPosition() + CastPlayer()->GetMovement() * 0.1f + command.desiredDirection * 10.0f;
        commandQueue.push_back(command);

      }

      if (actionButton == e_ButtonFunction_TeamPressure) {

        team->GetController()->ApplyTeamPressure();

      }

      if (actionButton == e_ButtonFunction_KeeperRush) {

        team->GetController()->ApplyKeeperRush();

      }

    } else {

      // action button released!
      actionMode = 0;

    }
  }

  // set piece?
  if ((match->IsInSetPiece() && team->GetController()->GetPieceTaker() == player && (actionMode != 2 || (actionMode == 2 && hid->GetButton(actionButton)) || match->GetBallRetainer() == player)) ||
      (match->IsInSetPiece() && team->GetController()->GetPieceTaker() != player && match->GetBallRetainer() == 0)) {
    _SetPieceCommand(commandQueue);
    //if (team->GetController()->GetPieceTaker() == player) printf("waiting to take set piece!\n");
    //if (team->GetController()->GetPieceTaker() != player) printf("waiting for teammate to take set piece!\n");
    return;
  }

  // delay direction input until we have chosen a steady direction.
  // this is because humans can only move an analog stick so fast, and we don't want requeues to happen mid-analogstick-movement.
  Vector3 inputDirectionSaveNonsteady = inputDirection;
  //SetGreenDebugPilon(player->GetPosition() + inputDirection * (inputVelocityFloat * 0.5f + 0.5f));
  inputDirection = steadyDirection;
  //SetBlueDebugPilon(player->GetPosition() + inputDirection * (inputVelocityFloat * 0.5f + 0.6f));

  if (match->IsInPlay() && !match->IsInSetPiece()) {

    bool idleTurnToOpponentGoal = false;
    bool knockOn = false;
    if (hid->GetButton(e_ButtonFunction_Dribble)) idleTurnToOpponentGoal = true;
    if (!IsChargingShotVariant() && knockOnArmed) knockOn = true;

    // special adapted input for ballcontrol and trap, when we have shoot/pass buffers
    Vector3 inputDirectionSave2 = inputDirection;
    float inputVelocitySave2 = inputVelocityFloat;
    if (actionMode == 2) {
      float dot = CastPlayer()->GetDirectionVec().GetDotProduct(inputDirection) * 0.5f + 0.5f; // todo: test
      if (CastPlayer()->GetEnumVelocity() != e_Velocity_Idle) {
        inputDirection = (CastPlayer()->GetDirectionVec() * 1.0f + inputDirection * 0.0f).GetNormalized(inputDirection); // want inputdirection to be biggest, so we won't stubbornly fail to do 180s
      }
      dot = pow(dot, 1.5f); // prefer braking, even on slight angles
      dot = dot * 0.8f + 0.2f;
      dot *= 0.9f; // else, <=walk-anims may never work (backheels and such)
      inputVelocityFloat = CastPlayer()->GetFloatVelocity() * dot;
    }

    // ball control?
    bool keepCurrentBodyDirection = false;
    // sidestep dribble disabled for now, too quirky: if (hid->GetButton(e_ButtonFunction_Dribble)) keepCurrentBodyDirection = true;
    _BallControlCommand(commandQueue, idleTurnToOpponentGoal, knockOn, true, keepCurrentBodyDirection);

    // trap?
    _TrapCommand(commandQueue, idleTurnToOpponentGoal, knockOn);

    // reload original input
    if (actionMode == 2) {
      inputDirection = inputDirectionSave2;
      inputVelocityFloat = inputVelocitySave2;
    }

    // interfere?
    bool byAnyMeans = false;
    if (hid->GetButton(e_ButtonFunction_Pressure)) byAnyMeans = true;
    _InterfereCommand(commandQueue, byAnyMeans);
  }

  // movement
  bool forceMagnet = false;
  bool extraHaste = false;
  if (actionMode != 2 && hid->GetButton(e_ButtonFunction_Pressure)) {
    forceMagnet = true;
    extraHaste = true;
  }
  if (actionMode == 2) {
    forceMagnet = true;
    extraHaste = true;
  }
  _MovementCommand(commandQueue, forceMagnet, extraHaste);

  if (commandQueue.size() > 0) {
    PlayerCommand &command = commandQueue.at(commandQueue.size() - 1);
    assert(command.desiredFunctionType == e_FunctionType_Movement); // make sure this is the movement command (is probably guaranteed, check out _MovementCommand)

/*
    // no magnet
    command.desiredDirection = inputDirection;
    command.desiredVelocityFloat = inputVelocityFloat;
    if (command.desiredVelocityFloat < idleDribbleSwitch) command.desiredDirection = (_mentalImage->GetBallPrediction(500).Get2D() - player->GetPosition()).GetNormalized(inputDirection);
    //command.desiredLookAt = CastPlayer()->GetPosition() + inputDirection * 10;
    command.desiredLookAt = _mentalImage->GetBallPrediction(500).Get2D();
*/

    // super cancel
    if (!IsChargingShotVariant() && hid->GetButton(e_ButtonFunction_Special)) {
      if (!hasBestPossession) {
        command.desiredDirection = inputDirection;
        command.desiredVelocityFloat = inputVelocityFloat;
      }
    }
  }

  // reload original input
  inputDirection = inputDirectionSaveNonsteady;
}

void HumanController::Process() {

  // just doesn't work so well (fixes the 'humans can't change stick pos instantly' problem, but introduces too much lag). maybe revisit/update later
  bool enableSteadyDirectionSystem = false;

  PlayerController::Process();

  // Set-piece taker menu (#30) is up (or just closed and its button not yet released): the taker's
  // own device drives the menu, so freeze the gameplay side (no charge, no aim turn).
  if (match->IsSetPieceTakerMenuInputBlocked()) {
    actionMode = 0;
    gauge_ms = 0;
    actionBufferTime_ms = 0;
    return;
  }

  Vector3 currentDirection;
  float dud;
  _GetHidInput(currentDirection, dud);
  radian angle = fabs(currentDirection.GetAngle2D(previousDirection));
  previousDirection = currentDirection;

  // only set steadydirection if angle is small (= human probably reaching his intended direction)
  // or very large (= maybe the stick has been in deadzone space; humans can't move this fast)
  if (enableSteadyDirectionSystem) {
    if (angle < 0.01f * pi || angle > 0.65f * pi || (match->GetActualTime_ms() - lastSteadyDirectionSnapshotTime_ms) > 100) {
      steadyDirection = currentDirection;
      lastSteadyDirectionSnapshotTime_ms = match->GetActualTime_ms();
    }
  } else {
    steadyDirection = currentDirection;
    lastSteadyDirectionSnapshotTime_ms = match->GetActualTime_ms();
  }

  _CalculateSituation();

  // double-tap Sprint arms a one-shot knock-on; a single tap stays a plain sprint
  int sprintTime_ms = match->GetActualTime_ms();
  bool sprintPressed = hid->GetButton(e_ButtonFunction_Sprint);
  if (sprintPressed && !hid->GetPreviousButtonState(e_ButtonFunction_Sprint)) {
    if (sprintTime_ms - lastSprintTapTime_ms <= sprintDoubleTapWindow_ms) {
      knockOnArmed = true;
      knockOnExpireTime_ms = sprintTime_ms + sprintKnockOnWindow_ms;
    }
    lastSprintTapTime_ms = sprintTime_ms;
  }
  // the modifier is fed to ball touches for the duration of the knock-on window
  if (knockOnArmed && sprintTime_ms > knockOnExpireTime_ms) knockOnArmed = false;

  // Set-piece taker menu (#30): the local human actually taking the set piece opens it with Select
  // (gamepad Share / keyboard Tab). Only this peer's own device: a remote client's taker is a
  // NetHIDDevice here and opens its own menu. Not mid-action (charge/queued kick).
  if (!match->IsSetPieceTakerMenuInputBlocked() &&
      hid->GetOwnerId() == (unsigned int)match->GetLocalPeerId() &&
      match->IsInSetPiece() && team->GetController()->GetPieceTaker() == player &&
      actionMode != 2 && SetPiecePresentation::IsTakerMenuSelect(hid)) {
    match->RequestSetPieceTakerMenu();
  }

  // action?

  if (actionMode == 0 && (!match->IsInSetPiece() || team->GetController()->GetPieceTaker() == player)) {

    // todo: clean this up

    // what is the context: do we want defend buttons or pass/shot buttons?
    float possessionContext = possessionAmount - 1.0f;
    if (match->GetDesignatedPossessionPlayer() == player) {
      possessionContext = 1.0f; // new (keeper was allowed doing slidings before free kick sometimes lol, sign something was wrong)
    } else {
      // in situations where we aren't the designated player, we sometimes still want to do ball stuff, because we could try to extend our leg to pass, for example
      if (hid->GetButton(e_ButtonFunction_ShortPass)) possessionContext += 0.15f; // todo: bug: the logic of these weighings are based on the default 'pes' button settings.. need to take into account the defensive function of the buttons as well
      if (hid->GetButton(e_ButtonFunction_LongPass)) possessionContext += 0.15f;
      if (hid->GetButton(e_ButtonFunction_Shot)) possessionContext += 0.15f;

      if (hid->GetButton(e_ButtonFunction_Pressure)) possessionContext -= 0.15f;
      if (hid->GetButton(e_ButtonFunction_Sliding)) possessionContext -= 0.15f;
      if (hid->GetButton(e_ButtonFunction_TeamPressure)) possessionContext -= 0.15f;

      if (match->GetBall()->Predict(0).coords[2] > 1.5f) possessionContext += 0.2f;
    }

    if (possessionContext < 0.0f) {
      bool allowPressure = true;
      bool allowSliding = true;
      bool allowTeamPressure = true;
      bool allowKeeperRush = true;

      if (match->IsInSetPiece()) {
        allowPressure = false;
        allowSliding = false;
        allowTeamPressure = false;
        allowKeeperRush = false;
      }

      if (hid->GetButton(e_ButtonFunction_Pressure) && !hid->GetPreviousButtonState(e_ButtonFunction_Pressure) && allowPressure) {
        actionMode = 1;
        actionButton = e_ButtonFunction_Pressure;
      }

      if (hid->GetButton(e_ButtonFunction_Sliding) && !hid->GetPreviousButtonState(e_ButtonFunction_Sliding) && allowSliding) { // we don't want high passes to turn into slidings
        actionMode = 1;
        actionButton = e_ButtonFunction_Sliding;
      }

      if (hid->GetButton(e_ButtonFunction_TeamPressure) && !hid->GetPreviousButtonState(e_ButtonFunction_TeamPressure) && allowTeamPressure) {
        actionMode = 1;
        actionButton = e_ButtonFunction_TeamPressure;
      }

      if (hid->GetButton(e_ButtonFunction_KeeperRush) && !hid->GetPreviousButtonState(e_ButtonFunction_KeeperRush) && allowKeeperRush) {
        actionMode = 1;
        actionButton = e_ButtonFunction_KeeperRush;
      }

    } else if (_IsKeeperHands()) {

      // Keeper distribution (spec §8, #32). The hand throw (ShortPass) charges and plays the throw
      // clip; the directed clear (HighPass) charges and arms a kick; the centre clear (Shot) drops
      // the ball and arms immediately; "to feet" (LongPass) just drops the ball.
      if (hid->GetButton(e_ButtonFunction_ShortPass) && !hid->GetPreviousButtonState(e_ButtonFunction_ShortPass)) {
        actionMode = 2;
        actionButton = e_ButtonFunction_ShortPass;
      }

      if (hid->GetButton(e_ButtonFunction_HighPass) && !hid->GetPreviousButtonState(e_ButtonFunction_HighPass)) {
        actionMode = 2;
        actionButton = e_ButtonFunction_HighPass;
      }

      if (hid->GetButton(e_ButtonFunction_Shot) && !hid->GetPreviousButtonState(e_ButtonFunction_Shot)) {
        match->KeeperDropToFeet(team->GetID());
        keeperClearArmed = true;
        keeperClearButton = e_ButtonFunction_Shot;
        keeperClearCharge = 1.0f;
        keeperClearArmedTime_ms = match->GetActualTime_ms();
      }

      if (hid->GetButton(e_ButtonFunction_LongPass) && !hid->GetPreviousButtonState(e_ButtonFunction_LongPass)) {
        match->KeeperDropToFeet(team->GetID());
      }

    } else if (keeperClearArmed) {

      // Ball dropped, waiting for it to settle at the keeper's feet; RequestCommand issues the kick.

    } else {

      bool allowShortPass = true;
      bool allowLongPass = true;
      bool allowHighPass = true;
      bool allowShot = true;

      if (team->GetController()->GetPieceTaker() == player && team->GetController()->GetSetPieceType() == e_SetPiece_ThrowIn) {
        allowHighPass = false;
        allowShot = false;
      }

      // Set-piece kick types (spec §6): a free kick offers all four, a corner and a goal kick only
      // the two passes.
      if (_IsSetPieceKicker()) {
        e_SetPiece setPiece = team->GetController()->GetSetPieceType();
        if (setPiece == e_SetPiece_Corner || setPiece == e_SetPiece_GoalKick) {
          allowLongPass = false;
          allowShot = false;
        }
      }

      if (hid->GetButton(e_ButtonFunction_ShortPass) && !hid->GetPreviousButtonState(e_ButtonFunction_ShortPass) && allowShortPass) {
        actionMode = 2;
        actionButton = e_ButtonFunction_ShortPass;
      }

      if (hid->GetButton(e_ButtonFunction_LongPass) && !hid->GetPreviousButtonState(e_ButtonFunction_LongPass) && allowLongPass) {
        actionMode = 2;
        actionButton = e_ButtonFunction_LongPass;
      }

      if (hid->GetButton(e_ButtonFunction_HighPass) && !hid->GetPreviousButtonState(e_ButtonFunction_HighPass) && allowHighPass) {
        actionMode = 2;
        actionButton = e_ButtonFunction_HighPass;
      }

      if (hid->GetButton(e_ButtonFunction_Shot) && !hid->GetPreviousButtonState(e_ButtonFunction_Shot) && allowShot) {
        actionMode = 2;
        actionButton = e_ButtonFunction_Shot;
        // chip is not used on a set piece (spec §6): the kick is always a plain shot
        pendingShotType = _IsSetPieceKicker() ? e_ShotType_Normal : _SampleShotType();
      }

    }

  }

  if (actionMode == 2) {
    if (hid->GetButton(actionButton)) {
      gauge_ms += 10;
      gauge_ms = clamp(gauge_ms, 10, 1000);
      actionBufferTime_ms = 0;
    } else {
      // button released, stay in this actionMode until actionBufferTime_ms becomes too big
      actionBufferTime_ms += 10;
    }
  }

  if (hid->GetButton(e_ButtonFunction_Special) && hasPossession) team->GetController()->ApplyAttackingRun();

  _UpdatePenaltyAim();
  _UpdateSetPieceAim();

}

Vector3 HumanController::GetDirection() {
  Vector3 direction = CastPlayer()->GetDirectionVec();
  return hid->GetDirection().GetNormalized(direction);
}

float HumanController::GetFloatVelocity() {
  Vector3 rawInputDirection;
  float rawInputVelocityFloat = 0;
  _GetHidInput(rawInputDirection, rawInputVelocityFloat);
  return rawInputVelocityFloat;
}

int HumanController::GetReactionTime_ms() {
  return IController::GetReactionTime_ms(); // already have human reaction time to contend with
}

void HumanController::Reset() {
  actionMode = 0;
  gauge_ms = 0;
  actionButton = e_ButtonFunction_ShortPass;
  actionBufferTime_ms = 0;
  pendingShotType = e_ShotType_Normal;

  keeperClearArmed = false;
  keeperClearButton = e_ButtonFunction_Shot;
  keeperClearCharge = 0.0f;
  keeperClearAim = Vector3(-1, 0, 0);
  keeperClearArmedTime_ms = 0;

  lastSprintTapTime_ms = -100000;
  knockOnArmed = false;
  knockOnExpireTime_ms = 0;

  lastSwitchTime_ms = -10000;
  lastSwitchTimeDuration_ms = 300;

  lastSteadyDirectionSnapshotTime_ms = 0;
  steadyDirection = Vector3(0, -1, 0);
  previousDirection = Vector3(0, -1, 0);

  fadingTeamPossessionAmount = 1.0;
}

bool HumanController::_IsPenaltyTaker() {
  // Compare against the taker directly, not via GetRole(): the role requires a *locally* controlled
  // player, but on the host the taker may be the remote client's player (a HumanController bound to
  // its NetHIDDevice). The aim and the planned shot must work for any human taker.
  SetPiecePresentation *presentation = match->GetSetPiecePresentation();
  return presentation->GetType() == e_SetPiece_Penalty &&
         presentation->GetTaker() == CastPlayer();
}

void HumanController::_UpdatePenaltyAim() {
  // The human taker drives the aim; drawing is separate (local side only) and cleanup for a
  // lost/changed taker is done once per tick by SetPiecePresentation::Process().
  if (!_IsPenaltyTaker()) return;

  SetPiecePresentation *presentation = match->GetSetPiecePresentation();
  Vector3 stick = hid->GetDirection();
  bool hasInput = stick.GetLength() >= analogStickDeadzone;
  bool shotPressed = (actionMode == 2 && actionButton == e_ButtonFunction_Shot);
  presentation->UpdatePenaltyAim(stick.coords[0], stick.coords[1], hasInput, shotPressed, GetChargeRatio(), match->GetActualTime_ms());
}

bool HumanController::_IsSetPieceKicker() {
  // Free kick, corner or goal kick, and this player is the actual taker. Compare against the taker
  // directly (not the local role): on the host the taker may be the remote client's player.
  if (!match->IsInSetPiece()) return false;
  return team->GetController()->GetPieceTaker() == player &&
         setpiecelogic::SetPieceAimingUsed(team->GetController()->GetSetPieceType());
}

void HumanController::_UpdateSetPieceAim() {
  if (!_IsSetPieceKicker()) return;

  // Raw stick axis (GetDirection is normalized and already deadzoned): the aim turns on its own
  // 0.15 deadzone (spec §6), so small stick drift below it neither turns nor is fed as input.
  float rawX = hid->GetButtonValue(e_ButtonFunction_Right) - hid->GetButtonValue(e_ButtonFunction_Left);
  bool hasInput = fabs(rawX) >= _default_SetPiece_AimDeadzone;
  bool charging = (actionMode == 2);
  match->GetSetPiecePresentation()->UpdateSetPieceAim(rawX, hasInput, charging, match->GetActualTime_ms());
}

void HumanController::_SetPieceKickCommand(PlayerCommandQueue &commandQueue) {
  SetPiecePresentation *presentation = match->GetSetPiecePresentation();

  e_FunctionType functionType;
  switch (actionButton) {
    case e_ButtonFunction_ShortPass: functionType = e_FunctionType_ShortPass; break;
    case e_ButtonFunction_LongPass:  functionType = e_FunctionType_LongPass; break;
    case e_ButtonFunction_HighPass:  functionType = e_FunctionType_HighPass; break;
    case e_ButtonFunction_Shot:      functionType = e_FunctionType_Shot; break;
    default: return;
  }

  float rawY = hid->GetButtonValue(e_ButtonFunction_Up) - hid->GetButtonValue(e_ButtonFunction_Down);
  setpiecelogic::SetPieceKickPlan plan;
  if (!presentation->PlanSetPieceKick(functionType, GetChargeRatio(), rawY, plan)) return;

  PlayerCommand command;
  command.desiredFunctionType = functionType;
  command.useDesiredMovement = false;
  command.useDesiredLookAt = false;

  if (functionType == e_FunctionType_Shot) {
    command.touchInfo.desiredDirection = plan.desiredDirection;
    command.touchInfo.desiredPower = plan.desiredPower;
    command.touchInfo.shotType = e_ShotType_Normal; // chip is not used on a set piece
    command.touchInfo.curl = plan.curl;
    command.touchInfo.useSetPieceLaunch = plan.useLaunch;
    command.touchInfo.setPieceLaunch = plan.launch;
  } else {
    // The aimed heading and the hold are authoritative; plan height (stick-Y) and curl feed the
    // touch. AI_GetPass still picks the teammate for the control handoff.
    command.touchInfo.inputDirection = plan.desiredDirection;
    command.touchInfo.inputPower = plan.desiredPower;
    command.touchInfo.desiredDirection = plan.desiredDirection;
    command.touchInfo.desiredPower = plan.desiredPower;
    command.touchInfo.autoDirectionBias = 0.0f;
    command.touchInfo.autoPowerBias = 0.0f;
    command.touchInfo.aimHeight = plan.aimHeight;
    command.touchInfo.useAimHeight = plan.useAimHeight;
    command.touchInfo.curl = plan.curl;
    Vector3 ignoredDirection;
    float ignoredPower;
    AI_GetPass(CastPlayer(), functionType, plan.desiredDirection, plan.desiredPower, 0.0f, 0.0f, ignoredDirection, ignoredPower, command.touchInfo.targetPlayer);
  }

  commandQueue.push_back(command);
}

bool HumanController::_IsKeeperHands() {
  return CastPlayer()->GetFormationEntry().role == e_PlayerRole_GK &&
         match->GetKeeperState(team->GetID()) == e_KeeperState_Hands &&
         match->GetBallRetainer() == CastPlayer();
}

bool HumanController::_IsKeeperOutfield() {
  return CastPlayer()->GetFormationEntry().role == e_PlayerRole_GK &&
         match->GetKeeperState(team->GetID()) == e_KeeperState_Outfield;
}

float HumanController::_KeeperChargeRatio() const {
  return clamp((float)gauge_ms / (float)keeperDistChargeMax_ms, 0.0f, 1.0f);
}

void HumanController::_KeeperDistributionCommand(PlayerCommandQueue &commandQueue) {
  float chargeRatio = _KeeperChargeRatio();

  // Outfield teammates in front of the keeper, scored by direction x distance band (spec §8.8).
  std::vector<Vector3> candidates;
  std::vector<Player*> candidatePlayers;
  std::vector<Player*> players;
  team->GetActivePlayers(players);
  for (unsigned int i = 0; i < players.size(); i++) {
    Player *mate = players.at(i);
    if (mate == CastPlayer() || mate->GetFormationEntry().role == e_PlayerRole_GK) continue;
    candidates.push_back(mate->GetPosition());
    candidatePlayers.push_back(mate);
  }

  Vector3 aim = inputDirection.Get2D().GetNormalized(CastPlayer()->GetDirectionVec().Get2D());
  int idx = keeperlogic::SelectDistributionTarget(candidates, CastPlayer()->GetPosition(), aim,
                                                  keeperHandRollDist, keeperHandThrowDist, chargeRatio);
  Player *target = (idx >= 0) ? candidatePlayers.at(idx) : 0;

  // Hand throw (ShortPass): tap = low roll to the nearest, hold = overhand throw to the furthest.
  // Uses the existing throw clip, so the ball stays in the hands until the release frame.
  bool isThrow = gauge_ms >= keeperHandThrowCharge_ms;

  PlayerCommand command;
  command.desiredFunctionType = e_FunctionType_ShortPass;
  command.useDesiredMovement = false;
  command.useDesiredLookAt = false;
  command.touchInfo.targetPlayer = target;
  command.touchInfo.forcedTargetPlayer = target;
  command.touchInfo.autoDirectionBias = 0.0f;
  command.touchInfo.autoPowerBias = 0.0f;

  float band = isThrow ? keeperHandThrowDist : keeperHandRollDist;
  Vector3 direction = aim;
  float distance = clamp(0.3f + chargeRatio * 0.7f, 0.2f, 1.0f) * band;
  if (target) {
    Vector3 toTarget = (target->GetPosition() - CastPlayer()->GetPosition()).Get2D();
    distance = toTarget.GetLength();
    direction = toTarget.GetNormalized(aim);
  }
  command.touchInfo.inputDirection = direction;
  command.touchInfo.desiredDirection = direction;

  if (isThrow) {
    // Overhand: ballistic arc peaking at keeperHandThrowPeak; horizontal speed lands the ball on
    // the target within the flight time (the pass touch scales it as |v| = 36 * (power + 0.3)).
    float vz = std::sqrt(2.0f * _default_Shot_Gravity * keeperHandThrowPeak);
    float flightTime = 2.0f * vz / _default_Shot_Gravity;
    float horizontalSpeed = distance / std::max(flightTime, 0.1f);
    float power = clamp(horizontalSpeed / 36.0f - 0.3f, 0.05f, 1.0f);
    command.touchInfo.inputPower = power;
    command.touchInfo.desiredPower = power;
    command.touchInfo.useAimHeight = true;
    command.touchInfo.aimHeight = keeperHandThrowPeak;
  } else {
    // Low roll: the ground pass auto-assist (AI_GetPass on the forced target) sets the pace.
    float power = clamp(distance / keeperHandRollDist, 0.1f, 1.0f);
    command.touchInfo.inputPower = power;
    command.touchInfo.desiredPower = power;
  }

  commandQueue.push_back(command);

  // Hand off control to the addressee immediately (spec §8.7, #32).
  if (target) team->SelectPlayer(target);
}

void HumanController::_KeeperClearKickCommand(PlayerCommandQueue &commandQueue) {
  if (keeperClearButton == e_ButtonFunction_Shot) {
    // Foot-to-centre: a normal shot whose planned launch is the centre clearance (spec §8.4). The
    // ball is already at the keeper's feet from the drop; forceTouch guards the contact.
    Vector3 direction(-team->GetSide(), 0, 0); // toward the centre of the pitch
    PlayerCommand command;
    command.desiredFunctionType = e_FunctionType_Shot;
    command.useDesiredMovement = false;
    command.useDesiredLookAt = false;
    command.desiredVelocityFloat = 0.0f;
    command.touchInfo.inputDirection = direction;
    command.touchInfo.desiredDirection = direction;
    command.touchInfo.autoDirectionBias = 0.0f;
    command.touchInfo.shotType = e_ShotType_Normal;
    command.touchInfo.useSetPieceLaunch = true;
    command.touchInfo.setPieceLaunch = direction * keeperClearSpeed + Vector3(0, 0, keeperClearLift);
    command.touchInfo.forceTouch = true;
    commandQueue.push_back(command);
    return;
  }

  // Directed foot clear (HighPass): stick direction, charge sets the pace, the addressee gets the
  // handoff (spec §8.4/§8.7). The addressee is picked now, once the ball has settled.
  std::vector<Vector3> candidates;
  std::vector<Player*> candidatePlayers;
  std::vector<Player*> players;
  team->GetActivePlayers(players);
  for (unsigned int i = 0; i < players.size(); i++) {
    Player *mate = players.at(i);
    if (mate == CastPlayer() || mate->GetFormationEntry().role == e_PlayerRole_GK) continue;
    candidates.push_back(mate->GetPosition());
    candidatePlayers.push_back(mate);
  }
  int idx = keeperlogic::SelectDistributionTarget(candidates, CastPlayer()->GetPosition(), keeperClearAim,
                                                  keeperHandRollDist, keeperHandThrowDist, keeperClearCharge);
  Player *target = (idx >= 0) ? candidatePlayers.at(idx) : 0;

  PlayerCommand command;
  command.desiredFunctionType = e_FunctionType_HighPass;
  command.useDesiredMovement = false;
  command.useDesiredLookAt = false;
  command.touchInfo.inputDirection = keeperClearAim;
  command.touchInfo.inputPower = clamp(0.2f + keeperClearCharge * 0.6f, 0.2f, 0.8f);
  command.touchInfo.desiredDirection = keeperClearAim;
  command.touchInfo.desiredPower = command.touchInfo.inputPower;
  command.touchInfo.autoDirectionBias = 0.0f;
  command.touchInfo.autoPowerBias = 0.0f;
  command.touchInfo.aimHeight = keeperKickLoft;
  command.touchInfo.useAimHeight = true;
  command.touchInfo.targetPlayer = target;
  command.touchInfo.forcedTargetPlayer = target;
  command.touchInfo.forceTouch = true;
  commandQueue.push_back(command);
  if (target) team->SelectPlayer(target); // hand off to the addressee
}

float HumanController::GetChargeRatio() const {
  if (actionMode != 2) return 0.0f;
  int baseTime_ms = 60; // substract a little because we can't really press a button shorter than this
  float gaugeScale_ms = (actionButton == e_ButtonFunction_Shot) ? KICK_CHARGE_MAX_TIME * 1000.0f : 1000.0f;
  return clamp((gauge_ms - baseTime_ms) * (1.0f / (gaugeScale_ms - baseTime_ms)), 0.0f, 1.0f);
}

e_ShotType HumanController::_SampleShotType() {
  bool curlHeld = hid->GetButton(e_ButtonFunction_Dribble); // R2 / C
  bool chipHeld = hid->GetButton(e_ButtonFunction_Switch);  // L1 / Q
  if (curlHeld && !chipHeld) return e_ShotType_Curl;
  if (chipHeld && !curlHeld) return e_ShotType_Chip;
  return e_ShotType_Normal; // no modifier, or both held -> plain shot
}

void HumanController::_GetHidInput(Vector3 &rawInputDirection, float &rawInputVelocityFloat) {
  rawInputDirection = hid->GetDirection();

  if (rawInputDirection.GetLength() < analogStickDeadzone) {
    rawInputDirection = CastPlayer()->GetDirectionVec();
    rawInputVelocityFloat = idleVelocity;
  } else {
    if (hid->GetButton(e_ButtonFunction_Sprint)) rawInputVelocityFloat = sprintVelocity;
    else if (hid->GetButton(e_ButtonFunction_Dribble)) rawInputVelocityFloat = dribbleVelocity;
    else if (hid->GetButton(e_ButtonFunction_Special) && match->GetDesignatedPossessionPlayer() == CastPlayer()) rawInputVelocityFloat = idleVelocity;
    else rawInputVelocityFloat = walkVelocity;
    assert(rawInputDirection.GetLength() > 0.001f);
    rawInputDirection.Normalize(); // hid should do this, but still
  }

  // Holding the ball, the keeper runs at a fixed share of the top speed (spec §8.4, #32).
  if (_IsKeeperHands()) rawInputVelocityFloat = std::min(rawInputVelocityFloat, keeperHandsVelocityFloat * sprintVelocity);

  if (GetLastSwitchBias() > 0.0f) {
    float switchInfluence = 0.5f;
    float switchBias = pow(GetLastSwitchBias(), 0.7f);
    Vector3 currentMovement = player->GetDirectionVec() * player->GetFloatVelocity();
    Vector3 manualMovement = rawInputDirection * rawInputVelocityFloat;
    Vector3 resultMovement = currentMovement * switchBias * switchInfluence +
                             manualMovement * (1.0f - switchBias * switchInfluence);
    rawInputDirection = resultMovement.GetNormalized(rawInputDirection);
    rawInputVelocityFloat = resultMovement.GetLength();
  }

}

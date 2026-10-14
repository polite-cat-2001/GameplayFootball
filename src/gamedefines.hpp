// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#ifndef _HPP_GAMEDEFINES
#define _HPP_GAMEDEFINES

#include "defines.hpp"

#include "base/math/vector3.hpp"

#include <SDL3/SDL.h> // for key ids

using namespace blunted;

extern unsigned long time_ms;

// Expected SQLite schema version of the game data database, stored via
// `PRAGMA user_version` (and in manifest.json as schema_version) by
// tm-gf-import. Bump together with the game's CREATE TABLE schema in
// src/menu/mainmenu.cpp; the game rejects an incompatible database at startup.
const int databaseSchemaVersion = 3;

const float idleVelocity = 0.0f;
const float dribbleVelocity = 3.5f;
const float walkVelocity = 5.0f;
const float sprintVelocity = 8.0f;

const float animSprintVelocity = 7.0f;

const float idleDribbleSwitch = 1.8f;
const float dribbleWalkSwitch = 4.2f;
const float walkSprintSwitch = 6.0f;
// PES6 digital control mode, quantizes some input to x degree angles
const bool quantizeDirection = true;

const float analogStickDeadzone = 0.3f;

// Max gap between two Sprint presses that counts as a double-tap; a single press
// stays a plain sprint (see HumanController).
const int sprintDoubleTapWindow_ms = 400;
// How long a double-tap keeps feeding the knock-on modifier to ball touches,
// long enough to span one ball-control animation and its touch frame.
const int sprintKnockOnWindow_ms = 600;

const bool defaultControllerSimpleMode = false; // reserved: simple mode for sticks-less gamepads is a future feature (see docs/wiki/открытые-вопросы.md)

const float _default_CameraZoom = 0.5f;
const float _default_CameraHeight = 0.3f;
const float _default_CameraFOV = 0.4f;
const float _default_CameraAngleFactor = 0.0f;

const float _default_Difficulty = 0.6f;
const float _default_MatchDuration = 0.4f;

const float _default_QuantizedDirectionBias = 0.0f;

const float _default_AgilityFactor = 0.5f;
const float _default_AccelerationFactor = 0.5f;

const float _default_ShortPass_AutoDirection = 0.4f;
const float _default_ShortPass_AutoPower = 0.7f;
const float _default_ThroughPass_AutoDirection = 0.2f;
const float _default_ThroughPass_AutoPower = 0.7f;
const float _default_HighPass_AutoDirection = 0.2f;
const float _default_HighPass_AutoPower = 0.5f;
const float _default_Shot_AutoDirection = 0.74f;

// Curl/chip shot feel and ballistic launch (spec 2026-09-29 §7). Tune by playing.
const float _default_Shot_Curl_ZRot = 90.0f;        // lateral spin magnitude for a curled shot
const float _default_Shot_Curl_AimOut = 0.1f;       // radians to aim outside the target so the curve bends back in
const float _default_Shot_Curl_ZRotMax = 300.0f;    // slider range for curl spin
const float _default_Shot_Curl_AimOutMax = 0.3f;    // slider range for curl aim offset (radians)
const float _default_Shot_Curl_SpeedFactor = 0.85f; // finesse trades some power for placement/curl
const float _default_Shot_Chip_Angle = 30.0f * pi / 180.0f; // chip launch angle (radians); steeper than a normal shot
const float _default_Shot_Chip_SpeedFactor = 0.45f; // chip total speed as a fraction of the shot power
const float _default_Shot_AimYMin = 0.8f;           // aim height at zero charge (m)
const float _default_Shot_OverLift = 1.0f;          // full charge may aim this far above the crossbar (m)
const float _default_Shot_GroundChargeMax = 0.35f;  // below this charge (desiredPower scale) the shot is driven flat
const float _default_Shot_GroundAimY = 0.25f;       // aim height for a driven flat shot (m)
const float _default_Shot_MaxLift = 12.0f;          // cap on vertical launch speed (m/s); high enough to clear the bar and miss high
const float _default_Shot_GroundPower = 60.0f;      // driven low shot gets at least this horizontal speed (m/s)
const float _default_Shot_Gravity = 9.81f;          // vertical ballistic acceleration (m/s^2)
const float _default_Shot_AimLateralMargin = 1.5f;  // aim may overshoot the posts by this many goal half-widths
// Shots charge over this time; other actions keep the 1 s gauge (HumanController).
const float KICK_CHARGE_MAX_TIME = 0.5f;            // seconds

// Penalty reticle (spec 2026-09-29 §7): a 2D aim point on the attacker's goal plane, driven
// by the stick, with a charge-scaled spread disc. Tune by playing.
const float _default_Pen_ReticleSpeed = 6.0f;    // reticle travel speed across the goal plane (m/s)
const float _default_Pen_ReticleReturn = 12.0f;  // how fast the idle reticle returns to centre (1/s)
const float _default_Pen_ReticleStartY = 1.2f;   // initial reticle height above the ground (m)
const float _default_Pen_AimOverhang = 0.6f;     // the reticle may aim this far beyond post/crossbar (m)
const float _default_Pen_SpreadMinR = 0.15f;     // spread disc radius at zero charge (m)
const float _default_Pen_SpreadMaxR = 1.6f;      // spread disc radius at full charge (m)
const float _default_Pen_PowerMinSpeed = 24.0f;  // ball launch speed at zero charge (m/s)
const float _default_Pen_PowerMaxSpeed = 44.0f;  // ball launch speed at full charge (m/s)

// Set-piece cameras (spec 2026-09-29 §4). A presentation-only camera behind the taker for the
// penalty, free kick, corner and goal kick; throw-ins and kickoffs keep the normal camera. Pose is
// eye = spot - heading*BACK + (0,0,HEIGHT), look = spot + heading*AHEAD + (0,0,LOOK_Y). Tune by playing.
const float _default_SetPiece_CamAhead = 4.0f;       // look-at lead along the heading (m)
const float _default_SetPiece_CamFov = 45.0f;        // set-piece camera field of view (deg)
const float _default_SetPiece_CamNear = 1.0f;        // near clip plane (m)
const float _default_SetPiece_CamFar = 220.0f;       // far clip plane (m)
const int _default_SetPiece_CamHold_ms = 1500;       // hold after a strike before releasing (ms)
const float _default_SetPiece_CamPenaltyBack = 9.0f; // penalty: distance behind the ball (m)
const float _default_SetPiece_CamPenaltyHeight = 4.0f;
const float _default_SetPiece_CamPenaltyLookY = 1.2f;
const float _default_SetPiece_CamFreeKickBack = 8.0f; // free kick
const float _default_SetPiece_CamFreeKickHeight = 3.5f;
const float _default_SetPiece_CamFreeKickLookY = 1.4f;
const float _default_SetPiece_CamCornerBack = 8.0f;   // corner
const float _default_SetPiece_CamCornerHeight = 4.0f;
const float _default_SetPiece_CamCornerLookY = 2.0f;
const float _default_SetPiece_CamGoalKickBack = 8.0f; // goal kick
const float _default_SetPiece_CamGoalKickHeight = 3.5f;
const float _default_SetPiece_CamGoalKickLookY = 1.4f;

// Set-piece kick aiming and types (spec 2026-09-29 §6). Stick-X turns the world heading of the
// kick around the ball (a full turn on a free kick, a forward sector on a corner/goal kick) and
// also feeds the curl accumulator; stick-Y sets the launch height of a pass/cross (stick down
// scoops the ball up, stick up drives it lower), while a free-kick shot derives both speed and
// elevation from the charge. Tune by playing.
const float _default_SetPiece_AimSpeed = 1.0f;               // heading turn rate (rad/s)
const float _default_SetPiece_AimDeadzone = 0.15f;           // aim stick deadzone
const float _default_SetPiece_FreeKickAimArc = pi;           // free kick: full turn (rad)
const float _default_SetPiece_CornerAimArc = 1.4f;           // corner: forward sector half-angle (CORNER_AIM_ARC, rad)
const float _default_SetPiece_GoalKickAimArc = 1.2f;         // goal kick: forward sector half-angle (GK_AIM_ARC, rad)
const float _default_SetPiece_GoalMagnet = 0.18f;            // free-kick shot pull toward the goal centre (goal_magnet)
const float _default_SetPiece_FreeKickShotSpeedMin = 36.0f;  // free-kick shot launch speed at zero charge (m/s)
const float _default_SetPiece_FreeKickShotSpeedMax = 46.0f;  // free-kick shot launch speed at full charge (m/s)
const float _default_SetPiece_FreeKickShotElevMin = 4.0f * pi / 180.0f;  // free-kick shot elevation at zero charge (rad)
const float _default_SetPiece_FreeKickShotElevMax = 22.0f * pi / 180.0f; // free-kick shot elevation at full charge (rad)
const float _default_SetPiece_PassHeightMax = 5.0f;          // stick-down full loft peak height on a pass/cross (m)
const float _default_SetPiece_PassHeightMin = 1.0f;          // a cross never drops below the waist (m)
const float _default_SetPiece_CurlScale = 1.0f;              // stick-X accumulation to curl gain
const float _default_SetPiece_CurlMax = 1.0f;                // |curl| cap
const float _default_SetPiece_CurlSpin = 220.0f;             // lateral spin (rad/s) a full curl puts on a pass/cross

// Set-piece taker placement: how far behind the ball the taker stands, and the lateral offset to
// the taker's kicking-foot side (right-footed -> left of the shooting line, left-footed -> right),
// so the strong foot swings into the ball. Used on free kick, corner, goal kick and penalty.
const float _default_SetPiece_RunupDist = 2.2f;          // taker stands this far behind the ball (m)
const float _default_SetPiece_RunupFootLateral = 0.8f;   // lateral shift to the kicking-foot side (m)

// Goalkeeper layer (spec 2026-09-29 §8, #31). The keeper is hard-clamped to his own penalty
// area while he holds the ball (Hands): `keeperBoxDepth` metres from his goal line,
// `keeperBoxHalfWidth` either side of the centre. Shared with the keeper deflect check
// (`PlayerController::_KeeperDeflectCommand`) and the ball-retainer box nudge (`Humanoid`).
const float keeperBoxDepth = 16.4f;
const float keeperBoxHalfWidth = 20.05f;
// Tolerance (m) for box-boundary tests and for skipping a negligible clamp correction.
const float keeperBoxEpsilon = 0.01f;

// Keeper distribution (spec 2026-09-29 §8, #32): the four Hands actions, the 6-second rule and
// the hand throw / foot clear ballistics. The hand roll and throw target bands, the tap/hold
// threshold and the charge window live here; the centre clear speed/lift define the forced kick.
const int keeperSixSecond_ms = 6000;           // catch -> ball put down at the keeper's feet timeout
const int keeperDistChargeMax_ms = 700;        // full charge of a keeper distribution
const int keeperHandThrowCharge_ms = 200;      // hold >= this = overhand throw, below = low roll
const float keeperHandRollDist = 12.0f;        // low roll target distance band (m)
const float keeperHandThrowDist = 30.0f;       // overhand throw target distance band (m)
const float keeperHandThrowPeak = 2.5f;        // overhand throw arc peak height (m)
const float keeperHandsVelocityFloat = 0.85f;  // movement speed while holding, as a share of sprint
const float keeperKickLoft = 5.0f;             // foot clear arc peak height (m)
const float keeperCenterClearPower = 1.0f;     // fixed power of the foot-to-centre clear (0..1)
// Grace after a chip to feet (or a foot drop) during which the keeper stays in Outfield while the
// ball is still falling out of his hands (HasPossession is briefly false).
const int keeperDropGrace_ms = 1000;

// Penalty defense (spec 2026-09-29 §9, #33). The keeper stays under AI; on the taker's touch the
// defending human's raw stick X+Y picks one of the dive zones (three height bands per side, plus a
// centre band). The dive plays a far deflect clip while the body is driven at the zone. All values
// in metres / metres-per-second.
// The dive is a committed lunge: the body is driven at the zone by a direct positional slide, so it
// is still there when the ball arrives.
const int penaltyKeeperDiveDuration_ms = 900;
const float penaltyKeeperDiveSpeed = 4.5f;         // positional lunge speed toward the zone (m/s)
const float penaltyKeeperSaveRadius = 0.8f;        // ball within this of the diving keeper is saved (m)
// Dive-target geometry: the side zones aim at the reach of the far-dive clips
// (`090_ground_deflect_far` / `090_midheight_deflect_far` / `090_high_deflect_far` in deflect/idle),
// which are the only deflect anims whose touch frame is late enough for a struck penalty.
// The dive target is the near post of the chosen half, so a correct side guess covers the whole
// side. The bands differ only in the clip/pose; the body reaches the same width.
const float penaltyKeeperDiveLateral = 3.2f;       // positional target lateral of a side zone (m)
// Clip-selection point: the far-dive clips (`090_ground_deflect_far` 2.53/0.11,
// `090_midheight_deflect_far` 1.94/0.94, `090_high_deflect_far` 1.26/2.09) are the only deflect
// anims whose touch frame is late enough for a struck penalty. The clip target follows the body
// lateral but is pushed outward by this margin (capped at the far edge) so the animator still picks
// that far lunge instead of a close catch.
const float penaltyKeeperDiveClipLateralLow = 2.5f;
const float penaltyKeeperDiveClipLateralMid = 1.94f;
const float penaltyKeeperDiveClipLateralHigh = 1.26f;
const float penaltyKeeperDiveClipBias = 0.6f;      // outward push of the clip point past the body (m)
const float penaltyKeeperDiveLowY = 0.11f;         // representative height of a low side zone (m)
const float penaltyKeeperDiveMidY = 0.94f;         // representative height of a mid side zone (m)
const float penaltyKeeperDiveHighY = 2.09f;        // representative height of a high side zone (m)
const float penaltyKeeperMidThreshold = 0.8f;      // crossing height above which a zone is no longer LOW
const float penaltyKeeperHighThreshold = 1.6f;     // crossing height above which a zone reads HIGH
const float penaltyKeeperCenterBand = 0.8f;        // |lateral| within this reads CENTER
const float penaltyKeeperStickDeadzone = 0.15f;    // stick length below which the pick is CENTER
// AI zone weights (symmetric): low 17% per side, mid 15.5% per side, high 15.5% per side,
// centre 4% (sums to 1).
const float penaltyKeeperZoneWeightLow = 0.17f;
const float penaltyKeeperZoneWeightMid = 0.155f;
const float penaltyKeeperZoneWeightHigh = 0.155f;
const float penaltyKeeperZoneWeightCenter = 0.04f;

const float distanceToVelocityMultiplier = 2.6f; // for example: when we need to travel 4 meters, we need to go at velo 4 * distanceToVelocityMultiplier

const unsigned int ballPredictionSize_ms = 3000;
const unsigned int ballHistorySize_ms = 4000;

const float ballDistanceOptimizeThreshold = 10.0f;

const int playerNum = 11;

// In-match substitutions allowed per team (extra time / extra windows not modelled yet).
const int maxSubstitutions = 5;

// how far into an animation the ball is usually touched
const unsigned int defaultTouchOffset_ms = 80;

const float defaultPlayerHeight = 1.92f;

const int temporalSmoother_history_ms = 20;

//#define dataSetSortable 1
#ifdef dataSetSortable
typedef std::list<int> DataSet;
#else
typedef std::deque<int> DataSet;
#endif

const SDL_Keycode defaultKeyIDs[18] = { SDLK_UP, SDLK_RIGHT, SDLK_DOWN, SDLK_LEFT, SDLK_W, SDLK_A, SDLK_S, SDLK_D, SDLK_W, SDLK_A, SDLK_S, SDLK_D, SDLK_Q, SDLK_Z, SDLK_E, SDLK_C, SDLK_TAB, SDLK_RETURN };

class Player;

enum e_Side {
  e_Side_Left,
  e_Side_Right
};

enum e_Velocity {
  e_Velocity_Idle,
  e_Velocity_Dribble,
  e_Velocity_Walk,
  e_Velocity_Sprint
};

enum e_FunctionType {
  e_FunctionType_None,
  e_FunctionType_Movement,
  e_FunctionType_BallControl,
  e_FunctionType_Trap,
  e_FunctionType_ShortPass,
  e_FunctionType_LongPass,
  e_FunctionType_HighPass,
  e_FunctionType_Header,
  e_FunctionType_Shot,
  e_FunctionType_Deflect,
  e_FunctionType_Catch,
  e_FunctionType_Interfere,
  e_FunctionType_Trip,
  e_FunctionType_Sliding,
  e_FunctionType_Special
};

enum e_TouchType {
  e_TouchType_Intentional_Kicked, // goalies can't touch this
  e_TouchType_Intentional_Nonkicked, // headers and such
  e_TouchType_Accidental, // collisions
  e_TouchType_None,
  e_TouchType_SIZE
};

enum e_SetPiece {
  e_SetPiece_None,
  e_SetPiece_KickOff,
  e_SetPiece_GoalKick,
  e_SetPiece_FreeKick,
  e_SetPiece_Corner,
  e_SetPiece_ThrowIn,
  e_SetPiece_Penalty,
};

enum e_MatchPhase {
  e_MatchPhase_PreMatch,
  e_MatchPhase_1stHalf,
  e_MatchPhase_2ndHalf,
  e_MatchPhase_1stExtraTime,
  e_MatchPhase_2ndExtraTime,
  e_MatchPhase_Penalties,
};

enum e_PlayerCommandModifier {
  e_PlayerCommandModifier_None = 0,
  e_PlayerCommandModifier_KnockOn = 1
};

enum e_ShotType {
  e_ShotType_Normal,
  e_ShotType_Curl,
  e_ShotType_Chip
};

class IController;

struct TouchInfo {

  TouchInfo() {
    inputPower = 0;
    autoDirectionBias = 0;
    autoPowerBias = 0;
    targetPlayer = 0;
    forcedTargetPlayer = 0;
    desiredPower = 0;
    shotType = e_ShotType_Normal;
    aimHeight = 0;
    useAimHeight = false;
    curl = 0;
    useSetPieceLaunch = false;
    setPieceLaunch = Vector3(0);
    useAimTarget = false;
    aimLateral = 0;
    aimSpeed = 0;
    forceTouch = false;
    useSyntheticBallTarget = false;
    syntheticBallTarget = Vector3(0);
  }

  Vector3         inputDirection;
  float           inputPower;

  float           autoDirectionBias;
  float           autoPowerBias;

  Vector3         desiredDirection; // inputdirection after pass function
  float           desiredPower;
  Player          *targetPlayer; // null == do not use
  Player          *forcedTargetPlayer; // null == do not use

  e_ShotType      shotType; // curl/chip modifier, sampled when the shot button is pressed

  // Planned launch height on the attacker's goal plane (m), set by the set-piece
  // shot planner; when useAimHeight is false the physics layer derives it from the charge.
  float           aimHeight;
  bool            useAimHeight;

  // Planned curl: a signed amount in -1..+1 (0 = no curl). A set-piece kick accumulates it from
  // the lateral stick; a curled shot uses the full +-1. The spin/aim-out magnitudes come from the
  // gameplay config.
  float           curl;

  // Set-piece free-kick shot (spec 2026-09-29 §6): the planner already derived the 3D launch
  // velocity from the charge (speed and elevation), so GetShotVector uses it as the best case
  // instead of the goal-line ballistic. The accumulated curl still travels in `curl`.
  bool            useSetPieceLaunch;
  Vector3         setPieceLaunch;

  // Penalty reticle (spec 2026-09-29 §7): when set, the shot is launched ballistically at
  // (aimLateral, aimHeight) on the attacker's goal plane at aimSpeed, ignoring the
  // charge-driven aim height, the skill spread and the lateral body curve. Penalty only.
  bool            useAimTarget;
  float           aimLateral; // y offset from the goal centre (m)
  float           aimSpeed;   // horizontal launch speed (m/s)

  // Keeper distribution (#32): a foot kick from the hands lays the ball a fixed distance ahead and
  // forces the touch on the animation's touch frame -- the shot/highpass anims carry no retain
  // state, so the held ball cannot be glued to the animation's contact point. Never set by normal
  // outfield commands.
  bool            forceTouch;

  // Penalty dive (spec 2026-09-29 §9, #33): pick the dive clip from a synthetic ball point at the
  // zone's far edge instead of the real ball, so the deflect animator chooses the far lunge clip.
  bool            useSyntheticBallTarget;
  Vector3         syntheticBallTarget;

};

enum e_StrictMovement {
  e_StrictMovement_False,
  e_StrictMovement_True,
  e_StrictMovement_Dynamic
};

struct PlayerCommand {

  /* specialVar1:

    1: happy celebration
    2: inverse celebration (feeling bad)
    3: referee showing card
  */

  PlayerCommand() {
    desiredFunctionType = e_FunctionType_Movement;
    useDesiredMovement = false;
    desiredVelocityFloat = idleVelocity;
    strictMovement = e_StrictMovement_Dynamic;
    useDesiredLookAt = false;
    useTripType = false;
    useDesiredTripDirection = false;
    onlyDeflectAnimsThatPickupBall = false;
    tripType = 1;
    useSpecialVar1 = false;
    specialVar1 = 0;
    useSpecialVar2 = false;
    specialVar2 = 0;
    modifier = 0;
    forceIncomingVelocityIdle = false;
  }

  e_FunctionType desiredFunctionType;

  bool           useDesiredMovement;
  Vector3        desiredDirection;
  e_StrictMovement strictMovement;

  float          desiredVelocityFloat;

  bool           useDesiredLookAt;
  Vector3        desiredLookAt; // absolute 'look at' position on pitch

  bool           useTouchInfo;
  TouchInfo      touchInfo;

  bool           onlyDeflectAnimsThatPickupBall;

  bool           useTripType;
  int            tripType; // only applicable for trip anims

  bool           useDesiredTripDirection;
  Vector3        desiredTripDirection;

  bool           useSpecialVar1;
  int            specialVar1;
  bool           useSpecialVar2;
  int            specialVar2;

  int            modifier;

  // Forced off-ball Specials (free-kick wall jump, #34): treat the current velocity as idle when
  // matching animation incoming velocity, so an idle-authored clip is selectable while running.
  bool           forceIncomingVelocityIdle;
};

typedef std::vector<PlayerCommand> PlayerCommandQueue;

enum e_PlayerRole {
  e_PlayerRole_GK,
  e_PlayerRole_CB,
  e_PlayerRole_LB,
  e_PlayerRole_RB,
  e_PlayerRole_DM,
  e_PlayerRole_CM,
  e_PlayerRole_LM,
  e_PlayerRole_RM,
  e_PlayerRole_AM,
  e_PlayerRole_CF,
  // Present in Transfermarkt-sourced data and used by formations; modelled here
  // so the tokens are not silently degraded to CM.
  e_PlayerRole_LW,
  e_PlayerRole_RW,
  e_PlayerRole_ST,
};

// Distance (m) from the attacking goal below which a free kick counts as
// "near" and uses the near free-kick taker; farther kicks use the far taker.
const float freeKickNearDistance = 35.0f;

// AI penalty execution (ticket #36).
const unsigned int _default_AI_PenaltyThinkDelay_ms = 1500; // pause after the whistle before the run-up
const float _default_AI_PenaltyCenterProbability = 0.20f;
const float _default_AI_PenaltyRightFootLeftProbability = 0.45f;
const float _default_AI_PenaltyLeftFootRightProbability = 0.51f;
const float _default_AI_PenaltySideLowProbability = 0.55f;
const float _default_AI_PenaltyCenterLowProbability = 0.70f;
const float _default_AI_PenaltySideMiddleProbability = 0.35f;
const float _default_AI_PenaltyCenterMiddleProbability = 0.25f;
const float _default_AI_PenaltyLowAimY = 0.30f;
const float _default_AI_PenaltyPowerThreeQuarterProbability = 0.70f;
const float _default_AI_PenaltyPowerMaximumProbability = 0.18f;
const float _default_AI_PenaltyPowerThreeQuarter = 0.75f;
const float _default_AI_PenaltyPowerMaximum = 1.0f;
const float _default_AI_PenaltyPowerHalf = 0.50f;
const float _default_AI_PenaltySpreadScale = 0.35f;

// Free-kick defensive wall (spec 2026-09-29 §10, #34). The wall stays under AI: it is a row of
// defenders on the ball->near-post line, count scaling with the distance to the goal. No human
// control and no dedicated "jump height" constant -- the forced jump clip sets the height.
const float freeKickWallDistance = 9.15f;  // metre law: wall stands this far from the ball (also the defenders' min distance)
const float freeKickWallFarDist = 40.0f;   // beyond this distance to goal there is no wall
const float freeKickWallNearDist = 25.0f;  // within this distance the wall is at its maximum
const int   freeKickWallMinPlayers = 2;
const int   freeKickWallMaxPlayers = 4;
const float freeKickWallSpacing = 0.62f;   // shoulder-to-shoulder spacing along the row
const float freeKickWallJumpTime = 0.5f;   // jump duration (s); the trigger fires half of it before arrival
const float freeKickWallJumpMinSpeed = 1.0f; // ball must be moving at least this fast (m/s) to count as in flight
const int   freeKickWallJumpSpecialVar1 = 4; // specialvar1 of the forced free-kick wall jump clip

// Free-kick shot-lane clearing: keep a lane around the ball->goal axis free of everyone but the
// taker, the keepers and the wall, so a teammate never stands in the kick line. The lane covers the
// taker's run-up side behind the ball and the cone through the wall edges.
const float _default_FreeKick_LaneMargin = 0.6f;         // extra clearance beyond the lane edge (m)
const float _default_FreeKick_LanePushFactor = 0.5f;     // extra push = this fraction of the ball->wall distance (dimensionless)
const float _default_FreeKick_LaneMinHalfWidth = 1.2f;   // floor so the lane is not razor-thin near the ball (m)
const float _default_FreeKick_LaneRunupHalfWidth = 1.5f; // half width of the corridor behind the ball (m)
const float _default_FreeKick_LaneRunupBack = 5.0f;      // clear this far behind the ball (taker side, m)
const float _default_FreeKick_LaneForward = 30.0f;       // clear this far ahead toward the goal (m)

enum e_TeamRole {
  e_TeamRole_Captain,
  e_TeamRole_PenaltyTaker,
  e_TeamRole_FreeKickTakerNear,
  e_TeamRole_FreeKickTakerFar,
  e_TeamRole_CornerTakerLeft,
  e_TeamRole_CornerTakerRight,
  e_TeamRole_SIZE
};

std::string GetRoleName(e_PlayerRole playerRole);
e_PlayerRole GetRoleFromString(const std::string &roleString);
std::string GetTeamRoleName(e_TeamRole teamRole);

struct FormationEntry {
  e_PlayerRole role;
  Vector3 databasePosition;
  Vector3 position; // adapted to player role (combination of databasePosition and hardcoded role position)
};

struct PlayerImage {
  int teamID;
  signed int side;
  int playerID;
  Player *player;
  Vector3 position;
  Vector3 directionVec;
  Vector3 bodyDirectionVec;
  float velocity;
  Vector3 movement;
  FormationEntry formationEntry;
  FormationEntry dynamicFormationEntry;
};

bool PlayerImageDepthSortFunc(const PlayerImage &a, const PlayerImage &b);

const float pitchHalfW = 55; // only inside side- and backlines
const float pitchHalfH = 36;
const float pitchFullHalfW = 60; // including 'rim'
const float pitchFullHalfH = 40;
const float lineHalfW = 0.06f;

const float goalDepth = 2.55f;
const float goalHeight = 2.5f;
const float goalHalfWidth = 3.7f;
const float _default_AI_PenaltyAimLateral = goalHalfWidth * 0.68f;
const float _default_AI_PenaltyMiddleAimY = goalHeight * 0.50f;
const float _default_AI_PenaltyHighAimY = goalHeight * 0.70f;

// Goal net (NetSim, spec 2026-09-29 §13/#37). Presentation-only: the ball drives the net and
// nothing is fed back. The pure module (src/onthepitch/net/netsim.*) takes these as parameters
// and never reads them itself, so it can run headless. Box dims mirror the GF goal: width 7.4
// (goalHalfWidth * 2), height 2.5, depth 2.55. The divisions are chosen so the cells are square
// (width/35 = 0.211, height/12 = 0.208, depth/12 = 0.213 m), yielding 1274 nodes / 2398 edges /
// 284 pins.
const float NET_DEPTH = 2.55f;
const int NET_WIDTH_DIV = 35;
const int NET_HEIGHT_DIV = 12;
const int NET_DEPTH_DIV = 12;
const float NET_STIFFNESS = 0.18f;
const int NET_CONSTRAINT_ITERATIONS = 2;
const float NET_DAMPING = 0.98f;
const float NET_SHAPE_RETURN = 2.5f;
const float NET_SLACK = 0.9f;
const float NET_GRAVITY = 9.8f;
const float NET_SPRING_STIFFNESS = 400.0f;
const float NET_SIM_STEP = 0.016f;
const float NET_BALL_RADIUS = 0.7f;
const float NET_BALL_VEL_SCALE = 0.45f;
const float NET_BALL_MIN_SPEED = 1.5f;
const float NET_BALL_FORCE = 150.0f;
const float NET_BOUNCE = 0.1f;
const float NET_FRICTION = 1.0f;
const float NET_CELEBRATION_TIME = 5.0f;

enum e_DecayType {
  e_DecayType_Constant,
  e_DecayType_Variable
};

enum e_MagnetType {
  e_MagnetType_Attract,
  e_MagnetType_Repel
};

// forcefields consist of forcespots, representing a repelling or attracting force from a position, including linearity/etc parameters
struct ForceSpot {
  ForceSpot() {
    exp = 1.0f;
  }
  Vector3 origin;
  e_MagnetType magnetType;
  e_DecayType decayType;
  float exp;
  float power;
  float scale; // scaled #meters until effect is almost decimated
};

class PassRating {

  public:
    PassRating(int playerID, float odds, float pos, float sit) : playerID(playerID), odds(odds), pos(pos), sit(sit), rating(0) {}
    virtual ~PassRating() {}

    void CalculateRating(float opportunism) {
      rating = (sit * 1.0f + odds * 1.0f) * 0.5f * (1 - opportunism) +
               pos * opportunism;
    }

    bool operator < (const PassRating &otherPassRating) const {
      return rating < otherPassRating.rating;
    }

    int playerID;

    // 0 .. 1 == worst .. best
    float odds; // what are the odds a pass to this player will complete?
    float pos; // is this player in a good position?
    float sit; // target's situational rating
    float rating; // resulting rating

};

typedef std::vector<PassRating> PassRatings;


void GetVertexColors(std::map<Vector3, Vector3> &colorCoords);

e_FunctionType StringToFunctionType(const std::string &fun);

float GetGlobalVelocityMultiplier();

#endif

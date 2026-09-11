#include "matchsnapshot.hpp"

#include <map>

#include "onthepitch/match.hpp"
#include "onthepitch/ball.hpp"
#include "onthepitch/player/player.hpp"
#include "onthepitch/player/playerbase.hpp"

#include "utils/animation.hpp"

void CapturePlayerPose(PlayerBase *player, int team, int slot, int ownerId, const std::map<Animation*, int> &animIDs, SnapshotPlayer &out) {
  const AnimApplyBuffer &applyBuffer = player->GetAnimApplyBuffer();

  out.team = team;
  out.slot = slot;
  out.ownerId = ownerId;
  std::map<Animation*, int>::const_iterator iter = animIDs.find(applyBuffer.anim);
  out.animID = (iter != animIDs.end()) ? iter->second : -1;
  out.frameNum = applyBuffer.frameNum;
  out.noPos = applyBuffer.noPos;
  out.position = applyBuffer.position;
  out.orientation = applyBuffer.orientation;
}

Snapshot CaptureSnapshot(Match *match) {
  Snapshot snapshot;

  snapshot.matchTime_ms = match->GetMatchTime_ms();
  snapshot.actualTime_ms = match->GetActualTime_ms();
  snapshot.score[0] = match->GetMatchData()->GetGoalCount(0);
  snapshot.score[1] = match->GetMatchData()->GetGoalCount(1);
  snapshot.matchPhase = (int)match->GetMatchPhase();
  snapshot.inPlay = match->IsInPlay();
  snapshot.inSetPiece = match->IsInSetPiece();
  snapshot.pause = match->GetPause();
  snapshot.bestPossessionTeamID = match->GetBestPossessionTeamID();
  snapshot.goalScored = match->IsGoalScored();
  snapshot.goalScoredTimer = match->GetGoalScoredTimer();
  snapshot.message = match->GetSpamMessage();
  snapshot.messageTime_ms = match->GetSpamMessageTime_ms();
  snapshot.messageCounter = match->GetSpamMessageCounter();

  const std::vector<Animation*> &animations = match->GetAnims()->GetAnimations();
  std::map<Animation*, int> animIDs;
  for (unsigned int i = 0; i < animations.size(); i++) {
    animIDs.insert(std::pair<Animation*, int>(animations.at(i), (int)i));
  }

  for (int team = 0; team < 2; team++) {
    std::vector<Player*> all;
    match->GetAllTeamPlayers(team, all);
    for (unsigned int i = 0; i < all.size(); i++) {
      if (!all.at(i)->IsActive()) continue;
      SnapshotPlayer pose;
      int ownerId = match->GetTeam(team)->GetControllingPeerId(all.at(i)->GetID());
      CapturePlayerPose(all.at(i), team, (int)i, ownerId, animIDs, pose);
      snapshot.players.push_back(pose);
    }
  }

  std::vector<PlayerBase*> officials;
  match->GetOfficialPlayers(officials);
  for (unsigned int i = 0; i < officials.size(); i++) {
    SnapshotPlayer pose;
    CapturePlayerPose(officials.at(i), 2, (int)i, -1, animIDs, pose);
    snapshot.officials.push_back(pose);
  }

  snapshot.ballPosition = match->GetBall()->GetStatePosition();
  snapshot.ballOrientation = match->GetBall()->GetStateOrientation();

  match->GetCameraState(snapshot.cameraOrientation, snapshot.cameraNodeOrientation, snapshot.cameraNodePosition,
                        snapshot.cameraFOV, snapshot.cameraNearCap, snapshot.cameraFarCap);

  return snapshot;
}

void WriteSnapshotPlayer(NetBuffer &buffer, const SnapshotPlayer &player) {
  buffer.PutU32((uint32_t)player.team);
  buffer.PutU32((uint32_t)player.slot);
  buffer.PutU32((uint32_t)player.ownerId);
  buffer.PutU32((uint32_t)player.animID);
  buffer.PutU32((uint32_t)player.frameNum);
  buffer.PutBool(player.noPos);
  buffer.PutVector3(player.position);
  buffer.PutFloat(player.orientation);
}

SnapshotPlayer ReadSnapshotPlayer(NetBuffer &buffer) {
  SnapshotPlayer player;
  player.team = (int)buffer.GetU32();
  player.slot = (int)buffer.GetU32();
  player.ownerId = (int)buffer.GetU32();
  player.animID = (int)buffer.GetU32();
  player.frameNum = (int)buffer.GetU32();
  player.noPos = buffer.GetBool();
  player.position = buffer.GetVector3();
  player.orientation = buffer.GetFloat();
  return player;
}

void WriteSnapshot(NetBuffer &buffer, const Snapshot &snapshot) {
  buffer.PutU32((uint32_t)snapshot.matchTime_ms);
  buffer.PutU32((uint32_t)snapshot.actualTime_ms);
  buffer.PutU32((uint32_t)snapshot.score[0]);
  buffer.PutU32((uint32_t)snapshot.score[1]);
  buffer.PutU32((uint32_t)snapshot.matchPhase);
  buffer.PutBool(snapshot.inPlay);
  buffer.PutBool(snapshot.inSetPiece);
  buffer.PutBool(snapshot.pause);
  buffer.PutU32((uint32_t)snapshot.bestPossessionTeamID);
  buffer.PutBool(snapshot.goalScored);
  buffer.PutU32((uint32_t)snapshot.goalScoredTimer);
  buffer.PutU32((uint32_t)snapshot.maxRtt_ms);
  buffer.PutString(snapshot.message);
  buffer.PutU32((uint32_t)snapshot.messageTime_ms);
  buffer.PutU32((uint32_t)snapshot.messageCounter);

  buffer.PutU32((uint32_t)snapshot.players.size());
  for (unsigned int i = 0; i < snapshot.players.size(); i++) {
    WriteSnapshotPlayer(buffer, snapshot.players.at(i));
  }

  buffer.PutU32((uint32_t)snapshot.officials.size());
  for (unsigned int i = 0; i < snapshot.officials.size(); i++) {
    WriteSnapshotPlayer(buffer, snapshot.officials.at(i));
  }

  buffer.PutVector3(snapshot.ballPosition);
  buffer.PutQuaternion(snapshot.ballOrientation);

  buffer.PutQuaternion(snapshot.cameraOrientation);
  buffer.PutQuaternion(snapshot.cameraNodeOrientation);
  buffer.PutVector3(snapshot.cameraNodePosition);
  buffer.PutFloat(snapshot.cameraFOV);
  buffer.PutFloat(snapshot.cameraNearCap);
  buffer.PutFloat(snapshot.cameraFarCap);
}

Snapshot ReadSnapshot(NetBuffer &buffer) {
  Snapshot snapshot;
  snapshot.matchTime_ms = buffer.GetU32();
  snapshot.actualTime_ms = buffer.GetU32();
  snapshot.score[0] = (int)buffer.GetU32();
  snapshot.score[1] = (int)buffer.GetU32();
  snapshot.matchPhase = (int)buffer.GetU32();
  snapshot.inPlay = buffer.GetBool();
  snapshot.inSetPiece = buffer.GetBool();
  snapshot.pause = buffer.GetBool();
  snapshot.bestPossessionTeamID = (int)buffer.GetU32();
  snapshot.goalScored = buffer.GetBool();
  snapshot.goalScoredTimer = buffer.GetU32();
  snapshot.maxRtt_ms = (int)buffer.GetU32();
  snapshot.message = buffer.GetString();
  snapshot.messageTime_ms = (int)buffer.GetU32();
  snapshot.messageCounter = buffer.GetU32();

  uint32_t playerCount = buffer.GetU32();
  snapshot.players.resize(playerCount);
  for (unsigned int i = 0; i < playerCount; i++) {
    snapshot.players.at(i) = ReadSnapshotPlayer(buffer);
  }

  uint32_t officialCount = buffer.GetU32();
  snapshot.officials.resize(officialCount);
  for (unsigned int i = 0; i < officialCount; i++) {
    snapshot.officials.at(i) = ReadSnapshotPlayer(buffer);
  }

  snapshot.ballPosition = buffer.GetVector3();
  snapshot.ballOrientation = buffer.GetQuaternion();

  snapshot.cameraOrientation = buffer.GetQuaternion();
  snapshot.cameraNodeOrientation = buffer.GetQuaternion();
  snapshot.cameraNodePosition = buffer.GetVector3();
  snapshot.cameraFOV = buffer.GetFloat();
  snapshot.cameraNearCap = buffer.GetFloat();
  snapshot.cameraFarCap = buffer.GetFloat();
  return snapshot;
}

void ApplySnapshotPose(PlayerBase *player, const SnapshotPlayer &pose, const std::vector<Animation*> &animTable) {
  player->SetRemoteOwnerId(pose.ownerId);
  if (pose.animID < 0 || pose.animID >= (int)animTable.size()) return;
  Animation *animation = animTable.at(pose.animID);
  if (!animation) return;
  player->SetRemotePose(animation, pose.frameNum, pose.position, (radian)pose.orientation, pose.noPos);
}

int ApplySnapshot(Match *match, const Snapshot &snapshot, const std::vector<Animation*> &animTable) {
  std::vector<Player*> teamPlayers[2];
  match->GetAllTeamPlayers(0, teamPlayers[0]);
  match->GetAllTeamPlayers(1, teamPlayers[1]);
  std::vector<PlayerBase*> officials;
  match->GetOfficialPlayers(officials);

  int applied = 0;
  for (unsigned int i = 0; i < snapshot.players.size(); i++) {
    const SnapshotPlayer &pose = snapshot.players.at(i);
    if (pose.team < 0 || pose.team > 1) continue;
    std::vector<Player*> &all = teamPlayers[pose.team];
    if (pose.slot < 0 || pose.slot >= (int)all.size()) continue;
    if (pose.animID >= 0 && pose.animID < (int)animTable.size() && animTable.at(pose.animID)) {
      ApplySnapshotPose(all.at(pose.slot), pose, animTable);
      applied++;
    }
  }
  for (unsigned int i = 0; i < snapshot.officials.size(); i++) {
    const SnapshotPlayer &pose = snapshot.officials.at(i);
    if (pose.slot < 0 || pose.slot >= (int)officials.size()) continue;
    if (pose.animID >= 0 && pose.animID < (int)animTable.size() && animTable.at(pose.animID)) {
      ApplySnapshotPose(officials.at(pose.slot), pose, animTable);
      applied++;
    }
  }

  match->GetBall()->SetRemoteState(snapshot.ballPosition, snapshot.ballOrientation);
  return applied;
}

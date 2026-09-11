#include "netmatchsession.hpp"

#include "main.hpp"

#include "blunted.hpp"

#include "delayedhiddevice.hpp"
#include "netclient.hpp"
#include "netmessages.hpp"
#include "nethiddevice.hpp"
#include "netserver.hpp"

#include "managers/environmentmanager.hpp"

#include "onthepitch/match.hpp"
#include "onthepitch/team.hpp"

#include "utils/animation.hpp"

namespace {
// Lobby 'device' field: 0 = keyboard, 1 = gamepad. Returns the local HID device
// to bind (host) or to sample for InputFrame (client).
IHIDevice *FindLocalDevice(int deviceType) {
  const std::vector<IHIDevice*> &controllers = GetControllers();
  if (deviceType == 1) {
    for (unsigned int i = 1; i < controllers.size(); i++) {
      if (controllers.at(i)->GetDeviceType() == e_HIDeviceType_Gamepad) return controllers.at(i);
    }
    return 0;
  }
  for (unsigned int i = 0; i < controllers.size(); i++) {
    if (controllers.at(i)->GetDeviceType() == e_HIDeviceType_Keyboard) return controllers.at(i);
  }
  return 0;
}

int GetLocalDeviceType(boost::shared_ptr<NetClient> client) {
  NetLobbyState lobby = client->GetLobbyState();
  uint32_t localId = client->GetPlayerId();
  for (unsigned int i = 0; i < lobby.players.size(); i++) {
    if (lobby.players.at(i).id == localId) return lobby.players.at(i).device;
  }
  return 0;
}
}

NetMatchSession::NetMatchSession() : match(0), lastSnapshotTime_ms(0) {
}

NetMatchSession::~NetMatchSession() {
}

bool NetMatchSession::IsActive() const {
  return GetMenuTask()->GetNetServer() != 0 || GetMenuTask()->GetNetClient() != 0;
}

bool NetMatchSession::IsHost() const {
  return GetMenuTask()->GetNetServer() != 0;
}

bool NetMatchSession::IsClient() const {
  return GetMenuTask()->GetNetClient() != 0;
}

void NetMatchSession::StartMatch(Match *match) {
  this->match = match;
  lastSnapshotTime_ms = 0;
  clientInputQueue.clear();
  hostInputDelay.reset();

  if (IsClient()) return; // the caller already put the Match in remote mode

  boost::shared_ptr<NetServer> server = GetMenuTask()->GetNetServer();
  if (!server) return;

  SetupControllers(match);
  // Lobby join/leave churn must not look like a mid-match roster change.
  server->ClearRosterEvents();

  // Publish the animation table so clients can resolve animIDs.
  std::vector<std::string> names;
  const std::vector<Animation*> &animations = match->GetAnims()->GetAnimations();
  names.reserve(animations.size());
  for (unsigned int i = 0; i < animations.size(); i++) names.push_back(animations.at(i)->GetName());
  NetBuffer buffer;
  WriteAnimationTable(buffer, names);
  server->BroadcastMessage(e_NetMessage_AnimationTable, buffer);

  // Match environment (sun + kits) is mirrored so clients match the host.
  match->BroadcastMatchOptions();
}

void NetMatchSession::StopMatch() {
  match = 0;
  clientInputQueue.clear();
  hostInputDelay.reset();
}

e_NetMatchPhaseState NetMatchSession::GetState() const {
  if (!match) return e_NetMatchPhaseState_Playing;
  if (!match->GetPause()) return e_NetMatchPhaseState_Playing;
  if (SideSelectActive()) return e_NetMatchPhaseState_SideSelect;
  return e_NetMatchPhaseState_Paused;
}

bool NetMatchSession::SideSelectActive() const {
  boost::shared_ptr<NetServer> server = GetMenuTask()->GetNetServer();
  if (server) return server->GetLobbyState().sideSelect;
  boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
  if (client) return client->GetLobbyState().sideSelect;
  return false;
}

void NetMatchSession::SetupControllers(Match *target) {
  boost::shared_ptr<NetServer> server = GetMenuTask()->GetNetServer();
  if (!server || !target) return;

  // Re-bindable: applied at match start and again from the pause side selection.
  target->GetTeam(0)->DeleteHumanGamers();
  target->GetTeam(1)->DeleteHumanGamers();
  hostInputDelay.reset();

  const NetLobbyState lobby = server->GetLobbyState();
  // Fairness delay only matters when at least one remote client is connected.
  const bool hasClients = !server->GetHIDevices().empty();

  int colorCounter[2] = {0, 0};
  for (unsigned int i = 0; i < lobby.players.size(); i++) {
    const NetLobbyPlayer &player = lobby.players.at(i);
    if (player.side != e_NetSide_Home && player.side != e_NetSide_Away) continue;

    int teamID = (player.side == e_NetSide_Home) ? 0 : 1;
    IHIDevice *device = 0;
    if (player.isHost) {
      IHIDevice *local = FindLocalDevice(player.device);
      if (local && hasClients) hostInputDelay = boost::make_shared<DelayedHIDDevice>(local);
      device = hostInputDelay ? hostInputDelay.get() : local;
    } else {
      device = server->GetHIDevice(player.id).get();
    }
    if (device) {
      target->GetTeam(teamID)->AddHumanGamer(device, (e_PlayerColor)(colorCounter[teamID] % 5));
      colorCounter[teamID]++;
    }
  }
}

void NetMatchSession::RebindControllers(Match *target) {
  if (!target || target->IsRemotePresentation()) return;
  SetupControllers(target);
  target->Pause(false);
}

void NetMatchSession::HandleRosterChanges(Match *match) {
  boost::shared_ptr<NetServer> server = GetMenuTask()->GetNetServer();
  if (!server) return;

  bool rosterChanged = false;
  std::vector<uint32_t> joinedIds;

  uint32_t joinedId = 0;
  while (server->ConsumeJoinedPlayer(joinedId)) {
    rosterChanged = true;
    joinedIds.push_back(joinedId);
    // Hand the newcomer the current match so it can build the scene and then
    // join the side selection like everyone else.
    NetMatchSetup setup;
    setup.teamId[0] = GetMenuTask()->GetTeamID(0);
    setup.teamId[1] = GetMenuTask()->GetTeamID(1);
    NetBuffer setupBuffer;
    WriteMatchSetup(setupBuffer, setup);
    server->SendToPlayer(joinedId, e_NetMessage_MatchSetup, setupBuffer);

    std::vector<std::string> names;
    const std::vector<Animation*> &animations = match->GetAnims()->GetAnimations();
    names.reserve(animations.size());
    for (unsigned int i = 0; i < animations.size(); i++) names.push_back(animations.at(i)->GetName());
    NetBuffer animBuffer;
    WriteAnimationTable(animBuffer, names);
    server->SendToPlayer(joinedId, e_NetMessage_AnimationTable, animBuffer);

    NetMatchEnvironment environment;
    match->GetMatchEnvironment(environment);
    NetBuffer environmentBuffer;
    WriteMatchEnvironment(environmentBuffer, environment);
    server->SendToPlayer(joinedId, e_NetMessage_MatchEnvironment, environmentBuffer);

    NetBuffer snapshotBuffer;
    match->CaptureRemoteSnapshot(snapshotBuffer);
    server->SendToPlayer(joinedId, e_NetMessage_Snapshot, snapshotBuffer);
  }

  uint32_t goneId = 0;
  while (server->ConsumeDisconnectedPlayer(goneId)) rosterChanged = true;

  if (rosterChanged) {
    if (!match->GetPause()) match->Pause(true);
    // A peer that joins while the match is already paused never saw the
    // PauseState broadcast, so send it the current state explicitly.
    for (unsigned int i = 0; i < joinedIds.size(); i++) {
      NetBuffer pauseBuffer;
      pauseBuffer.PutBool(match->GetPause());
      server->SendToPlayer(joinedIds.at(i), e_NetMessage_PauseState, pauseBuffer);
    }
    // Drop bindings to devices that no longer exist (a disconnected client) and
    // let AI take over the freed side until players re-confirm; only then can
    // the retired device be released.
    SetupControllers(match);
    server->ClearRetiredDevices();
    server->SetSideSelectMode(true);
  }
}

void NetMatchSession::ProcessHost(Match *match) {
  // Disconnect/join during a match pauses the game and re-opens side selection
  // for every peer (like a mid-match controller unplug).
  HandleRosterChanges(match);

  boost::shared_ptr<NetServer> server = GetMenuTask()->GetNetServer();
  if (!server) return;

  // Fairness: delay the host's own input by 2U + B, matching the slowest
  // client's round trip. The wrapped device samples the local one, which the
  // caller already Process()ed this tick.
  if (hostInputDelay) {
    // RefreshGamepads may have deleted the wrapped device; never sample a freed
    // pointer. The match is paused in that case anyway.
    const std::vector<IHIDevice*> &controllers = GetControllers();
    bool sourceAlive = false;
    for (unsigned int i = 0; i < controllers.size(); i++) {
      if (controllers.at(i) == hostInputDelay->GetSource()) { sourceAlive = true; break; }
    }
    if (sourceAlive) {
      int delay_ms = server->GetMaxClientRtt_ms() + net_interpolationBuffer_ms;
      hostInputDelay->SetDelayTicks((delay_ms + 5) / 10);
      hostInputDelay->Process();
    }
  }

  // Feed remote clients' input into their virtual devices before simulating.
  std::vector<boost::shared_ptr<NetHIDDevice> > netDevices = server->GetHIDevices();
  for (unsigned int i = 0; i < netDevices.size(); i++) netDevices.at(i)->Process();

  // Pause is peer-equal: apply any client request and rebroadcast.
  bool requestPaused = false;
  if (server->ConsumePauseRequest(requestPaused)) match->Pause(requestPaused);

  // Resume is a vote: unpause only once every peer has agreed.
  if (match->GetPause() && server->ConsumeAllResumeReady()) match->Pause(false);

  // A peer left side selection: apply the chosen sides but stay paused (back to
  // the pause menu). Resuming is a separate action (Continue vote).
  if (server->ConsumeSideSelectCancel()) SetupControllers(match);

  match->Process();
}

void NetMatchSession::ProcessClient(Match *match) {
  boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
  if (!client) return;

  NetMatchEnvironment environment;
  if (client->ConsumeEnvironment(environment)) {
    match->SetSunParams(environment.sunPosition, environment.sunColor);
    if (environment.homeKit > 0) match->GetTeam(0)->SetKitNumber(environment.homeKit);
    if (environment.awayKit > 0) match->GetTeam(1)->SetKitNumber(environment.awayKit);
  }

  bool networkPaused = false;
  if (client->ConsumePauseState(networkPaused)) match->SetPauseFromNetwork(networkPaused);

  // Sample the local device, delay by (2U - own one-way delay + B) and ship it
  // to the host, so every peer's input reaches the simulation at the same moment
  // after the button press. maxRtt mirrors the host's value.
  IHIDevice *localDevice = FindLocalDevice(GetLocalDeviceType(client));
  if (localDevice) {
    NetInputFrame frame;
    frame.buttons = 0;
    for (int b = 0; b < e_ButtonFunction_Size; b++) {
      if (localDevice->GetButton((e_ButtonFunction)b)) frame.buttons |= (1u << b);
    }
    frame.direction = localDevice->GetDirection();
    clientInputQueue.push_back(frame);

    int rtt_ms = client->GetRtt_ms();
    if (rtt_ms < 0) rtt_ms = 0;
    int delay_ms = match->GetRemoteMaxRtt_ms() + net_interpolationBuffer_ms - rtt_ms / 2;
    if (delay_ms < 0) delay_ms = 0;
    int delayTicks = (delay_ms + 5) / 10;

    if ((int)clientInputQueue.size() > delayTicks) {
      client->SendInputFrame(clientInputQueue.front());
      clientInputQueue.pop_front();
    }
    // If the required delay shrank, drop stale frames instead of lagging behind
    // forever at one frame per tick.
    while ((int)clientInputQueue.size() > delayTicks + 1) clientInputQueue.pop_front();
  }

  if (!match->HasRemoteAnimTable()) {
    std::vector<std::string> names;
    if (client->ConsumeAnimationTable(names)) match->ResolveRemoteAnimTable(names);
  }
  if (match->HasRemoteAnimTable()) {
    std::vector<uint8_t> bytes;
    if (client->ConsumeSnapshot(bytes)) {
      NetBuffer buffer;
      buffer.Data().assign(bytes.begin(), bytes.end());
      buffer.ResetRead();
      match->ApplyRemoteSnapshot(buffer);
    }
  }
}

void NetMatchSession::Process(Match *match) {
  this->match = match;
  if (match->IsRemotePresentation()) ProcessClient(match);
  else ProcessHost(match);
}

void NetMatchSession::BroadcastSnapshot(Match *match) {
  boost::shared_ptr<NetServer> server = GetMenuTask()->GetNetServer();
  if (!server || match->IsRemotePresentation()) return;

  unsigned long now_ms = EnvironmentManager::GetInstance().GetTime_ms();
  if (now_ms - lastSnapshotTime_ms >= (unsigned long)(1000 / net_snapshotRate_hz)) {
    lastSnapshotTime_ms = now_ms;
    NetBuffer buffer;
    match->CaptureRemoteSnapshot(buffer);
    server->BroadcastMessage(e_NetMessage_Snapshot, buffer);
  }
}

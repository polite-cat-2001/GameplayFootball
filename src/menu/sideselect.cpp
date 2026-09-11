// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#include "sideselect.hpp"

#include "../main.hpp"

#include "pagefactory.hpp"
#include "menutask.hpp"
#include "gametask.hpp"

#include "../onthepitch/match.hpp"

#include "hid/gamepad.hpp"
#include "hid/keyboard.hpp"

#include "../gamedefines.hpp"

#include "net/netserver.hpp"
#include "net/netclient.hpp"
#include "net/netmessages.hpp"

#include "managers/environmentmanager.hpp"

#include "scene/objects/image2d.hpp"

#include <SDL3/SDL.h>
#include <cmath>

using namespace blunted;

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

static int SpatialIndex(int side) {
  if (side < 0) return 0;
  if (side > 0) return 2;
  return 1;
}

static int SideFromSpatial(int spatial) {
  if (spatial <= 0) return -1;
  if (spatial >= 2) return 1;
  return 0;
}

static int SideToNet(int side) {
  if (side < 0) return e_NetSide_Home;
  if (side > 0) return e_NetSide_Away;
  return e_NetSide_Spectator;
}

static int NetToSide(int side) {
  if (side == e_NetSide_Home) return -1;
  if (side == e_NetSide_Away) return 1;
  return 0;
}

// ---------------------------------------------------------------------------
// LocalSideSelectBackend
// ---------------------------------------------------------------------------

LocalSideSelectBackend::LocalSideSelectBackend(bool inGame, bool resumeOnClose)
  : inGame(inGame), resumeOnClose(resumeOnClose), built(false) {}

void LocalSideSelectBackend::BuildParticipants() {
  // Keep sides chosen in this screen across a hot-plug rebuild (match by stable
  // joystick id); controllers that appeared since use the queued setup.
  std::vector<SideSelectParticipant> live = participants;
  std::vector<SideSelection> saved = GetMenuTask()->GetControllerSetup();

  participants.clear();
  const std::vector<IHIDevice*> &controllers = GetControllers();
  for (unsigned int i = 0; i < controllers.size(); i++) {
    SideSelectParticipant p;
    p.remote = false;
    p.id = i;
    p.isGamepad = (controllers.at(i)->GetDeviceType() == e_HIDeviceType_Gamepad);
    p.device = p.isGamepad ? 1 : 0;
    p.joystickID = p.isGamepad ? static_cast<HIDGamepad*>(controllers.at(i))->GetJoystickID() : 0;
    p.isLocalPeer = true;
    p.canControl = true;
    p.side = 0;
    p.ready = false;

    bool restored = false;
    for (unsigned int s = 0; s < live.size(); s++) {
      if (live.at(s).joystickID == p.joystickID) { p.side = live.at(s).side; p.ready = live.at(s).ready; restored = true; break; }
    }
    if (!restored) {
      for (unsigned int s = 0; s < saved.size(); s++) {
        if (saved.at(s).joystickID == p.joystickID) { p.side = saved.at(s).side; break; }
      }
    }
    if (p.isGamepad) p.layout = static_cast<HIDGamepad*>(controllers.at(i))->GetLayout();

    // auto-pick a side for the first device(s) when there is no previous setup
    if (!inGame && !built && live.empty() && saved.empty()) {
      if (i == 0 && controllers.size() < 2) p.side = -1;
      else if (i == 1) p.side = -1;
    }

    participants.push_back(p);
  }
}

std::vector<SideSelectParticipant> LocalSideSelectBackend::GetParticipants() {
  BuildParticipants();
  built = true;
  return participants;
}

void LocalSideSelectBackend::SetSide(int id, int side) {
  for (unsigned int i = 0; i < participants.size(); i++) {
    if (participants.at(i).id == id) {
      participants.at(i).side = side;
      participants.at(i).ready = false;
      return;
    }
  }
}

void LocalSideSelectBackend::SetReady(int id, bool ready) {
  for (unsigned int i = 0; i < participants.size(); i++) {
    if (participants.at(i).id == id) { participants.at(i).ready = ready; return; }
  }
}

void LocalSideSelectBackend::ToggleLayout(int id) {
  for (unsigned int i = 0; i < participants.size(); i++) {
    if (participants.at(i).id != id || !participants.at(i).isGamepad) continue;
    const std::vector<IHIDevice*> &controllers = GetControllers();
    if (id < 0 || id >= (int)controllers.size()) return;
    HIDGamepad *gamepad = static_cast<HIDGamepad*>(controllers.at(id));
    e_ControllerLayout next = (gamepad->GetLayout() == e_ControllerLayout_PES) ? e_ControllerLayout_FIFA : e_ControllerLayout_PES;
    gamepad->SetLayout(next);
    participants.at(i).layout = next;
    return;
  }
}

bool LocalSideSelectBackend::AllReady() {
  for (unsigned int i = 0; i < participants.size(); i++) {
    if (participants.at(i).side == 0) continue; // centre/spectator needs no confirm
    if (!participants.at(i).ready) return false;
  }
  return true;
}

void LocalSideSelectBackend::ApplyControllerSetup() {
  std::vector<SideSelection> sides;
  for (unsigned int i = 0; i < participants.size(); i++) {
    SideSelection s;
    s.controllerID = participants.at(i).id;
    s.joystickID = participants.at(i).joystickID;
    s.controllerImage = 0;
    s.side = participants.at(i).side;
    s.confirmed = participants.at(i).ready;
    sides.push_back(s);
  }
  GetMenuTask()->SetControllerSetup(sides);

  if (inGame) {
    Match *match = GetGameTask() ? GetGameTask()->GetMatch() : 0;
    if (match) {
      match->UpdateControllerSetup();
      if (resumeOnClose) match->Pause(false); // resume the match we paused on unplug
    }
  }
}

void LocalSideSelectBackend::Commit() { ApplyControllerSetup(); }
void LocalSideSelectBackend::Cancel() { ApplyControllerSetup(); }

// ---------------------------------------------------------------------------
// NetworkSideSelectBackend
// ---------------------------------------------------------------------------

NetworkSideSelectBackend::NetworkSideSelectBackend(bool resume)
  : resume(resume), sawSideSelect(false), committed(false) {}

bool NetworkSideSelectBackend::IsHost() const {
  return GetMenuTask()->GetNetServer() != 0;
}

int NetworkSideSelectBackend::GetLocalPlayerId() {
  if (IsHost()) return 0;
  boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
  if (client) return (int)client->GetPlayerId();
  return 0;
}

void NetworkSideSelectBackend::SendAction(int type, int side, int value) {
  NetLobbyAction action;
  action.type = type;
  action.side = side;
  action.value = value;
  if (IsHost()) {
    boost::shared_ptr<NetServer> server = GetMenuTask()->GetNetServer();
    if (server) { action.playerId = 0; server->ApplyLobbyAction(action); }
  } else {
    boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
    if (client) client->SendLobbyAction(action);
  }
}

std::vector<SideSelectParticipant> NetworkSideSelectBackend::GetParticipants() {
  NetLobbyState state;
  if (IsHost()) {
    boost::shared_ptr<NetServer> server = GetMenuTask()->GetNetServer();
    if (server) state = server->GetLobbyState();
  } else {
    boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
    if (client) state = client->GetLobbyState();
  }

  int localId = GetLocalPlayerId();
  std::vector<SideSelectParticipant> result;
  for (unsigned int i = 0; i < state.players.size(); i++) {
    const NetLobbyPlayer &player = state.players.at(i);
    SideSelectParticipant p;
    p.remote = true;
    p.id = (int)player.id;
    p.device = player.device;
    p.isGamepad = (player.device == 1);
    p.isLocalPeer = (player.id == (uint32_t)localId);
    p.canControl = p.isLocalPeer;
    p.side = NetToSide(player.side);
    p.ready = player.ready;
    p.label = player.name;
    result.push_back(p);
  }
  return result;
}

void NetworkSideSelectBackend::SetSide(int id, int side) {
  SendAction(e_NetLobbyAction_SetSide, SideToNet(side), 0);
}

void NetworkSideSelectBackend::SetReady(int id, bool ready) {
  SendAction(e_NetLobbyAction_SetReady, 0, ready ? 1 : 0);
}

void NetworkSideSelectBackend::SetDevice(int device) {
  SendAction(e_NetLobbyAction_SetDevice, 0, device);
}

bool NetworkSideSelectBackend::AllReady() {
  NetLobbyState state;
  if (IsHost()) {
    boost::shared_ptr<NetServer> server = GetMenuTask()->GetNetServer();
    if (server) state = server->GetLobbyState();
  } else {
    boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
    if (client) state = client->GetLobbyState();
  }
  if (state.players.empty()) return false;
  for (unsigned int i = 0; i < state.players.size(); i++) {
    if (!state.players.at(i).ready) return false;
  }
  return true;
}

void NetworkSideSelectBackend::Commit() {
  if (!resume || !IsHost()) return;
  boost::shared_ptr<NetServer> server = GetMenuTask()->GetNetServer();
  if (server) server->SetSideSelectMode(false);
  GetGameTask()->RebindNetworkControllers();
  committed = true;
}

void NetworkSideSelectBackend::Cancel() {
  if (!resume) {
    // Pre-match lobby: leaving closes the session for everyone.
    if (IsHost()) {
      boost::shared_ptr<NetServer> server = GetMenuTask()->GetNetServer();
      if (server) server->Stop();
      GetMenuTask()->SetNetServer(boost::shared_ptr<NetServer>());
    } else {
      boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
      if (client) client->Disconnect();
      GetMenuTask()->SetNetClient(boost::shared_ptr<NetClient>());
    }
    return;
  }

  if (IsHost()) {
    boost::shared_ptr<NetServer> server = GetMenuTask()->GetNetServer();
    if (server) server->SetSideSelectMode(false);
    // Apply the chosen sides but keep the match paused: leaving side selection
    // returns to the pause menu, resuming is a separate Continue vote.
    GetGameTask()->ApplyNetworkControllers();
  } else {
    SendAction(e_NetLobbyAction_RequestSideSelect, 0, 0);
  }
}

void NetworkSideSelectBackend::Process() {
  if (!resume) return;
  int saw = 0;
  if (IsHost()) {
    boost::shared_ptr<NetServer> server = GetMenuTask()->GetNetServer();
    if (server && server->GetLobbyState().sideSelect) saw = 1;
  } else {
    boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
    if (client && client->GetLobbyState().sideSelect) saw = 1;
  }
  if (saw) sawSideSelect = true;
}

int NetworkSideSelectBackend::PollTransition() {
  if (resume) return -1;

  if (!IsHost()) {
    boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
    if (!client || client->GetState() == e_NetConnectionState_Disconnected) return e_PageID_NetworkMenu;

    // A client joining (or re-joining) a match that is already running gets the
    // streamed MatchSetup from the host; build the paused match, then the menu
    // layer opens the mirrored in-match side selection over it.
    NetMatchSetup setup;
    if (client->ConsumeMatchSetup(setup)) {
      GetMenuTask()->SetTeamIDs(int_to_str(setup.teamId[0]), int_to_str(setup.teamId[1]));
      return e_PageID_LoadingMatch;
    }
  }

  NetLobbyState state;
  if (IsHost()) {
    boost::shared_ptr<NetServer> server = GetMenuTask()->GetNetServer();
    if (server) state = server->GetLobbyState();
  } else {
    boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
    if (client) state = client->GetLobbyState();
  }
  if (state.phase == e_NetLobbyPhase_Teams) return e_PageID_NetworkLobby;
  return -1;
}

bool NetworkSideSelectBackend::PollClosed() {
  if (!resume) return false;

  int sideSelect = 0;
  if (IsHost()) {
    boost::shared_ptr<NetServer> server = GetMenuTask()->GetNetServer();
    if (server) sideSelect = server->GetLobbyState().sideSelect ? 1 : 0;
  } else {
    boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
    if (client) sideSelect = client->GetLobbyState().sideSelect ? 1 : 0;
  }
  if (sawSideSelect && !sideSelect) return true;

  // Client safety net: the host resumed without us seeing the flag clear.
  if (!IsHost()) {
    Match *match = GetGameTask() ? GetGameTask()->GetMatch() : 0;
    if (match && !match->GetPause()) return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
// SideSelectPage
// ---------------------------------------------------------------------------

SideSelectPage::SideSelectPage(Gui2WindowManager *windowManager, const Gui2PageData &pageData) : Gui2Page(windowManager, pageData) {

  inGame = pageData.properties && pageData.properties->GetBool("isInGame");
  resumeOnClose = pageData.properties && pageData.properties->GetBool("resumeOnClose");
  committed = false;
  sentDevice = -1;
  lastGamepadSideChange_ms = 0;

  const bool network = GetMenuTask()->GetNetServer() || GetMenuTask()->GetNetClient();
  if (network) backend = new NetworkSideSelectBackend(inGame);
  else backend = new LocalSideSelectBackend(inGame, resumeOnClose);

  background = new Gui2Image(windowManager, "image_sideselect_bg", 10, 15, 80, 70);
  this->AddView(background);
  background->LoadImage("media/menu/backgrounds/black.png");
  background->Show();

  const bool local = backend->IsLocal();
  homeCaption = new Gui2Caption(windowManager, "caption_sideselect_home", 0, 0, 28, 3, local ? "Team 1" : "HOME");
  awayCaption = new Gui2Caption(windowManager, "caption_sideselect_away", 0, 0, 28, 3, local ? "Team 2" : "AWAY");
  homeCaption->SetPosition(25 - homeCaption->GetTextWidthPercent() * 0.5, 10);
  awayCaption->SetPosition(75 - awayCaption->GetTextWidthPercent() * 0.5, 10);
  this->AddView(homeCaption);
  homeCaption->Show();
  this->AddView(awayCaption);
  awayCaption->Show();

  phaseCaption = new Gui2Caption(windowManager, "caption_sideselect_phase", 35, 5, 30, 3, "choose your side");
  this->AddView(phaseCaption);
  phaseCaption->Show();

  helpCaption = new Gui2Caption(windowManager, "caption_sideselect_help", 20, 88, 60, 3, "Left/Right: side    Enter: ready    Esc: leave");
  this->AddView(helpCaption);
  helpCaption->Show();

  participants = backend->GetParticipants();
  RebuildRows();
  SetImagePositions();

  this->SetFocus();
  this->Show();
}

SideSelectPage::~SideSelectPage() {
  delete backend;
}

void SideSelectPage::DrawPixelLine(boost::intrusive_ptr<Image2D> img, int x0, int y0, int x1, int y1, const Vector3 &color) {
  int dx = abs(x1 - x0);
  int dy = -abs(y1 - y0);
  int sx = x0 < x1 ? 1 : -1;
  int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  while (true) {
    img->PutPixel(x0, y0, color);
    img->PutPixel(x0 + 1, y0, color);
    img->PutPixel(x0, y0 + 1, color);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

void SideSelectPage::RebuildRows() {
  for (unsigned int i = 0; i < rowImages.size(); i++) {
    rowImages.at(i)->Exit();
    delete rowImages.at(i);
  }
  for (unsigned int i = 0; i < rowNames.size(); i++) {
    if (rowNames.at(i)) { rowNames.at(i)->Exit(); delete rowNames.at(i); }
  }
  for (unsigned int i = 0; i < rowReadyIcons.size(); i++) {
    rowReadyIcons.at(i)->Exit();
    delete rowReadyIcons.at(i);
  }
  rowImages.clear();
  rowNames.clear();
  rowReadyIcons.clear();
  rowReadyState.clear();
  rowDevice.clear();
  rowDelay.clear();

  const bool local = backend->IsLocal();
  for (unsigned int i = 0; i < participants.size(); i++) {
    const SideSelectParticipant &p = participants.at(i);

    Gui2Image *image = new Gui2Image(windowManager, "image_sideselect_device" + int_to_str(i), 0, 0, 14, 10);
    this->AddView(image);
    image->LoadImage(p.device == 1 ?
        "media/menu/controller/controller_small.png" :
        "media/menu/controller/keyboard_small.png");
    image->Show();
    rowImages.push_back(image);

    Gui2Caption *name = 0;
    if (local) {
      if (p.isGamepad) {
        std::string layoutStr = (p.layout == e_ControllerLayout_PES) ? "PES" : "FIFA";
        name = new Gui2Caption(windowManager, "caption_sideselect_layout" + int_to_str(i), 0, 0, 12, 3, "LAYOUT: " + layoutStr);
      }
    } else {
      name = new Gui2Caption(windowManager, "caption_sideselect_name" + int_to_str(i), 0, 0, 28, 3, "");
    }
    if (name) {
      this->AddView(name);
      name->Show();
      rowNames.push_back(name);
    } else {
      rowNames.push_back(0);
    }

    Gui2Image *ready = new Gui2Image(windowManager, "image_sideselect_ready" + int_to_str(i), 0, 0, 3, 3);
    this->AddView(ready);
    ready->Hide();
    rowReadyIcons.push_back(ready);
    rowReadyState.push_back(false);
    rowDevice.push_back(p.device);
    rowDelay.push_back(0);
  }
}

void SideSelectPage::RefreshDeviceIcons() {
  for (unsigned int i = 0; i < participants.size() && i < rowImages.size() && i < rowDevice.size(); i++) {
    if (rowDevice.at(i) == participants.at(i).device) continue;
    rowDevice.at(i) = participants.at(i).device;
    rowImages.at(i)->LoadImage(participants.at(i).device == 1 ?
        "media/menu/controller/controller_small.png" :
        "media/menu/controller/keyboard_small.png");
  }
}

void SideSelectPage::SetImagePositions() {
  for (unsigned int i = 0; i < participants.size() && i < rowImages.size(); i++) {
    int x = 43 + participants.at(i).side * 25;
    int y = 20 + i * 15;
    rowImages.at(i)->SetPosition(x, y);

    Gui2Caption *name = rowNames.at(i);
    if (name) {
      if (backend->IsLocal()) {
        if (participants.at(i).isGamepad) {
          std::string layoutStr = (participants.at(i).layout == e_ControllerLayout_PES) ? "PES" : "FIFA";
          name->SetCaption("LAYOUT: " + layoutStr);
        }
      } else {
        std::string label = participants.at(i).label;
        name->SetCaption(participants.at(i).isLocalPeer ? "> " + label : label);
      }
      name->SetPosition(x + 7 - name->GetTextWidthPercent() * 0.5, y + 10);
    }

    if (i < rowReadyIcons.size()) rowReadyIcons.at(i)->SetPosition(x + 5, y + 13);
  }
}

void SideSelectPage::SetReadyIcon(int row, bool ready) {
  if (row < 0 || row >= (int)rowReadyIcons.size()) return;
  if (rowReadyState.at(row) == ready) {
    if (ready) rowReadyIcons.at(row)->Show();
    return;
  }
  rowReadyState.at(row) = ready;

  Gui2Image *icon = rowReadyIcons.at(row);
  if (!ready) { icon->Hide(); return; }

  boost::intrusive_ptr<Image2D> img = icon->GetImage2D();
  int w = int(round(img->GetSize().coords[0]));
  int h = int(round(img->GetSize().coords[1]));
  img->DrawRectangle(0, 0, w, h, Vector3(0, 0, 0), 0);

  Vector3 green(0.0f, 200.0f, 0.0f);
  Vector3 white(255.0f, 255.0f, 255.0f);
  int cx = w / 2;
  int cy = h / 2;
  int r = (w < h ? w : h) / 2 - 1;
  for (int y = 0; y < h; y++) {
    int dyy = y - cy;
    int d = r * r - dyy * dyy;
    if (d >= 0) {
      int halfW = (int)floor(sqrt((real)d));
      img->DrawRectangle(cx - halfW, y, halfW * 2, 1, green);
    }
  }
  int x0 = cx - int(r * 0.5);
  int y0 = cy;
  int x1 = cx - int(r * 0.1);
  int y1 = cy + int(r * 0.4);
  int x2 = cx + int(r * 0.6);
  int y2 = cy - int(r * 0.4);
  DrawPixelLine(img, x0, y0, x1, y1, white);
  DrawPixelLine(img, x1, y1, x2, y2, white);
  img->OnChange();
  icon->Show();
}

void SideSelectPage::ChangeSide(int participantId, int delta) {
  for (unsigned int i = 0; i < participants.size(); i++) {
    if (participants.at(i).id != participantId) continue;
    int spatial = SpatialIndex(participants.at(i).side) + delta;
    if (spatial < 0) spatial = 0;
    if (spatial > 2) spatial = 2;
    int side = SideFromSpatial(spatial);
    if (side != participants.at(i).side) backend->SetSide(participantId, side);
    return;
  }
}

bool SideSelectPage::CheckAllConfirmed() {
  if (committed) return false;

  if (backend->IsLocal()) {
    if (!backend->AllReady()) return false;
    committed = true;
    backend->Commit();
    if (backend->IsInGame()) GoBack();
    else CreatePage(e_PageID_TeamSelect);
    return true;
  }

  // Pre-match network: the server advances to the team phase on its own.
  if (!backend->IsInGame()) return false;

  if (!backend->AllReady()) return false;
  committed = true;
  if (backend->IsHost()) backend->Commit();
  // client: Process()/PollClosed() closes the overlay once the host resumes
  return false;
}

void SideSelectPage::Leave() {
  if (backend->IsLocal()) {
    backend->Cancel();
    GoBack();
    return;
  }

  if (backend->IsInGame()) {
    backend->Cancel();
    if (backend->IsHost()) GoBack();
    // client: wait for Process() to see sideSelect clear
    return;
  }

  backend->Cancel();
  CreatePage(e_PageID_NetworkMenu);
}

void SideSelectPage::Process() {
  Gui2View::Process();

  backend->Process();

  int next = backend->PollTransition();
  if (next >= 0) { CreatePage(next); return; }

  if (backend->PollClosed()) { GoBack(); return; }

  std::vector<SideSelectParticipant> current = backend->GetParticipants();
  if (current.size() != participants.size()) {
    participants = current;
    RebuildRows();
  } else {
    participants = current;
  }
  RefreshDeviceIcons();
  for (unsigned int i = 0; i < participants.size() && i < rowReadyIcons.size(); i++) {
    SetReadyIcon(i, participants.at(i).ready);
  }
  SetImagePositions();

  if (!backend->IsLocal()) CheckAllConfirmed();
}

void SideSelectPage::ProcessKeyboardEvent(KeyboardEvent *event) {
  if (!backend->IsLocal() && sentDevice != 0) {
    sentDevice = 0;
    backend->SetDevice(0);
  }

  // Find the participant the keyboard steers: the first local device (offline)
  // or the local peer (network).
  int index = -1;
  for (unsigned int i = 0; i < participants.size(); i++) {
    if (backend->IsLocal()) { if (participants.at(i).id == 0) { index = i; break; } }
    else if (participants.at(i).isLocalPeer) { index = i; break; }
  }

  if (event->GetKeyOnce(SDLK_ESCAPE)) {
    if (backend->IsLocal() && index >= 0 && participants.at(index).ready) {
      backend->SetReady(participants.at(index).id, false); // two-step: unconfirm first
    } else {
      Leave();
    }
    return;
  }

  if (index < 0) return;
  int id = participants.at(index).id;

  if (backend->IsLocal()) {
    HIDKeyboard *keyboard = static_cast<HIDKeyboard*>(GetControllers().at(0));
    if (event->GetKeyOnce(keyboard->GetFunctionMapping(e_ButtonFunction_Left))) ChangeSide(id, -1);
    if (event->GetKeyOnce(keyboard->GetFunctionMapping(e_ButtonFunction_Right))) ChangeSide(id, 1);
  } else {
    if (event->GetKeyOnce(SDLK_LEFT)) ChangeSide(id, -1);
    if (event->GetKeyOnce(SDLK_RIGHT)) ChangeSide(id, 1);
  }

  if (event->GetKeyOnce(SDLK_RETURN)) {
    // Network ready is a personal toggle; local confirm is one-way (Esc/B undo).
    backend->SetReady(id, backend->IsLocal() ? true : !participants.at(index).ready);
    if (CheckAllConfirmed()) return;
  }

  participants = backend->GetParticipants();
  SetImagePositions();
}

void SideSelectPage::ProcessJoystickEvent(JoystickEvent *event) {
  if (!backend->IsLocal() && sentDevice != 1) {
    sentDevice = 1;
    backend->SetDevice(1);
  }

  const std::vector<IHIDevice*> &controllers = GetControllers();
  unsigned long now_ms = EnvironmentManager::GetInstance().GetTime_ms();

  if (backend->IsLocal()) {
    for (unsigned int c = 1; c < controllers.size(); c++) {
      if (controllers.at(c)->GetDeviceType() != e_HIDeviceType_Gamepad) continue;
      HIDGamepad *gamepad = static_cast<HIDGamepad*>(controllers.at(c));
      int joyID = gamepad->GetGamepadID();
      int index = -1;
      for (unsigned int i = 0; i < participants.size(); i++) {
        if (participants.at(i).id == (int)c) { index = i; break; }
      }
      if (index < 0) continue;
      int id = participants.at(index).id;

      if (event->GetButton(joyID, gamepad->GetControllerMapping(e_ControllerButton_B))) {
        if (participants.at(index).ready) backend->SetReady(id, false);
        else { Leave(); return; }
        continue;
      }

      if (now_ms - rowDelay.at(index) > 250) {
        if (gamepad->GetButtonValue(e_ButtonFunction_Left) > 0.5f) { ChangeSide(id, -1); rowDelay.at(index) = now_ms; }
        else if (gamepad->GetButtonValue(e_ButtonFunction_Right) > 0.5f) { ChangeSide(id, 1); rowDelay.at(index) = now_ms; }
      }

      if (event->GetButton(joyID, gamepad->GetControllerMapping(e_ControllerButton_L1)) ||
          event->GetButton(joyID, gamepad->GetControllerMapping(e_ControllerButton_R1))) {
        backend->ToggleLayout(id);
      }

      if (event->GetButton(joyID, gamepad->GetControllerMapping(e_ControllerButton_A))) {
        backend->SetReady(id, true);
        if (CheckAllConfirmed()) return;
      }
    }
  } else {
    int localId = -1;
    for (unsigned int i = 0; i < participants.size(); i++) {
      if (participants.at(i).isLocalPeer) { localId = participants.at(i).id; break; }
    }
    for (unsigned int c = 1; c < controllers.size(); c++) {
      if (controllers.at(c)->GetDeviceType() != e_HIDeviceType_Gamepad) continue;
      HIDGamepad *gamepad = static_cast<HIDGamepad*>(controllers.at(c));
      int joyID = gamepad->GetGamepadID();

      if (now_ms - lastGamepadSideChange_ms > 250) {
        if (gamepad->GetButtonValue(e_ButtonFunction_Left) > 0.5f) { ChangeSide(localId, -1); lastGamepadSideChange_ms = now_ms; }
        else if (gamepad->GetButtonValue(e_ButtonFunction_Right) > 0.5f) { ChangeSide(localId, 1); lastGamepadSideChange_ms = now_ms; }
      }

      if (event->GetButton(joyID, gamepad->GetControllerMapping(e_ControllerButton_A))) {
        bool ready = false;
        for (unsigned int i = 0; i < participants.size(); i++) {
          if (participants.at(i).isLocalPeer) { ready = participants.at(i).ready; break; }
        }
        backend->SetReady(localId, !ready);
        if (CheckAllConfirmed()) return;
      }
    }
  }

  participants = backend->GetParticipants();
  SetImagePositions();
}

void SideSelectPage::ProcessWindowingEvent(WindowingEvent *event) {
  // Escape/B are handled per participant (two-step confirm/leave); the generic
  // windowing escape must not navigate on its own.
  event->Ignore();
}

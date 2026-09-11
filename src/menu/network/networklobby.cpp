#include "networklobby.hpp"

#include "network.hpp"

#include "../pagefactory.hpp"
#include "../startmatch/teamselect.hpp"

#include "../../main.hpp"

#include "utils/gui2/events.hpp"

#include "net/netclient.hpp"
#include "net/netserver.hpp"

#include "hid/gamepad.hpp"
#include "managers/environmentmanager.hpp"

#include "scene/objects/image2d.hpp"

#include <SDL3/SDL.h>
#include <cmath>

NetworkLobbyPage::NetworkLobbyPage(Gui2WindowManager *windowManager, const Gui2PageData &pageData) : Gui2Page(windowManager, pageData) {

  teamsBuilt = false;
  teamsVisible = false;
  sentDevice = -1;
  lastGamepadSideChange_ms = 0;
  localGamepadId = -1;
  deviceLostSent = false;
  for (int s = 0; s < 2; s++) {
    teamBg[s] = 0;
    teamGrid[s] = 0;
    countrySelect[s] = 0;
    leagueSelect[s] = 0;
    teamSelect[s] = 0;
    readyButton[s] = 0;
    lastCountryId[s] = -2;
    lastLeagueId[s] = -2;
    lastTeamId[s] = -2;
    defaultSent[s] = false;
  }

  background = new Gui2Image(windowManager, "image_network_lobby_bg", 10, 15, 80, 70);
  this->AddView(background);
  background->LoadImage("media/menu/backgrounds/black.png");
  background->Show();

  phaseCaption = new Gui2Caption(windowManager, "caption_network_lobby_phase", 35, 5, 30, 3, "");
  this->AddView(phaseCaption);
  phaseCaption->Show();

  side1Caption = new Gui2Caption(windowManager, "caption_network_lobby_side1", 0, 0, 28, 3, "HOME");
  side2Caption = new Gui2Caption(windowManager, "caption_network_lobby_side2", 0, 0, 28, 3, "AWAY");
  side1Caption->SetPosition(25 - side1Caption->GetTextWidthPercent() * 0.5, 10);
  side2Caption->SetPosition(75 - side2Caption->GetTextWidthPercent() * 0.5, 10);
  this->AddView(side1Caption);
  side1Caption->Show();
  this->AddView(side2Caption);
  side2Caption->Show();

  team1Caption = new Gui2Caption(windowManager, "caption_network_lobby_team1", 0, 0, 28, 3, "");
  team2Caption = new Gui2Caption(windowManager, "caption_network_lobby_team2", 0, 0, 28, 3, "");
  this->AddView(team1Caption);
  team1Caption->Hide();
  this->AddView(team2Caption);
  team2Caption->Hide();

  for (int i = 0; i < net_maxPlayers; i++) {
    Gui2Image *image = new Gui2Image(windowManager, "image_network_lobby_player" + int_to_str(i), 0, 0, 14, 10);
    this->AddView(image);
    image->LoadImage("media/menu/controller/controller_small.png");
    image->Hide();

    Gui2Caption *name = new Gui2Caption(windowManager, "caption_network_lobby_name" + int_to_str(i), 0, 0, 28, 3, "");
    this->AddView(name);
    name->Hide();

    Gui2Image *ready = new Gui2Image(windowManager, "image_network_lobby_ready" + int_to_str(i), 0, 0, 3, 3);
    this->AddView(ready);
    ready->Hide();

    playerImages.push_back(image);
    playerNames.push_back(name);
    playerReadyIcons.push_back(ready);
    playerReadyState.push_back(false);
    playerDeviceState.push_back(-1);
  }

  helpCaption = new Gui2Caption(windowManager, "caption_network_lobby_help", 20, 88, 60, 3, "");
  this->AddView(helpCaption);
  helpCaption->Show();

  this->SetFocus();
  this->Show();
}

NetworkLobbyPage::~NetworkLobbyPage() {
}

bool NetworkLobbyPage::IsHost() {
  return GetMenuTask()->GetNetServer() != 0;
}

NetLobbyState NetworkLobbyPage::GetState() {
  if (IsHost()) {
    boost::shared_ptr<NetServer> server = GetMenuTask()->GetNetServer();
    if (server) return server->GetLobbyState();
  } else {
    boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
    if (client) return client->GetLobbyState();
  }
  return NetLobbyState();
}

uint32_t NetworkLobbyPage::GetLocalPlayerId() {
  if (IsHost()) return 0;
  boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
  if (client) return client->GetPlayerId();
  return 0;
}

int NetworkLobbyPage::GetChooserSide(uint32_t playerId) {
  NetLobbyState state = GetState();
  for (int side = 0; side < 2; side++) {
    if (state.chooser[side] == playerId) return side;
  }
  return -1;
}

void NetworkLobbyPage::SendAction(int type, int side, int value, int value2) {
  NetLobbyAction action;
  action.type = type;
  action.side = side;
  action.value = value;
  action.value2 = value2;

  if (IsHost()) {
    boost::shared_ptr<NetServer> server = GetMenuTask()->GetNetServer();
    if (server) { action.playerId = 0; server->ApplyLobbyAction(action); }
  } else {
    boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
    if (client) client->SendLobbyAction(action);
  }
}

int NetworkLobbyPage::SideOffset(int side) {
  if (side == e_NetSide_Home) return -1;
  if (side == e_NetSide_Away) return 1;
  return 0;
}

int NetworkLobbyPage::SpatialIndex(int side) {
  if (side == e_NetSide_Home) return 0;
  if (side == e_NetSide_Spectator) return 1;
  return 2;
}

int NetworkLobbyPage::SideFromSpatialIndex(int spatial) {
  if (spatial <= 0) return e_NetSide_Home;
  if (spatial >= 2) return e_NetSide_Away;
  return e_NetSide_Spectator;
}

void NetworkLobbyPage::ChangeSide(int delta) {
  NetLobbyState state = GetState();
  uint32_t localId = GetLocalPlayerId();

  const NetLobbyPlayer *local = 0;
  for (unsigned int i = 0; i < state.players.size(); i++) {
    if (state.players.at(i).id == localId) { local = &state.players.at(i); break; }
  }

  int currentSide = local ? local->side : e_NetSide_Spectator;
  int spatial = SpatialIndex(currentSide) + delta;
  if (spatial < 0) spatial = 0;
  if (spatial > 2) spatial = 2;
  SendAction(e_NetLobbyAction_SetSide, SideFromSpatialIndex(spatial), 0);
}

void NetworkLobbyPage::ToggleReady() {
  NetLobbyState state = GetState();
  uint32_t localId = GetLocalPlayerId();

  bool ready = false;
  for (unsigned int i = 0; i < state.players.size(); i++) {
    if (state.players.at(i).id == localId) { ready = state.players.at(i).ready; break; }
  }
  SendAction(e_NetLobbyAction_SetReady, 0, ready ? 0 : 1);
}

int NetworkLobbyPage::FindLocalGamepadId() {
  const std::vector<IHIDevice*> &controllers = GetControllers();
  for (unsigned int i = 1; i < controllers.size(); i++) {
    if (controllers.at(i)->GetDeviceType() == e_HIDeviceType_Gamepad) {
      return static_cast<HIDGamepad*>(controllers.at(i))->GetGamepadID();
    }
  }
  return -1;
}

bool NetworkLobbyPage::GamepadPresent(int id) {
  if (id < 0) return false;
  const std::vector<IHIDevice*> &controllers = GetControllers();
  for (unsigned int i = 1; i < controllers.size(); i++) {
    if (controllers.at(i)->GetDeviceType() == e_HIDeviceType_Gamepad &&
        static_cast<HIDGamepad*>(controllers.at(i))->GetGamepadID() == id) {
      return true;
    }
  }
  return false;
}

void NetworkLobbyPage::ConfigureTeamsInput() {
  NetLobbyState state = GetState();
  uint32_t localId = GetLocalPlayerId();

  bool chooser = (GetChooserSide(localId) >= 0);
  int device = 0;
  for (unsigned int i = 0; i < state.players.size(); i++) {
    if (state.players.at(i).id == localId) { device = state.players.at(i).device; break; }
  }

  deviceLostSent = false;

  if (chooser && device == 1) {
    localGamepadId = FindLocalGamepadId();
    GetMenuTask()->DisableKeyboard();
    GetMenuTask()->SetActiveJoystickID(localGamepadId);
  } else {
    localGamepadId = -1;
    GetMenuTask()->EnableKeyboard();
    GetMenuTask()->SetActiveJoystickID(-1);
  }
}

void NetworkLobbyPage::RestoreInput() {
  GetMenuTask()->EnableKeyboard();
  GetMenuTask()->SetActiveJoystickID(0);
}

void NetworkLobbyPage::DrawPixelLine(boost::intrusive_ptr<Image2D> img, int x0, int y0, int x1, int y1, const Vector3 &color) {
  int dx = abs(x1 - x0);
  int dy = -abs(y1 - y0);
  int sx = x0 < x1 ? 1 : -1;
  int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  while (true) {
    img->PutPixel(x0, y0, color);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

void NetworkLobbyPage::SetReadyIndicator(int slot, bool ready) {
  if (playerReadyState.at(slot) == ready) {
    if (ready) playerReadyIcons.at(slot)->Show();
    return;
  }
  playerReadyState.at(slot) = ready;

  Gui2Image *icon = playerReadyIcons.at(slot);
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

void NetworkLobbyPage::HideSlot(int slot) {
  playerImages.at(slot)->Hide();
  playerNames.at(slot)->Hide();
  playerReadyIcons.at(slot)->Hide();
  playerReadyState.at(slot) = false;
  playerDeviceState.at(slot) = -1;
}

void NetworkLobbyPage::SetSidePhaseVisible(bool on) {
  if (on) {
    background->Show();
    phaseCaption->Show();
    side1Caption->Show();
    side2Caption->Show();
    helpCaption->Show();
  } else {
    background->Hide();
    phaseCaption->Hide();
    side1Caption->Hide();
    side2Caption->Hide();
    helpCaption->Hide();
    for (int i = 0; i < net_maxPlayers; i++) HideSlot(i);
  }
}

int NetworkLobbyPage::FirstCountryIndex(Gui2IconSelector *selector) {
  // countries are added after the special "National Teams" entry
  return selector->FindEntryIndex("national") >= 0 ? 1 : 0;
}

void NetworkLobbyPage::SelectEntryById(Gui2IconSelector *selector, int id) {
  int index = selector->FindEntryIndex(int_to_str(id));
  if (index >= 0) selector->SetSelectedEntry(index);
}

void NetworkLobbyPage::BuildTeamPanels() {
  for (int s = 0; s < 2; s++) {
    float gx = (s == 0) ? 19 : 51;

    teamBg[s] = new Gui2Image(windowManager, "image_network_lobby_teambg" + int_to_str(s), gx, 24, 30, 42);
    this->AddView(teamBg[s]);
    teamBg[s]->LoadImage("media/menu/backgrounds/black.png");

    countrySelect[s] = new Gui2IconSelector(windowManager, "net_country" + int_to_str(s), 0, 0, 29, 18, "Country select");
    leagueSelect[s] = new Gui2IconSelector(windowManager, "net_league" + int_to_str(s), 0, 0, 29, 18, "Competition select");
    teamSelect[s] = new Gui2IconSelector(windowManager, "net_team" + int_to_str(s), 0, 0, 29, 18, "Team select");
    readyButton[s] = new Gui2Button(windowManager, "net_ready" + int_to_str(s), 0, 0, 29, 3, "Ready");
    readyButton[s]->SetToggleable(true);
    readyButton[s]->sig_OnClick.connect(boost::bind(&NetworkLobbyPage::OnReadyClicked, this, s));
    leagueSelect[s]->SetDrawOutline(true);
    teamSelect[s]->SetDrawOutline(true);

    countrySelect[s]->sig_OnChange.connect(boost::bind(&NetworkLobbyPage::OnCountryChanged, this, s));
    leagueSelect[s]->sig_OnChange.connect(boost::bind(&NetworkLobbyPage::OnLeagueChanged, this, s));
    teamSelect[s]->sig_OnChange.connect(boost::bind(&NetworkLobbyPage::OnTeamChanged, this, s));
    countrySelect[s]->sig_OnClick.connect([this, s]() { leagueSelect[s]->SetFocus(); });
    leagueSelect[s]->sig_OnClick.connect([this, s]() { teamSelect[s]->SetFocus(); });

    teamGrid[s] = new Gui2Grid(windowManager, "net_teamgrid" + int_to_str(s), gx, 24, 30, 41);
    teamGrid[s]->AddView(countrySelect[s], 0, 0);
    teamGrid[s]->AddView(leagueSelect[s], 1, 0);
    teamGrid[s]->AddView(teamSelect[s], 2, 0);
    teamGrid[s]->AddView(readyButton[s], 3, 0);
    teamGrid[s]->UpdateLayout(0.5);
    this->AddView(teamGrid[s]);

    AddCountries(countrySelect[s]);
    countrySelect[s]->SetSelectedEntry(FirstCountryIndex(countrySelect[s]));
    leagueSelect[s]->SetDrawOutline(true);
    teamSelect[s]->SetDrawOutline(true);

    teamBg[s]->Hide();
    teamGrid[s]->Hide();
  }

  homePanelCaption = new Gui2Caption(windowManager, "caption_net_homepanel", 19, 20, 28, 3, "HOME");
  awayPanelCaption = new Gui2Caption(windowManager, "caption_net_awaypanel", 51, 20, 28, 3, "AWAY");
  this->AddView(homePanelCaption);
  homePanelCaption->Hide();
  this->AddView(awayPanelCaption);
  awayPanelCaption->Hide();

  teamsBuilt = true;
}

void NetworkLobbyPage::ApplyTeamState() {
  NetLobbyState state = GetState();
  uint32_t localId = GetLocalPlayerId();

  for (int s = 0; s < 2; s++) {
    int cid = state.countryId[s];
    int lid = state.leagueId[s];
    int tid = state.teamId[s];
    int stateCid = cid;

    bool cChanged = (cid != lastCountryId[s]);
    bool lChanged = (lid != lastLeagueId[s]);
    bool tChanged = (tid != lastTeamId[s]);

    if (cChanged) {
      if (cid < 0) {
        countrySelect[s]->SetSelectedEntry(FirstCountryIndex(countrySelect[s]));
      } else if (cid == 0) {
        int nationalIndex = countrySelect[s]->FindEntryIndex("national");
        if (nationalIndex >= 0) countrySelect[s]->SetSelectedEntry(nationalIndex);
      } else {
        SelectEntryById(countrySelect[s], cid);
      }

      if (cid == 0) {
        leagueSelect[s]->ClearEntries();
        teamSelect[s]->ClearEntries();
        AddTeams(teamSelect[s], GetNationalTeamsLeagueID());
        teamSelect[s]->SetDrawOutline(true);
        lastCountryId[s] = state.countryId[s];
        lastLeagueId[s] = state.leagueId[s];
        tChanged = true;
      } else {
        int effectiveCid = atoi(countrySelect[s]->GetSelectedEntryID().c_str());
        AddLeagues(leagueSelect[s], int_to_str(effectiveCid));
        leagueSelect[s]->SetDrawOutline(true);
        lastCountryId[s] = state.countryId[s];
        lChanged = true;
        tChanged = true;
      }
      defaultSent[s] = false;
    }

    if (lChanged && stateCid != 0) {
      if (lid < 0) leagueSelect[s]->SetSelectedEntry(0);
      else SelectEntryById(leagueSelect[s], lid);
      int effectiveLid = atoi(leagueSelect[s]->GetSelectedEntryID().c_str());
      AddTeams(teamSelect[s], int_to_str(effectiveLid));
      teamSelect[s]->SetDrawOutline(true);
      lastLeagueId[s] = state.leagueId[s];
      tChanged = true;
      defaultSent[s] = false;
    }

    if (tChanged) {
      if (tid < 0) teamSelect[s]->SetSelectedEntry(0);
      else SelectEntryById(teamSelect[s], tid);
      lastTeamId[s] = state.teamId[s];
    }

    bool chooser = (state.chooser[s] == localId);
    countrySelect[s]->SetSelectable(chooser);
    leagueSelect[s]->SetSelectable(chooser && state.countryId[s] != 0);
    teamSelect[s]->SetSelectable(chooser);
    readyButton[s]->SetSelectable(chooser);
    readyButton[s]->SetActive(chooser);
    readyButton[s]->SetToggled(state.teamReady[s]);

    if (chooser && !defaultSent[s]) {
      int c = atoi(countrySelect[s]->GetSelectedEntryID().c_str());
      int l = atoi(leagueSelect[s]->GetSelectedEntryID().c_str());
      int t = atoi(teamSelect[s]->GetSelectedEntryID().c_str());
      if (state.countryId[s] < 0) {
        if (c > 0) SendAction(e_NetLobbyAction_SetSelection, s, 0, c);
        if (l > 0) SendAction(e_NetLobbyAction_SetSelection, s, 1, l);
        if (t > 0) SendAction(e_NetLobbyAction_SetSelection, s, 2, t);
        defaultSent[s] = true;
      } else if (state.countryId[s] == 0) {
        if (state.teamId[s] < 0 && t > 0) {
          SendAction(e_NetLobbyAction_SetSelection, s, 2, t);
          defaultSent[s] = true;
        }
      } else if (state.leagueId[s] < 0) {
        if (l > 0) SendAction(e_NetLobbyAction_SetSelection, s, 1, l);
        if (t > 0) SendAction(e_NetLobbyAction_SetSelection, s, 2, t);
        defaultSent[s] = true;
      } else if (state.teamId[s] < 0) {
        if (t > 0) {
          SendAction(e_NetLobbyAction_SetSelection, s, 2, t);
          defaultSent[s] = true;
        }
      }
    }
  }
}

void NetworkLobbyPage::OnCountryChanged(int side) {
  NetLobbyState state = GetState();
  if (state.chooser[side] != GetLocalPlayerId()) return;

  std::string idStr = countrySelect[side]->GetSelectedEntryID();
  if (idStr == "national") {
    leagueSelect[side]->ClearEntries();
    teamSelect[side]->ClearEntries();
    AddTeams(teamSelect[side], GetNationalTeamsLeagueID());
    teamSelect[side]->SetDrawOutline(true);
    teamSelect[side]->SetSelectedEntry(0);
    int t = atoi(teamSelect[side]->GetSelectedEntryID().c_str());
    SendAction(e_NetLobbyAction_SetSelection, side, 0, 0);
    if (t > 0) SendAction(e_NetLobbyAction_SetSelection, side, 2, t);
    return;
  }

  int c = atoi(idStr.c_str());
  if (c <= 0) return;

  AddLeagues(leagueSelect[side], int_to_str(c));
  leagueSelect[side]->SetDrawOutline(true);
  leagueSelect[side]->SetSelectedEntry(0);
  int l = atoi(leagueSelect[side]->GetSelectedEntryID().c_str());

  AddTeams(teamSelect[side], int_to_str(l));
  teamSelect[side]->SetDrawOutline(true);
  teamSelect[side]->SetSelectedEntry(0);
  int t = atoi(teamSelect[side]->GetSelectedEntryID().c_str());

  SendAction(e_NetLobbyAction_SetSelection, side, 0, c);
  if (l > 0) SendAction(e_NetLobbyAction_SetSelection, side, 1, l);
  if (t > 0) SendAction(e_NetLobbyAction_SetSelection, side, 2, t);
}

void NetworkLobbyPage::OnLeagueChanged(int side) {
  NetLobbyState state = GetState();
  if (state.chooser[side] != GetLocalPlayerId()) return;

  int l = atoi(leagueSelect[side]->GetSelectedEntryID().c_str());
  if (l <= 0) return;

  AddTeams(teamSelect[side], int_to_str(l));
  teamSelect[side]->SetDrawOutline(true);
  teamSelect[side]->SetSelectedEntry(0);
  int t = atoi(teamSelect[side]->GetSelectedEntryID().c_str());

  SendAction(e_NetLobbyAction_SetSelection, side, 1, l);
  if (t > 0) SendAction(e_NetLobbyAction_SetSelection, side, 2, t);
}

void NetworkLobbyPage::OnTeamChanged(int side) {
  NetLobbyState state = GetState();
  if (state.chooser[side] != GetLocalPlayerId()) return;

  int t = atoi(teamSelect[side]->GetSelectedEntryID().c_str());
  if (t > 0) SendAction(e_NetLobbyAction_SetSelection, side, 2, t);
}

void NetworkLobbyPage::OnReadyClicked(int side) {
  NetLobbyState state = GetState();
  if (state.chooser[side] != GetLocalPlayerId()) return;

  bool ready = state.teamReady[side];
  SendAction(e_NetLobbyAction_SetTeamReady, side, ready ? 0 : 1);
}

void NetworkLobbyPage::Process() {
  Gui2View::Process();

  if (!IsHost()) {
    boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
    if (!client || client->GetState() == e_NetConnectionState_Disconnected) {
      GetMenuTask()->SetNetClient(boost::shared_ptr<NetClient>());
      CreatePage(e_PageID_NetworkMenu);
      return;
    }
  }

  NetLobbyState state = GetState();
  uint32_t localId = GetLocalPlayerId();

  if (!teamsBuilt) BuildTeamPanels();

  bool teams = (state.phase == e_NetLobbyPhase_Teams);
  if (teams != teamsVisible) {
    teamsVisible = teams;
    SetSidePhaseVisible(!teams);
    if (teams) {
      teamBg[0]->Show();
      teamBg[1]->Show();
      teamGrid[0]->Show();
      teamGrid[1]->Show();
      homePanelCaption->Show();
      awayPanelCaption->Show();
      int cs = GetChooserSide(localId);
      if (cs >= 0) countrySelect[cs]->SetFocus();
      else this->SetFocus();
      ConfigureTeamsInput();
    } else {
      teamBg[0]->Hide();
      teamBg[1]->Hide();
      teamGrid[0]->Hide();
      teamGrid[1]->Hide();
      homePanelCaption->Hide();
      awayPanelCaption->Hide();
      this->SetFocus();
      RestoreInput();
    }
  }

  if (teams) {
    ApplyTeamState();

    for (unsigned int i = 0; i < state.players.size(); i++) {
      if (state.players.at(i).id == localId && state.players.at(i).device == 1) {
        if (!deviceLostSent && !GamepadPresent(localGamepadId)) {
          deviceLostSent = true;
          SendAction(e_NetLobbyAction_DeviceLost, 0, 0);
        }
        break;
      }
    }
    return;
  }

  phaseCaption->SetCaption("choose your side");

  for (int i = 0; i < net_maxPlayers; i++) {
    if (i >= (int)state.players.size()) { HideSlot(i); continue; }

    const NetLobbyPlayer &player = state.players.at(i);
    int x = 43 + SideOffset(player.side) * 25;
    int y = 20 + i * 15;

    if (playerDeviceState.at(i) != player.device) {
      playerDeviceState.at(i) = player.device;
      playerImages.at(i)->LoadImage(player.device == 1 ?
          "media/menu/controller/controller_small.png" :
          "media/menu/controller/keyboard_small.png");
    }

    playerImages.at(i)->SetPosition(x, y);
    playerImages.at(i)->Show();

    std::string name = (player.id == localId ? "> " : "") + player.name;
    playerNames.at(i)->SetCaption(name);
    playerNames.at(i)->SetPosition(x + 7 - playerNames.at(i)->GetTextWidthPercent() * 0.5, y + 10);
    playerNames.at(i)->Show();

    playerReadyIcons.at(i)->SetPosition(x + 5.5, y + 13);
    SetReadyIndicator(i, player.ready);
  }

  helpCaption->SetCaption("Left/Right: side    Enter: ready    Esc: leave");
}

void NetworkLobbyPage::ProcessKeyboardEvent(KeyboardEvent *event) {
  if (sentDevice != 0) {
    sentDevice = 0;
    SendAction(e_NetLobbyAction_SetDevice, 0, 0);
  }

  NetLobbyState state = GetState();

  if (event->GetKeyOnce(SDLK_ESCAPE)) {
    Leave();
    return;
  }

  if (state.phase != e_NetLobbyPhase_Sides) return;

  if (event->GetKeyOnce(SDLK_LEFT)) ChangeSide(-1);
  else if (event->GetKeyOnce(SDLK_RIGHT)) ChangeSide(1);
  else if (event->GetKeyOnce(SDLK_RETURN)) ToggleReady();
}

void NetworkLobbyPage::ProcessJoystickEvent(JoystickEvent *event) {
  if (sentDevice != 1) {
    sentDevice = 1;
    SendAction(e_NetLobbyAction_SetDevice, 0, 1);
  }

  NetLobbyState state = GetState();
  if (state.phase != e_NetLobbyPhase_Sides) return;

  unsigned long now_ms = EnvironmentManager::GetInstance().GetTime_ms();
  const std::vector<IHIDevice*> &controllers = GetControllers();
  for (unsigned int i = 1; i < controllers.size(); i++) {
    if (controllers.at(i)->GetDeviceType() != e_HIDeviceType_Gamepad) continue;
    HIDGamepad *gamepad = static_cast<HIDGamepad*>(controllers.at(i));
    int joyID = gamepad->GetGamepadID();

    if (now_ms - lastGamepadSideChange_ms > 250) {
      if (gamepad->GetButtonValue(e_ButtonFunction_Left) > 0.5f) {
        ChangeSide(-1);
        lastGamepadSideChange_ms = now_ms;
      } else if (gamepad->GetButtonValue(e_ButtonFunction_Right) > 0.5f) {
        ChangeSide(1);
        lastGamepadSideChange_ms = now_ms;
      }
    }

    if (event->GetButton(joyID, gamepad->GetControllerMapping(e_ControllerButton_A))) {
      ToggleReady();
    }
  }
}

void NetworkLobbyPage::ProcessWindowingEvent(WindowingEvent *event) {
  event->Ignore();
}

void NetworkLobbyPage::Leave() {
  if (IsHost()) {
    boost::shared_ptr<NetServer> server = GetMenuTask()->GetNetServer();
    if (server) server->Stop();
    GetMenuTask()->SetNetServer(boost::shared_ptr<NetServer>());
  } else {
    boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
    if (client) client->Disconnect();
    GetMenuTask()->SetNetClient(boost::shared_ptr<NetClient>());
  }
  CreatePage(e_PageID_NetworkMenu);
}

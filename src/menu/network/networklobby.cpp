#include "networklobby.hpp"

#include "network.hpp"

#include "../pagefactory.hpp"
#include "../startmatch/teamselect.hpp"

#include "../../main.hpp"

#include "utils/gui2/events.hpp"

#include "net/netclient.hpp"
#include "net/netserver.hpp"

#include "hid/gamepad.hpp"

#include <SDL3/SDL.h>

NetworkLobbyPage::NetworkLobbyPage(Gui2WindowManager *windowManager, const Gui2PageData &pageData) : Gui2Page(windowManager, pageData) {

  teamsBuilt = false;
  matchStartTriggered = false;
  deviceLostSent = false;
  sentDevice = -1;
  lastLocalDevice = -1;
  localGamepadId = -1;
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

  homePanelCaption = new Gui2Caption(windowManager, "caption_net_homepanel", 19, 20, 28, 3, "HOME");
  awayPanelCaption = new Gui2Caption(windowManager, "caption_net_awaypanel", 51, 20, 28, 3, "AWAY");
  this->AddView(homePanelCaption);
  homePanelCaption->Hide();
  this->AddView(awayPanelCaption);
  awayPanelCaption->Hide();

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

    teamBg[s]->Show();
    teamGrid[s]->Show();
  }

  homePanelCaption->Show();
  awayPanelCaption->Show();

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

void NetworkLobbyPage::Process() {
  Gui2View::Process();

  if (!IsHost()) {
    boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
    if (!client || client->GetState() == e_NetConnectionState_Disconnected) {
      GetMenuTask()->SetNetClient(boost::shared_ptr<NetClient>());
      CreatePage(e_PageID_NetworkMenu);
      return;
    }

    // The host starts the match: build the same Match from the streamed setup.
    NetMatchSetup setup;
    if (client->ConsumeMatchSetup(setup)) {
      GetMenuTask()->SetTeamIDs(int_to_str(setup.teamId[0]), int_to_str(setup.teamId[1]));
      CreatePage(e_PageID_LoadingMatch);
      return;
    }
  }

  NetLobbyState state = GetState();

  // A join/leave or a device loss drops the lobby back to the side phase: the
  // shared SideSelectPage takes over from here.
  if (state.phase != e_NetLobbyPhase_Teams) {
    CreatePage(e_PageID_SideSelect);
    return;
  }

  if (!teamsBuilt) {
    BuildTeamPanels();
    int cs = GetChooserSide(GetLocalPlayerId());
    if (cs >= 0) countrySelect[cs]->SetFocus();
    else this->SetFocus();
    ConfigureTeamsInput();
    lastLocalDevice = -1;
  }

  // Switch menu input to the device the local peer last used (keyboard/gamepad),
  // also when it changed after the panels were built.
  uint32_t localId = GetLocalPlayerId();
  int localDevice = 0;
  for (unsigned int i = 0; i < state.players.size(); i++) {
    if (state.players.at(i).id == localId) { localDevice = state.players.at(i).device; break; }
  }
  if (localDevice != lastLocalDevice) {
    lastLocalDevice = localDevice;
    ConfigureTeamsInput();
  }

  ApplyTeamState();

  for (unsigned int i = 0; i < state.players.size(); i++) {
    if (state.players.at(i).id == GetLocalPlayerId() && state.players.at(i).device == 1) {
      if (!deviceLostSent && !GamepadPresent(localGamepadId)) {
        deviceLostSent = true;
        SendAction(e_NetLobbyAction_DeviceLost, 0, 0);
      }
      break;
    }
  }

  // Both teams chosen and confirmed by their choosers -> the host starts.
  if (IsHost() && !matchStartTriggered &&
      state.teamReady[0] && state.teamReady[1] &&
      state.teamId[0] > 0 && state.teamId[1] > 0) {
    StartHostMatch(state.teamId[0], state.teamId[1]);
  }
}

void NetworkLobbyPage::ProcessKeyboardEvent(KeyboardEvent *event) {
  // Remember the last used device so the match binds the right one (same
  // inference the side-selection screen uses).
  if (sentDevice != 0) {
    sentDevice = 0;
    SendAction(e_NetLobbyAction_SetDevice, 0, 0);
  }
  if (event->GetKeyOnce(SDLK_ESCAPE)) {
    Leave();
    return;
  }
}

void NetworkLobbyPage::ProcessJoystickEvent(JoystickEvent *event) {
  // Team selectors handle their own joystick input via focus; here we only
  // record that this peer is playing with a gamepad.
  if (sentDevice != 1) {
    sentDevice = 1;
    SendAction(e_NetLobbyAction_SetDevice, 0, 1);
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

void NetworkLobbyPage::StartHostMatch(int team0, int team1) {
  matchStartTriggered = true;

  boost::shared_ptr<NetServer> server = GetMenuTask()->GetNetServer();
  if (server) {
    NetMatchSetup setup;
    setup.teamId[0] = team0;
    setup.teamId[1] = team1;
    NetBuffer buffer;
    WriteMatchSetup(buffer, setup);
    server->BroadcastMessage(e_NetMessage_MatchSetup, buffer);
  }

  GetMenuTask()->SetTeamIDs(int_to_str(team0), int_to_str(team1));
  CreatePage(e_PageID_LoadingMatch);
}

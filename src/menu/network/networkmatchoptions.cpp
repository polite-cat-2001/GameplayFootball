#include "networkmatchoptions.hpp"

#include "network.hpp"

#include "../pagefactory.hpp"

#include "../../main.hpp"
#include "../../gamedefines.hpp"

#include "utils/gui2/events.hpp"

#include "net/netclient.hpp"
#include "net/netserver.hpp"

NetworkMatchOptionsPage::NetworkMatchOptionsPage(Gui2WindowManager *windowManager, const Gui2PageData &pageData) : Gui2Page(windowManager, pageData) {

  matchStartTriggered = false;

  Gui2Image *bg = new Gui2Image(windowManager, "net_matchoptions_bg", 30, 20, 40, 70);
  this->AddView(bg);
  bg->LoadImage("media/menu/backgrounds/black.png");
  bg->Show();

  Gui2Caption *header = new Gui2Caption(windowManager, "net_matchoptions_caption", 30, 15, 40, 3, "Match options");
  this->AddView(header);
  header->Show();

  Gui2Grid *grid = new Gui2Grid(windowManager, "net_matchoptions_grid", 35, 25, 30, 60);

  difficultySlider = new Gui2Slider(windowManager, "net_matchoptions_slider_difficulty", 0, 0, 29, 6, "difficulty (when HUMAN vs CPU)");
  durationSlider = new Gui2Slider(windowManager, "net_matchoptions_slider_duration", 0, 0, 29, 6, "match duration (5 minutes .. 25 min.)");
  startButton = new Gui2Button(windowManager, "net_matchoptions_button_start", 0, 0, 29, 3, "Start match");

  grid->AddView(difficultySlider, 0, 0);
  grid->AddView(durationSlider, 1, 0);
  grid->AddView(startButton, 2, 0);
  grid->UpdateLayout(0.5);
  this->AddView(grid);
  grid->Show();

  statusCaption = new Gui2Caption(windowManager, "net_matchoptions_status", 20, 88, 60, 3, "");
  this->AddView(statusCaption);
  statusCaption->Show();

  const bool host = IsHost();
  if (host) {
    // Host owns the values; seed from its config and push so everyone mirrors it.
    difficultySlider->SetValue(GetConfiguration()->GetReal("match_difficulty", _default_Difficulty));
    durationSlider->SetValue(GetConfiguration()->GetReal("match_duration", _default_MatchDuration));
    SendOption(0, difficultySlider->GetValue());
    SendOption(1, durationSlider->GetValue());

    difficultySlider->sig_OnChange.connect([this](Gui2Slider *slider) { SendOption(0, slider->GetValue()); });
    durationSlider->sig_OnChange.connect([this](Gui2Slider *slider) { SendOption(1, slider->GetValue()); });
    startButton->sig_OnClick.connect(boost::bind(&NetworkMatchOptionsPage::StartHostMatch, this));

    difficultySlider->SetFocus();
    statusCaption->SetCaption("Host: set options — Start match; Esc: back to teams");
  } else {
    NetLobbyState state = GetState();
    difficultySlider->SetValue(state.matchDifficulty);
    durationSlider->SetValue(state.matchDuration);
    // Spectators: display only.
    difficultySlider->SetSelectable(false);
    durationSlider->SetSelectable(false);
    startButton->SetSelectable(false);
    startButton->SetActive(false);
    this->SetFocus();
    statusCaption->SetCaption("Waiting for the host — Esc: back to teams");
  }

  this->Show();
}

NetworkMatchOptionsPage::~NetworkMatchOptionsPage() {
}

bool NetworkMatchOptionsPage::IsHost() {
  return GetMenuTask()->GetNetServer() != 0;
}

NetLobbyState NetworkMatchOptionsPage::GetState() {
  if (IsHost()) {
    boost::shared_ptr<NetServer> server = GetMenuTask()->GetNetServer();
    if (server) return server->GetLobbyState();
  } else {
    boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
    if (client) return client->GetLobbyState();
  }
  return NetLobbyState();
}

void NetworkMatchOptionsPage::SendOption(int field, float value) {
  if (!IsHost()) return;
  boost::shared_ptr<NetServer> server = GetMenuTask()->GetNetServer();
  if (!server) return;
  NetLobbyAction action;
  action.type = e_NetLobbyAction_SetMatchOptions;
  action.playerId = 0;
  action.value = field;
  action.value2 = (int)(value * 1000.0f + 0.5f);
  server->ApplyLobbyAction(action);
}

void NetworkMatchOptionsPage::SendBackToTeams() {
  NetLobbyAction action;
  action.type = e_NetLobbyAction_BackToTeams;
  if (IsHost()) {
    boost::shared_ptr<NetServer> server = GetMenuTask()->GetNetServer();
    if (server) { action.playerId = 0; server->ApplyLobbyAction(action); }
  } else {
    boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
    if (client) client->SendLobbyAction(action);
  }
}

void NetworkMatchOptionsPage::StartHostMatch() {
  if (matchStartTriggered) return;
  matchStartTriggered = true;

  GetConfiguration()->Set("match_difficulty", difficultySlider->GetValue());
  GetConfiguration()->Set("match_duration", durationSlider->GetValue());

  NetLobbyState state = GetState();
  boost::shared_ptr<NetServer> server = GetMenuTask()->GetNetServer();
  if (server) {
    NetMatchSetup setup;
    setup.teamId[0] = state.teamId[0];
    setup.teamId[1] = state.teamId[1];
    NetBuffer buffer;
    WriteMatchSetup(buffer, setup);
    server->BroadcastMessage(e_NetMessage_MatchSetup, buffer);
  }
  GetMenuTask()->SetTeamIDs(int_to_str(state.teamId[0]), int_to_str(state.teamId[1]));

  CreatePage(e_PageID_LoadingMatch);
}

void NetworkMatchOptionsPage::Process() {
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

  // A roster change (or device loss) drops the lobby back before options:
  // NetworkLobbyPage routes to the teams/sides phase as needed.
  if (state.phase != e_NetLobbyPhase_Options) {
    CreatePage(e_PageID_NetworkLobby);
    return;
  }

  if (IsHost()) return;

  // Client: mirror the host's values, and start when the setup arrives.
  difficultySlider->SetValue(state.matchDifficulty);
  durationSlider->SetValue(state.matchDuration);

  NetMatchSetup setup;
  boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
  if (client && client->ConsumeMatchSetup(setup)) {
    GetMenuTask()->SetTeamIDs(int_to_str(setup.teamId[0]), int_to_str(setup.teamId[1]));
    CreatePage(e_PageID_LoadingMatch);
    return;
  }
}

void NetworkMatchOptionsPage::ProcessKeyboardEvent(KeyboardEvent *event) {
  event->Ignore();
}

void NetworkMatchOptionsPage::ProcessJoystickEvent(JoystickEvent *event) {
  event->Ignore();
}

void NetworkMatchOptionsPage::ProcessWindowingEvent(WindowingEvent *event) {
  if (event->IsEscape()) {
    // Any peer may step back to team selection; the server flips the phase and
    // Process() reopens NetworkLobbyPage for everyone.
    SendBackToTeams();
    return;
  }
  event->Ignore();
}

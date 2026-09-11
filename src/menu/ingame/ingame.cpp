// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#include "ingame.hpp"

#include "main.hpp"
#include "../gameplan.hpp"
#include "../sideselect.hpp"
#include "../pagefactory.hpp"

#include "replaymenu.hpp"

#include "../settings.hpp"

#include "../../net/netclient.hpp"
#include "../../net/netmessages.hpp"
#include "../../net/netserver.hpp"

using namespace blunted;

IngamePage::IngamePage(Gui2WindowManager *windowManager, const Gui2PageData &pageData) : Gui2Page(windowManager, pageData) {

  teamID = pageData.properties->GetInt("teamID", 0);
  buttonContinue = 0;

  // Only the peer that opened the menu initiates the pause; peers that open it
  // because a PauseState arrived are already paused.
  if (GetGameTask()->GetMatch() && !GetGameTask()->GetMatch()->GetPause()) {
    GetGameTask()->GetMatch()->Pause(true);
  }

  Gui2Root *root = windowManager->GetRoot();

  buttonContinue = new Gui2Button(windowManager, "button_continue", 0, 0, 30, 3, "Continue");
  buttonContinue->sig_OnClick.connect(boost::bind(&IngamePage::VoteResume, this));

  const bool networkMatch = GetMenuTask()->GetNetServer() || GetMenuTask()->GetNetClient();

  Gui2Button *buttonGamePlan = new Gui2Button(windowManager, "button_gameplan", 0, 0, 30, 3, "game plan");
  // In a network match the input device is chosen on the side-selection screen,
  // which already binds controller-to-side; a local controller select would
  // rebind via UpdateControllerSetup and break the network gamer mapping.
  Gui2Button *buttonControllerSelect = networkMatch ? 0 : new Gui2Button(windowManager, "button_controllerselect", 0, 0, 30, 3, "controller select");
  Gui2Button *buttonSideSelect = new Gui2Button(windowManager, "button_sideselect", 0, 0, 30, 3, "side selection");
  Gui2Button *buttonCameraSettings = new Gui2Button(windowManager, "button_camerasettings", 0, 0, 30, 3, "camera settings");
  Gui2Button *buttonVisualOptions = new Gui2Button(windowManager, "button_visualoptions", 0, 0, 30, 3, "visual options");
  Gui2Button *buttonSystemSettings = new Gui2Button(windowManager, "button_systemsettings", 0, 0, 30, 3, "system settings");
  Gui2Button *buttonReplay = new Gui2Button(windowManager, "button_replay", 0, 0, 30, 3, "replay");
  Gui2Button *buttonPreQuit = new Gui2Button(windowManager, "button_quit", 0, 0, 30, 3, "forfeit match");

  buttonGamePlan->sig_OnClick.connect(boost::bind(&IngamePage::GoGamePlan, this));
  if (buttonControllerSelect) buttonControllerSelect->sig_OnClick.connect(boost::bind(&IngamePage::GoControllerSelect, this));
  buttonSideSelect->sig_OnClick.connect(boost::bind(&IngamePage::GoSideSelect, this));
  buttonCameraSettings->sig_OnClick.connect(boost::bind(&IngamePage::GoCameraSettings, this));
  buttonVisualOptions->sig_OnClick.connect(boost::bind(&IngamePage::GoVisualOptions, this));
  buttonSystemSettings->sig_OnClick.connect(boost::bind(&IngamePage::GoSystemSettings, this));
  buttonReplay->sig_OnClick.connect(boost::bind(&IngamePage::GoReplay, this));
  buttonPreQuit->sig_OnClick.connect(boost::bind(&IngamePage::GoPreQuit, this));


  Gui2Grid *grid = new Gui2Grid(windowManager, "grid", 10, 10, 80, 80);

  int nextRow = 0;
  grid->AddView(buttonContinue, nextRow, 0); nextRow++;
  grid->AddView(buttonGamePlan, nextRow, 0); nextRow++;
  if (buttonControllerSelect) { grid->AddView(buttonControllerSelect, nextRow, 0); nextRow++; }
  if (networkMatch) {
    grid->AddView(buttonSideSelect, nextRow, 0);
    nextRow++;
  }
  grid->AddView(buttonCameraSettings, nextRow, 0); nextRow++;
  grid->AddView(buttonVisualOptions, nextRow, 0); nextRow++;
  grid->AddView(buttonSystemSettings, nextRow, 0); nextRow++;
  grid->AddView(buttonReplay, nextRow, 0); nextRow++;
  grid->AddView(buttonPreQuit, nextRow, 0);

  grid->UpdateLayout(0.5);

  this->AddView(grid);
  grid->Show();

  buttonContinue->SetFocus();

  this->Show();
}

IngamePage::~IngamePage() {
}

void IngamePage::GoGamePlan() {
  Properties properties;
  properties.Set("teamID", teamID);
  CreatePage(e_PageID_GamePlan, properties);
}

void IngamePage::GoControllerSelect() {
  Properties properties;
  properties.SetBool("isInGame", true);
  CreatePage(e_PageID_SideSelect, properties);
}

void IngamePage::GoSideSelect() {
  // Network matches re-select sides live (mirrored); the host also re-binds the
  // input devices and AI takes over any freed side. A client asks the host.
  boost::shared_ptr<NetServer> server = GetMenuTask()->GetNetServer();
  boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
  if (server) {
    server->SetSideSelectMode(true);
  } else if (client) {
    NetLobbyAction action;
    action.type = e_NetLobbyAction_RequestSideSelect;
    action.value = 1; // != 0: open; 0 would cancel
    client->SendLobbyAction(action);
  }
  Properties properties;
  properties.SetBool("isInGame", true);
  properties.SetBool("resumeOnClose", true);
  CreatePage(e_PageID_SideSelect, properties);
}

void IngamePage::GoCameraSettings() {
  CreatePage(e_PageID_Camera);
}

void IngamePage::GoVisualOptions() {
  CreatePage(e_PageID_VisualOptions);
}

void IngamePage::GoSystemSettings() {
  CreatePage(e_PageID_Settings);
}

void IngamePage::GoReplay() {
  CreatePage(e_PageID_Replay);
}

void IngamePage::GoPreQuit() {
  CreatePage(e_PageID_PreQuit);
}


bool IngamePage::IsNetworkMatch() {
  return GetMenuTask()->GetNetServer() != 0 || GetMenuTask()->GetNetClient() != 0;
}

bool IngamePage::LocalResumeReady() {
  boost::shared_ptr<NetServer> server = GetMenuTask()->GetNetServer();
  if (server) {
    NetLobbyState state = server->GetLobbyState();
    for (unsigned int i = 0; i < state.players.size(); i++) {
      if (state.players.at(i).isHost) return state.players.at(i).resumeReady;
    }
    return false;
  }
  boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
  if (client) {
    NetLobbyState state = client->GetLobbyState();
    uint32_t localId = client->GetPlayerId();
    for (unsigned int i = 0; i < state.players.size(); i++) {
      if (state.players.at(i).id == localId) return state.players.at(i).resumeReady;
    }
  }
  return false;
}

int IngamePage::GetResumeReadyCount() {
  NetLobbyState state;
  boost::shared_ptr<NetServer> server = GetMenuTask()->GetNetServer();
  boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
  if (server) state = server->GetLobbyState();
  else if (client) state = client->GetLobbyState();
  int ready = 0;
  for (unsigned int i = 0; i < state.players.size(); i++) {
    if (state.players.at(i).resumeReady) ready++;
  }
  return ready;
}

int IngamePage::GetPeerCount() {
  boost::shared_ptr<NetServer> server = GetMenuTask()->GetNetServer();
  boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
  if (server) return (int)server->GetLobbyState().players.size();
  if (client) return (int)client->GetLobbyState().players.size();
  return 0;
}

void IngamePage::VoteResume() {
  Match *match = GetGameTask()->GetMatch();
  if (!match) return;

  // Single player / local game: resume immediately.
  if (!IsNetworkMatch()) {
    match->Pause(false);
    return;
  }

  // Network: cast (or retract) this peer's resume vote; the host resumes once
  // every peer has voted. Sub-screens are personal, but leaving the pause menu
  // is a group decision.
  NetLobbyAction action;
  action.type = e_NetLobbyAction_SetResumeReady;
  action.value = LocalResumeReady() ? 0 : 1;
  boost::shared_ptr<NetServer> server = GetMenuTask()->GetNetServer();
  if (server) {
    action.playerId = 0;
    server->ApplyLobbyAction(action);
  } else {
    boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
    if (client) client->SendLobbyAction(action);
  }
}

void IngamePage::Process() {
  Gui2Page::Process();

  if (buttonContinue) {
    if (IsNetworkMatch()) {
      // Group decision: show how many peers already want to continue.
      std::string caption = "Continue (" + int_to_str(GetResumeReadyCount()) + "/" + int_to_str(GetPeerCount()) + ")";
      if (LocalResumeReady()) caption += " - waiting for others";
      buttonContinue->SetCaption(caption);
    } else {
      buttonContinue->SetCaption("Continue");
    }
  }

  // Everyone agreed to continue (network) or the match was resumed elsewhere:
  // close the menu here too so it disappears on every PC.
  if (GetGameTask()->GetMatch() && !GetGameTask()->GetMatch()->GetPause()) {
    GoBack();
  }
}

void IngamePage::ProcessWindowingEvent(WindowingEvent *event) {
  if (event->IsEscape()) {
    // Cast/retract the resume vote and stay in the menu until everyone agrees
    // (or resume immediately in single player).
    GetMenuTask()->ReleaseAllButtons();
    VoteResume();
    event->Ignore();
    return;
  }
  Gui2Page::ProcessWindowingEvent(event);
}



PreQuitPage::PreQuitPage(Gui2WindowManager *windowManager, const Gui2PageData &pageData) : Gui2Page(windowManager, pageData) {
  Gui2Root *root = windowManager->GetRoot();
  Gui2Image *bg = new Gui2Image(windowManager, "image_prequit_bg", 30, 42.5, 40, 15);
  bg->LoadImage("media/menu/backgrounds/black.png");
  this->AddView(bg);

  Gui2Caption *restartCaption = new Gui2Caption(windowManager, "caption_prequit_info", 0, 0, 100, 3, "are you sure you want to forfeit?");
  Gui2Button *okButton = new Gui2Button(windowManager, "button_prequit_ok", 10, 0, 30, 3, "OK, forfeit");
  Gui2Button *cancelButton = new Gui2Button(windowManager, "button_prequit_cancel", 10, 0, 30, 3, "Continue match");
  okButton->sig_OnClick.connect(boost::bind(&PreQuitPage::GoMenu, this));
  cancelButton->sig_OnClick.connect(boost::bind(&PreQuitPage::GoBack, this));

  Gui2Grid *grid = new Gui2Grid(windowManager, "grid_prequit", 30, 42.5, 40, 15);

  grid->AddView(restartCaption, 0, 0);
  grid->AddView(okButton, 1, 0);
  grid->AddView(cancelButton, 2, 0);

  grid->UpdateLayout(0.5);

  this->AddView(grid);
  grid->Show();

  cancelButton->SetFocus();

  this->Show();
}

PreQuitPage::~PreQuitPage() {
}

void PreQuitPage::GoMenu() {
  this->Exit();
  GetMenuTask()->SetMenuAction(e_MenuAction_Menu);
  delete this;
}

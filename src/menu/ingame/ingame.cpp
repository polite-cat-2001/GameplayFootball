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

#include "../../hid/gamepad.hpp"

using namespace blunted;

IngamePage::IngamePage(Gui2WindowManager *windowManager, const Gui2PageData &pageData) : Gui2Page(windowManager, pageData) {

  teamID = pageData.properties->GetInt("teamID", 0);
  buttonContinue = 0;
  localTwoPlayers = !GetMenuTask()->IsNetworkMatch() && GetMenuTask()->HasTwoLocalPlayers();

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

void IngamePage::CancelResumeVote() {
  // The match must not resume behind a sub-screen: leaving the pause menu
  // withdraws this peer's Continue vote. Local two-player votes are per device.
  if (GetMenuTask()->IsNetworkMatch()) {
    if (GetMenuTask()->LocalResumeReady()) GetMenuTask()->SetLocalResumeReady(false);
  } else if (localTwoPlayers && !localResumeVotes.empty()) {
    localResumeVotes.clear();
    UpdateContinueCaption();
  }
}

void IngamePage::GoGamePlan() {
  CancelResumeVote();
  Properties properties;
  properties.Set("teamID", teamID);
  CreatePage(e_PageID_GamePlan, properties);
}

void IngamePage::GoControllerSelect() {
  CancelResumeVote();
  Properties properties;
  properties.SetBool("isInGame", true);
  CreatePage(e_PageID_SideSelect, properties);
}

void IngamePage::GoSideSelect() {
  CancelResumeVote();
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
  CancelResumeVote();
  CreatePage(e_PageID_Camera);
}

void IngamePage::GoVisualOptions() {
  CancelResumeVote();
  CreatePage(e_PageID_VisualOptions);
}

void IngamePage::GoSystemSettings() {
  CancelResumeVote();
  CreatePage(e_PageID_Settings);
}

void IngamePage::GoReplay() {
  CancelResumeVote();
  CreatePage(e_PageID_Replay);
}

void IngamePage::GoPreQuit() {
  CancelResumeVote();
  CreatePage(e_PageID_PreQuit);
}

void IngamePage::VoteResume() {
  Match *match = GetGameTask()->GetMatch();
  if (!match) return;

  // Local two-player: the GUI activation is only a no-op; each side votes with
  // its own controller (see ProcessJoystickEvent / ProcessKeyboardEvent), so one
  // player can't resume alone.
  if (localTwoPlayers) return;

  // Single player / local game: resume immediately.
  if (!GetMenuTask()->IsNetworkMatch()) {
    match->Pause(false);
    return;
  }

  // Network: cast (or retract) this peer's resume vote; the host resumes once
  // every peer has voted. Sub-screens are personal, but leaving the pause menu
  // is a group decision.
  GetMenuTask()->SetLocalResumeReady(!GetMenuTask()->LocalResumeReady());
}

bool IngamePage::ContinueButtonFocused() {
  return buttonContinue && windowManager->GetFocus() == buttonContinue;
}

void IngamePage::ToggleLocalResumeVote(int controllerID) {
  if (localResumeVotes.count(controllerID)) localResumeVotes.erase(controllerID);
  else localResumeVotes.insert(controllerID);
  UpdateContinueCaption();

  int device[2];
  GetMenuTask()->GetLocalSideDevices(device);
  int needed = 0;
  for (int i = 0; i < 2; i++) if (device[i] >= 0) needed++;
  if ((int)localResumeVotes.size() >= needed) {
    Match *match = GetGameTask()->GetMatch();
    if (match) match->Pause(false);
  }
}

void IngamePage::UpdateContinueCaption() {
  if (!buttonContinue) return;
  int device[2];
  GetMenuTask()->GetLocalSideDevices(device);
  int total = 0;
  for (int i = 0; i < 2; i++) if (device[i] >= 0) total++;
  std::string caption = "Continue (" + int_to_str((int)localResumeVotes.size()) + "/" + int_to_str(total) + ")";
  if (!localResumeVotes.empty()) caption += " - waiting for other player";
  buttonContinue->SetCaption(caption);
}

void IngamePage::ProcessKeyboardEvent(KeyboardEvent *event) {
  // Local two-player resume vote: the keyboard side (controller 0) votes with
  // Enter (while "Continue" is focused) or with Back/Esc. Back is the same vote
  // as Continue; voting again retracts it. The GUI activation is a no-op in this
  // mode, so the vote is cast exactly once, here.
  if (localTwoPlayers) {
    int device[2];
    GetMenuTask()->GetLocalSideDevices(device);
    if (device[0] == 0 || device[1] == 0) {
      bool confirm = event->GetKeyOnce(SDLK_RETURN) || event->GetKeyOnce(SDLK_KP_ENTER);
      bool back = event->GetKeyOnce(SDLK_ESCAPE);
      if (back || (confirm && ContinueButtonFocused())) {
        ToggleLocalResumeVote(0);
        event->Accept();
        return;
      }
    }
    return;
  }
  Gui2Page::ProcessKeyboardEvent(event);
}

void IngamePage::ProcessJoystickEvent(JoystickEvent *event) {
  if (localTwoPlayers) {
    int device[2];
    GetMenuTask()->GetLocalSideDevices(device);
    const std::vector<IHIDevice*> &controllers = GetControllers();
    for (unsigned int c = 1; c < controllers.size(); c++) {
      if (controllers.at(c)->GetDeviceType() != e_HIDeviceType_Gamepad) continue;
      if ((int)c != device[0] && (int)c != device[1]) continue;
      HIDGamepad *gamepad = static_cast<HIDGamepad*>(controllers.at(c));
      int joyID = gamepad->GetGamepadID();
      bool confirm = event->GetButton(joyID, gamepad->GetControllerMapping(e_ControllerButton_A));
      bool back = event->GetButton(joyID, gamepad->GetControllerMapping(e_ControllerButton_B));
      if (back || (confirm && ContinueButtonFocused())) {
        ToggleLocalResumeVote((int)c);
        event->Accept();
        return;
      }
    }
    return;
  }
  Gui2Page::ProcessJoystickEvent(event);
}

void IngamePage::Process() {
  Gui2Page::Process();

  if (buttonContinue) {
    if (GetMenuTask()->IsNetworkMatch()) {
      // Group decision: show how many peers already want to continue.
      std::string caption = "Continue (" + int_to_str(GetMenuTask()->GetResumeReadyCount()) + "/" + int_to_str(GetMenuTask()->GetPeerCount()) + ")";
      if (GetMenuTask()->LocalResumeReady()) caption += " - waiting for others";
      buttonContinue->SetCaption(caption);
    } else if (localTwoPlayers) {
      UpdateContinueCaption();
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
    // Back is the same vote as Continue: cast/retract it and stay until everyone
    // agrees (or resume immediately in single player). Local two-player votes are
    // toggled by the per-device handlers above, so only swallow the windowing
    // escape here to keep it from falling through to GoBack.
    if (localTwoPlayers) { event->Accept(); return; }
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

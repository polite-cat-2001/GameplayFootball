// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#include "setpiecetaker.hpp"

#include "main.hpp"

#include "../../onthepitch/setpiece/setpiecepresentation.hpp"

using namespace blunted;

// A held analog stick sends a direction event every tick; without a delay a short tilt would skip
// several players at once (the d-pad does not, it only edges in). Menu UI, not game logic.
static const unsigned long takerMenuMoveDelay_ms = 220;

SetPieceTakerPage::SetPieceTakerPage(Gui2WindowManager *windowManager, const Gui2PageData &pageData) : Gui2Page(windowManager, pageData) {
  match = GetGameTask() ? GetGameTask()->GetMatch() : 0;
  selectedIndex = 0;
  lastMoveTime_ms = 0;
  ownerDevice = 0;
  savedActiveJoystick = GetMenuTask()->GetActiveJoystickID();
  savedKeyboardActive = GetMenuTask()->IsKeyboardActive();

  // Only the taker's own device may drive the menu (#30): a second local player's keyboard or
  // gamepad must not scroll or confirm it. Restrict the GUI input sources for the overlay's
  // lifetime (the match is stopped, so this cannot affect gameplay).
  SetPiecePresentation *ownerPresentation = match ? match->GetSetPiecePresentation() : 0;
  if (ownerPresentation) ownerDevice = ownerPresentation->LocalKickerDevice();
  if (ownerDevice) {
    if (ownerDevice->GetDeviceType() == e_HIDeviceType_Keyboard) {
      GetMenuTask()->EnableKeyboard();
      GetMenuTask()->SetActiveJoystickID(-1);
    } else {
      GetMenuTask()->DisableKeyboard();
      GetMenuTask()->SetActiveJoystickID(ownerDevice->GetGamepadID());
    }
  }

  Gui2Caption *title = new Gui2Caption(windowManager, "caption_taker_title", 0, 7, 0, 3, "set-piece taker");
  float titleWidth = title->GetTextWidthPercent();
  title->SetPosition(50 - titleWidth * 0.5f, 7);
  this->AddView(title);
  title->Show();

  SetPiecePresentation *presentation = match ? match->GetSetPiecePresentation() : 0;
  Player *currentTaker = presentation ? presentation->GetTaker() : 0;
  if (match && match->IsInSetPiece() && currentTaker) {
    std::vector<Player*> activePlayers;
    currentTaker->GetTeam()->GetActivePlayers(activePlayers);

    float y = 15.0f;
    for (unsigned int i = 0; i < activePlayers.size(); i++) {
      Player *player = activePlayers.at(i);
      // Human keeper control is a separate effort (#31) and the unfinished path crashes: only keep
      // the keeper when it already is the taker (goal kick / throw-in), where it is a no-op.
      if (player->GetFormationEntry().role == e_PlayerRole_GK && player != currentTaker) continue;
      std::string caption = player->GetPlayerData()->GetLastName();
      if (player->GetFormationEntry().role == e_PlayerRole_GK) caption += " (GK)";
      if (player == currentTaker) caption += " *";

      Gui2Button *button = new Gui2Button(windowManager, "button_taker_" + int_to_str(player->GetID()), 30, y, 40, 3.4, caption);
      button->sig_OnClick.connect(boost::bind(&SetPieceTakerPage::OnPlayerChosen, this, player));
      this->AddView(button);
      button->Show();
      buttons.push_back(button);
      if (player == currentTaker) selectedIndex = (int)buttons.size() - 1;
      y += 3.8f;
    }

    if (!buttons.empty()) buttons.at(selectedIndex)->SetFocus();
  }

  Gui2Caption *hint = new Gui2Caption(windowManager, "caption_taker_hint", 0, 93, 0, 3, "enter: apply   escape: close");
  float hintWidth = hint->GetTextWidthPercent();
  hint->SetPosition(50 - hintWidth * 0.5f, 93);
  this->AddView(hint);
  hint->Show();

  this->Show();
}

SetPieceTakerPage::~SetPieceTakerPage() {
}

void SetPieceTakerPage::Exit() {
  // Clear the gameplay-freeze flag however the page is torn down (Escape, auto-close, or another
  // overlay replacing it), not only through CloseMenu. Also restores the GUI input sources.
  if (match) match->SetSetPieceTakerMenuOpen(false);
  GetMenuTask()->SetActiveJoystickID(savedActiveJoystick);
  if (savedKeyboardActive) GetMenuTask()->EnableKeyboard(); else GetMenuTask()->DisableKeyboard();
  Gui2Page::Exit();
}

void SetPieceTakerPage::Process() {
  Gui2Page::Process();

  // Auto-close once the set piece is over or this peer's human no longer takes it (e.g. the taker
  // was changed elsewhere, or the kick was struck).
  SetPiecePresentation *presentation = match ? match->GetSetPiecePresentation() : 0;
  if (!match || !presentation || !presentation->IsLocalTaker()) {
    CloseMenu();
    return;
  }

  // Pressing a kick button (pass / through / cross / shot) closes the menu. Ball play is frozen
  // while it is open, so the press never strikes the ball. On keyboard those are W/A/S/D, which is
  // why the menu is navigated with the arrows instead.
  if (SetPiecePresentation::IsKickButtonPressed(presentation->LocalKickerDevice())) {
    CloseMenu();
  }
}

void SetPieceTakerPage::ProcessWindowingEvent(WindowingEvent *event) {
  Vector3 direction = event->GetDirection();
  if (direction.GetLength() > 0.1f) {
    MoveSelection(direction.coords[1] > 0.0f ? 1 : -1);
    event->Accept();
    return;
  }
  if (event->IsEscape()) {
    CloseMenu();
    return;
  }
  Gui2Page::ProcessWindowingEvent(event);
}

void SetPieceTakerPage::MoveSelection(int delta) {
  if (buttons.empty()) return;
  unsigned long now = match ? match->GetActualTime_ms() : 0;
  if (lastMoveTime_ms != 0 && now - lastMoveTime_ms < takerMenuMoveDelay_ms) return;
  lastMoveTime_ms = now;
  int count = (int)buttons.size();
  selectedIndex = (selectedIndex + delta) % count;
  if (selectedIndex < 0) selectedIndex += count;
  buttons.at(selectedIndex)->SetFocus();
}

void SetPieceTakerPage::OnPlayerChosen(Player *player) {
  if (!match || !player) {
    CloseMenu();
    return;
  }
  if (match->IsRemotePresentation()) {
    // Thin client: ship the squad slot to the host over the input channel.
    int slot = player->GetTeam()->GetPlayerSlot(player->GetID());
    if (slot >= 0) match->RequestRemoteSetPieceTaker(slot);
  } else {
    // Host-authoritative: queue the change; Match::Process applies it this tick.
    match->RequestSetPieceTaker(player);
  }
  CloseMenu();
}

void SetPieceTakerPage::CloseMenu() {
  GoBack(); // Exit() clears the gameplay-freeze flag
}

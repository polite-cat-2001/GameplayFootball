// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#include "setpiecetaker.hpp"

#include "main.hpp"

#include "../../onthepitch/setpiece/setpiecepresentation.hpp"

using namespace blunted;

SetPieceTakerPage::SetPieceTakerPage(Gui2WindowManager *windowManager, const Gui2PageData &pageData) : Gui2Page(windowManager, pageData) {
  match = GetGameTask() ? GetGameTask()->GetMatch() : 0;
  selectedIndex = 0;

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
  // overlay replacing it), not only through CloseMenu.
  if (match) match->SetSetPieceTakerMenuOpen(false);
  Gui2Page::Exit();
}

void SetPieceTakerPage::Process() {
  Gui2Page::Process();

  // Auto-close once the set piece is over or this peer's human no longer takes it (e.g. the taker
  // was changed elsewhere, or the kick was struck).
  SetPiecePresentation *presentation = match ? match->GetSetPiecePresentation() : 0;
  if (!match || !presentation || !presentation->IsLocalTaker()) {
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

void SetPieceTakerPage::ProcessKeyboardEvent(KeyboardEvent *event) {
  // Arrows arrive as windowing events (Gui2Task); WASD is not mapped there, so move here.
  if (event->GetKeyRepeated(SDLK_W)) { MoveSelection(-1); event->Accept(); return; }
  if (event->GetKeyRepeated(SDLK_S)) { MoveSelection(1); event->Accept(); return; }
  Gui2Page::ProcessKeyboardEvent(event);
}

void SetPieceTakerPage::MoveSelection(int delta) {
  if (buttons.empty()) return;
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

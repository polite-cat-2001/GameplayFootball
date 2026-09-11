// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#include "visualoptions.hpp"

#include "../main.hpp"

#include "../net/netclient.hpp"

#include "onthepitch/match.hpp"

#include <cstdlib>

using namespace blunted;

VisualOptionsPage::VisualOptionsPage(Gui2WindowManager *windowManager, const Gui2PageData &pageData) : Gui2Page(windowManager, pageData) {

  Match *match = GetGameTask()->GetMatch();
  difficultyPulldown = 0;
  // A thin LAN client may override kits/weather for its own view only; the
  // host's changes are broadcast. AI difficulty is host-authoritative.
  const bool isClient = GetMenuTask()->GetNetClient() != 0;

  Gui2Frame *frame = new Gui2Frame(windowManager, "frame_visualoptions", 15, 50, 70, 40, true);
  this->AddView(frame);
  frame->Show();

  Gui2Caption *title = new Gui2Caption(windowManager, "caption_visualoptions", 5, 5, 20, 3, "Match options");
  frame->AddView(title);
  title->Show();

  Gui2Grid *grid = new Gui2Grid(windowManager, "grid_visualoptions", 5, 15, 70, 15);

  kitSelectionPulldown[0] = new Gui2Pulldown(windowManager, "pulldown_visualoptions_kitselection_t1", 0, 0, 30, 3);
  Gui2Caption *kitSelectionCaption1 = new Gui2Caption(windowManager, "caption_visualoptions_kitselection_t1", 0, 0, 20, 3, match->GetTeam(0)->GetTeamData()->GetName() + " kit");
  kitSelectionPulldown[1] = new Gui2Pulldown(windowManager, "pulldown_visualoptions_kitselection_t2", 0, 0, 30, 3);
  Gui2Caption *kitSelectionCaption2 = new Gui2Caption(windowManager, "caption_visualoptions_kitselection_t2", 0, 0, 20, 3, match->GetTeam(1)->GetTeamData()->GetName() + " kit");

  kitSelectionPulldown[0]->AddEntry("Kit 01", "team1kit01");
  kitSelectionPulldown[0]->AddEntry("Kit 02", "team1kit02");
  kitSelectionPulldown[0]->AddEntry("Kit 03", "team1kit03");
  kitSelectionPulldown[0]->AddEntry("Kit 04", "team1kit04");
  kitSelectionPulldown[1]->AddEntry("Kit 01", "team2kit01");
  kitSelectionPulldown[1]->AddEntry("Kit 02", "team2kit02");
  kitSelectionPulldown[1]->AddEntry("Kit 03", "team2kit03");
  kitSelectionPulldown[1]->AddEntry("Kit 04", "team2kit04");
  int kitIndex[2];
  for (int t = 0; t < 2; t++) {
    kitIndex[t] = match->GetTeam(t)->GetKitNumber() - 1;
    if (kitIndex[t] < 0 || kitIndex[t] > 3) kitIndex[t] = 0;
  }
  kitSelectionPulldown[0]->SetSelected(kitIndex[0]);
  kitSelectionPulldown[1]->SetSelected(kitIndex[1]);
  kitSelectionPulldown[0]->sig_OnChange.connect(boost::bind(&VisualOptionsPage::OnChangeKit, this, kitSelectionPulldown[0]));
  kitSelectionPulldown[1]->sig_OnChange.connect(boost::bind(&VisualOptionsPage::OnChangeKit, this, kitSelectionPulldown[1]));

  Gui2Button *randomizeSunButton = new Gui2Button(windowManager, "button_visualoptions_randomizesun", 0, 0, 20, 3, "Randomize sun position");
  randomizeSunButton->sig_OnClick.connect(boost::bind(&VisualOptionsPage::OnRandomizeSun, this));

  grid->AddView(kitSelectionCaption1, 0, 0);
  grid->AddView(kitSelectionPulldown[0], 0, 1);
  grid->AddView(kitSelectionCaption2, 1, 0);
  grid->AddView(kitSelectionPulldown[1], 1, 1);
  grid->AddView(randomizeSunButton, 2, 1);

  if (!isClient) {
    Gui2Caption *difficultyCaption = new Gui2Caption(windowManager, "caption_visualoptions_difficulty", 0, 0, 20, 3, "AI difficulty");
    difficultyPulldown = new Gui2Pulldown(windowManager, "pulldown_visualoptions_difficulty", 0, 0, 30, 3);
    difficultyPulldown->AddEntry("Easy", "0.20");
    difficultyPulldown->AddEntry("Normal", "0.50");
    difficultyPulldown->AddEntry("Professional", "0.80");
    difficultyPulldown->AddEntry("World class", "1.00");
    const float difficulty = match->GetMatchDifficulty();
    int difficultyIndex = 2;
    if (difficulty <= 0.35f) difficultyIndex = 0;
    else if (difficulty <= 0.65f) difficultyIndex = 1;
    else if (difficulty <= 0.90f) difficultyIndex = 2;
    else difficultyIndex = 3;
    difficultyPulldown->SetSelected(difficultyIndex);
    difficultyPulldown->sig_OnChange.connect(boost::bind(&VisualOptionsPage::OnChangeDifficulty, this, difficultyPulldown));
    grid->AddView(difficultyCaption, 3, 0);
    grid->AddView(difficultyPulldown, 3, 1);
  }

  frame->AddView(grid);
  grid->UpdateLayout(2.0f);
  grid->Show();

  if (isClient) {
    // Keep the note out of the grid: Gui2Grid sizes columns by their widest
    // view, so a wide caption would shift the kit pulldowns / sun button right.
    Gui2Caption *note = new Gui2Caption(windowManager, "caption_visualoptions_note", 5, 33, 55, 3,
                                         "Changes are local to you; AI difficulty is set by the host.");
    frame->AddView(note);
    note->Show();
  }

  kitSelectionPulldown[0]->SetFocus();

  this->Show();
}

VisualOptionsPage::~VisualOptionsPage() {
}

void VisualOptionsPage::OnRandomizeSun() {
  Match *match = GetGameTask()->GetMatch();
  match->SetRandomSunParams();
  // Hosts mirror their match state; clients keep it local.
  if (!match->IsRemotePresentation()) match->BroadcastMatchOptions();
}

void VisualOptionsPage::OnChangeKit(Gui2Pulldown *pulldown) {
  int teamID = atoi(pulldown->GetSelected().substr(4, 1).c_str()) - 1;
  int kitNumber = atoi(pulldown->GetSelected().substr(8, 2).c_str());
  Match *match = GetGameTask()->GetMatch();
  match->GetTeam(teamID)->SetKitNumber(kitNumber);
  if (!match->IsRemotePresentation()) match->BroadcastMatchOptions();
}

void VisualOptionsPage::OnChangeDifficulty(Gui2Pulldown *pulldown) {
  GetGameTask()->GetMatch()->SetMatchDifficulty((float)atof(pulldown->GetSelected().c_str()));
}

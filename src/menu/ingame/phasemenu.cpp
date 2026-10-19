// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#include "phasemenu.hpp"

#include "main.hpp"

#include "../gameplan.hpp"
#include "../pagefactory.hpp"

using namespace blunted;

MatchPhasePage::MatchPhasePage(Gui2WindowManager *windowManager, const Gui2PageData &pageData) : Gui2Page(windowManager, pageData) {

  // Opening the break screen pauses; returning here from a sub-screen keeps the
  // existing pause (and the peers' votes) intact.
  Match *match = GetGameTask()->GetMatch();
  if (match) match->SetPhaseBreak(true);
  if (match && !match->GetPause()) match->Pause(true);

  nextPhase = (e_MatchPhase)pageData.properties->GetInt("nextphase");

  if (nextPhase == e_MatchPhase_2ndHalf) phaseName = "second half";
  else if (nextPhase == e_MatchPhase_1stExtraTime) phaseName = "1st extra time";
  else if (nextPhase == e_MatchPhase_2ndExtraTime) phaseName = "2nd extra time";
  else if (nextPhase == e_MatchPhase_Penalties) phaseName = "penalties";

  buttonNext = new Gui2Button(windowManager, "button_next", 0, 0, 30, 3, "begin " + phaseName);
  Gui2Button *button1 = new Gui2Button(windowManager, "button1", 0, 0, 30, 3, "game plan");

  buttonNext->sig_OnClick.connect(boost::bind(&MatchPhasePage::ContinueGame, this));
  button1->sig_OnClick.connect(boost::bind(&MatchPhasePage::GoGamePlan, this));

  grid = new Gui2Grid(windowManager, "grid", 10, 10, 80, 80);

  grid->AddView(buttonNext, 0, 0);
  grid->AddView(button1, 1, 0);

  grid->UpdateLayout(0.5);

  this->AddView(grid);
  grid->Show();

  buttonNext->SetFocus();

  this->Show();
}

MatchPhasePage::~MatchPhasePage() {
}

void MatchPhasePage::GoGamePlan() {
  // Leaving for a sub-screen withdraws this peer's Continue vote: the match
  // must not resume while the plan is still open.
  if (GetMenuTask()->IsNetworkMatch() && GetMenuTask()->LocalResumeReady()) {
    GetMenuTask()->SetLocalResumeReady(false);
  }

  Properties properties;
  //properties.SetInt("teamID", );
  CreatePage(e_PageID_GamePlan, properties);
}

void MatchPhasePage::ContinueGame() {
  GetMenuTask()->ReleaseAllButtons();
  Match *match = GetGameTask()->GetMatch();
  if (!match) return;

  // Network: every peer votes; the host resumes once all agreed.
  if (GetMenuTask()->IsNetworkMatch()) {
    GetMenuTask()->SetLocalResumeReady(!GetMenuTask()->LocalResumeReady());
    return;
  }

  match->Pause(false);
  GoBack(); // back to gamepage
}

void MatchPhasePage::Process() {
  Gui2Page::Process();

  Match *match = GetGameTask()->GetMatch();
  if (!match) return;

  if (buttonNext && GetMenuTask()->IsNetworkMatch()) {
    std::string caption = "begin " + phaseName + " (" +
                          int_to_str(GetMenuTask()->GetResumeReadyCount()) + "/" +
                          int_to_str(GetMenuTask()->GetPeerCount()) + ")";
    if (GetMenuTask()->LocalResumeReady()) caption += " - waiting for others";
    buttonNext->SetCaption(caption);
  }

  // All peers agreed (or the match was resumed elsewhere): close this page so
  // the break screen disappears on every PC.
  if (!match->GetPause()) GoBack();
}

void MatchPhasePage::ProcessWindowingEvent(WindowingEvent *event) {
  if (event->IsEscape()) {
    ContinueGame();
    event->Ignore();
  } else {
    Gui2Page::ProcessWindowingEvent(event);
  }
}

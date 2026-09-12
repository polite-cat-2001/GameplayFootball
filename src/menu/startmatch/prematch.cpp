// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#include "prematch.hpp"

#include "main.hpp"

using namespace blunted;

namespace {
const int tabCount = 7;
const char *tabNames[tabCount] = { "Kit", "Stadium", "Kick-off", "Game plan", "Options", "Camera", "System" };
}

PreMatchPage::PreMatchPage(Gui2WindowManager *windowManager, const Gui2PageData &pageData) : Gui2Page(windowManager, pageData) {

  Gui2Image *bg = new Gui2Image(windowManager, "prematch_bg", 0, 0, 100, 100);
  bg->LoadImage("media/menu/backgrounds/black.png");
  this->AddView(bg);
  bg->Show();

  activeTab = pageData.properties->GetInt("tab", 2); // kick-off, like PES

  // The hub owns the match data so plan edits carry into the match.
  matchData = GetMenuTask()->GetMatchData();
  if (!matchData ||
      matchData->GetTeamData(0)->GetDatabaseID() != GetMenuTask()->GetTeamID(0) ||
      matchData->GetTeamData(1)->GetDatabaseID() != GetMenuTask()->GetTeamID(1)) {
    matchData = new MatchData(GetMenuTask()->GetTeamID(0), GetMenuTask()->GetTeamID(1));
    GetMenuTask()->SetMatchData(matchData);
  }
  teamData[0] = matchData->GetTeamData(0);
  teamData[1] = matchData->GetTeamData(1);

  Gui2Caption *header = new Gui2Caption(windowManager, "prematch_header", 0, 8, 100, 4,
                                        teamData[0]->GetName() + "  vs  " + teamData[1]->GetName());
  this->AddView(header);
  header->Show();

  BuildTabs();
  BuildContents();

  SelectTab(activeTab);
  tabButtons.at(activeTab)->SetFocus();
  this->Show();
}

PreMatchPage::~PreMatchPage() {
}

void PreMatchPage::BuildTabs() {
  tabGrid = new Gui2Grid(windowManager, "prematch_tabgrid", 5, 15, 90, 4);
  for (int i = 0; i < tabCount; i++) {
    Gui2Button *button = new Gui2Button(windowManager, "prematch_tab_" + int_to_str(i), 0, 0, 12.5, 3, tabNames[i]);
    button->SetToggleable(true);
    // Highlight the hovered tab immediately (PES-like); confirm enters content.
    button->sig_OnGainFocus.connect(boost::bind(&PreMatchPage::SelectTab, this, i));
    button->sig_OnClick.connect(boost::bind(&PreMatchPage::TabClicked, this, i));
    tabGrid->AddView(button, 0, i);
    tabButtons.push_back(button);
  }
  tabGrid->UpdateLayout(0.25, 0.25, 0.25, 0.25);
  this->AddView(tabGrid);
  tabGrid->Show();
}

void PreMatchPage::BuildContents() {
  contentGrid = new Gui2Grid(windowManager, "prematch_content", 20, 24, 60, 60);

  // 0: Kit
  Gui2Grid *kit = new Gui2Grid(windowManager, "prematch_content_kit", 0, 0, 60, 40);
  kit->AddView(new Gui2Caption(windowManager, "prematch_kit_info", 0, 0, 58, 3, "Home and away kits are selected automatically from the club."), 0, 0);
  kit->UpdateLayout(0.5);
  contentViews.push_back(kit);

  // 1: Stadium
  Gui2Grid *stadium = new Gui2Grid(windowManager, "prematch_content_stadium", 0, 0, 60, 40);
  stadium->AddView(new Gui2Caption(windowManager, "prematch_stadium_info", 0, 0, 58, 3, "Venue: default stadium. Weather: randomized at kick-off."), 0, 0);
  stadium->UpdateLayout(0.5);
  contentViews.push_back(stadium);

  // 2: Kick-off (lineups + start)
  Gui2Grid *kickoff = new Gui2Grid(windowManager, "prematch_content_kickoff", 0, 0, 60, 40);
  Gui2Grid *lineupGrid = new Gui2Grid(windowManager, "prematch_lineupgrid", 0, 0, 58, 30);
  for (int i = 0; i < playerNum; i++) {
    std::string name1 = teamData[0]->GetPlayerData(i)->GetLastName();
    std::string name2 = teamData[1]->GetPlayerData(i)->GetLastName();
    lineupGrid->AddView(new Gui2Caption(windowManager, "prematch_lineup1_" + int_to_str(i), 0, 0, 28, 2.4, name1), i, 0);
    lineupGrid->AddView(new Gui2Caption(windowManager, "prematch_lineup2_" + int_to_str(i), 0, 0, 28, 2.4, name2), i, 1);
  }
  lineupGrid->UpdateLayout(0.5);
  kickoff->AddView(lineupGrid, 0, 0);
  Gui2Button *startButton = new Gui2Button(windowManager, "prematch_start", 0, 0, 30, 3, "Start match");
  startButton->sig_OnClick.connect(boost::bind(&PreMatchPage::GoStartMatch, this));
  kickoff->AddView(startButton, 1, 0);
  kickoff->UpdateLayout(0.5);
  contentViews.push_back(kickoff);

  // 3: Game plan
  Gui2Grid *gameplan = new Gui2Grid(windowManager, "prematch_content_gameplan", 0, 0, 60, 40);
  Gui2Button *gameplanButton = new Gui2Button(windowManager, "prematch_gameplan_open", 0, 0, 30, 3, "Open game plan");
  gameplanButton->sig_OnClick.connect(boost::bind(&PreMatchPage::OpenPage, this, (int)e_PageID_GamePlan));
  gameplan->AddView(gameplanButton, 0, 0);
  gameplan->UpdateLayout(0.5);
  contentViews.push_back(gameplan);

  // 4: Options
  Gui2Grid *options = new Gui2Grid(windowManager, "prematch_content_options", 0, 0, 60, 40);
  difficultySlider = new Gui2Slider(windowManager, "prematch_options_difficulty", 0, 0, 40, 6, "difficulty (when HUMAN vs CPU)");
  matchDurationSlider = new Gui2Slider(windowManager, "prematch_options_duration", 0, 0, 40, 6, "match duration (5 .. 25 min.)");
  difficultySlider->SetValue(GetConfiguration()->GetReal("match_difficulty", _default_Difficulty));
  matchDurationSlider->SetValue(GetConfiguration()->GetReal("match_duration", _default_MatchDuration));
  options->AddView(difficultySlider, 0, 0);
  options->AddView(matchDurationSlider, 1, 0);
  Gui2Caption *subsInfo = new Gui2Caption(windowManager, "prematch_options_subs", 0, 0, 40, 3,
                                           "substitutions per match: " + int_to_str(maxSubstitutions));
  options->AddView(subsInfo, 2, 0);
  options->UpdateLayout(0.5);
  contentViews.push_back(options);

  // 5: Camera
  Gui2Grid *camera = new Gui2Grid(windowManager, "prematch_content_camera", 0, 0, 60, 40);
  Gui2Button *cameraButton = new Gui2Button(windowManager, "prematch_camera_open", 0, 0, 30, 3, "Camera settings");
  cameraButton->sig_OnClick.connect(boost::bind(&PreMatchPage::OpenPage, this, (int)e_PageID_Camera));
  camera->AddView(cameraButton, 0, 0);
  camera->UpdateLayout(0.5);
  contentViews.push_back(camera);

  // 6: System
  Gui2Grid *system = new Gui2Grid(windowManager, "prematch_content_system", 0, 0, 60, 40);
  Gui2Button *systemButton = new Gui2Button(windowManager, "prematch_system_open", 0, 0, 30, 3, "System settings");
  systemButton->sig_OnClick.connect(boost::bind(&PreMatchPage::OpenPage, this, (int)e_PageID_Settings));
  system->AddView(systemButton, 0, 0);
  system->UpdateLayout(0.5);
  contentViews.push_back(system);

  for (unsigned int i = 0; i < contentViews.size(); i++) {
    contentGrid->AddView(contentViews.at(i), 0, 0);
  }
  contentGrid->UpdateLayout(0.0);
  this->AddView(contentGrid);
  contentGrid->Show();
}

void PreMatchPage::SelectTab(int tab) {
  if (tab < 0 || tab >= (int)contentViews.size()) return;
  activeTab = tab;

  for (int i = 0; i < (int)tabButtons.size(); i++) {
    tabButtons.at(i)->SetToggled(i == tab);
  }

  ShowContent(tab);
}

void PreMatchPage::TabClicked(int tab) {
  SelectTab(tab);
  FocusFirstContent();
}

void PreMatchPage::FocusFirstContent() {
  const std::vector<Gui2View*> &children = contentViews.at(activeTab)->GetChildren();
  for (unsigned int i = 0; i < children.size(); i++) {
    if (children.at(i)->IsSelectable()) { children.at(i)->SetFocus(); return; }
  }
}

bool PreMatchPage::IsInContent(Gui2View *view) {
  while (view) {
    if (view == contentViews.at(activeTab)) return true;
    view = view->GetParent();
  }
  return false;
}

void PreMatchPage::ProcessWindowingEvent(WindowingEvent *event) {
  Gui2View *focus = windowManager->GetFocus();
  bool onTab = false;
  for (unsigned int i = 0; i < tabButtons.size(); i++) {
    if (tabButtons.at(i) == focus) { onTab = true; break; }
  }

  // Back is two-level: from a tab's content it returns to the tab strip (still
  // in the hub); from the tab strip it leaves the hub back to team selection.
  if (event->IsEscape()) {
    if (!onTab && IsInContent(focus)) {
      tabButtons.at(activeTab)->SetFocus();
    } else {
      GoBack();
    }
    event->Accept();
    return;
  }

  Vector3 direction = event->GetDirection();
  // Tab row sits above the content: down enters it, up returns to the tabs.
  if (onTab && direction.coords[1] > 0.75) {
    FocusFirstContent();
    event->Accept();
    return;
  }
  if (!onTab && direction.coords[1] < -0.75 && IsInContent(focus)) {
    tabButtons.at(activeTab)->SetFocus();
    event->Accept();
    return;
  }

  Gui2Page::ProcessWindowingEvent(event);
}

void PreMatchPage::ShowContent(int tab) {
  for (unsigned int i = 0; i < contentViews.size(); i++) {
    if ((int)i == tab) contentViews.at(i)->Show();
    else contentViews.at(i)->Hide();
  }
}

void PreMatchPage::OpenPage(int pageID) {
  // Remember the active tab in the (shared) page data so returning here with
  // Back recreates the hub on the same tab.
  pageData.properties->SetInt("tab", activeTab);
  Properties properties;
  CreatePage(pageID, properties);
}

void PreMatchPage::GoStartMatch() {
  GetConfiguration()->Set("match_difficulty", difficultySlider->GetValue());
  GetConfiguration()->Set("match_duration", matchDurationSlider->GetValue());
  GetConfiguration()->SaveFile(GetConfigFilename());

  this->Exit();

  Properties properties;
  windowManager->GetPageFactory()->CreatePage((int)e_PageID_LoadingMatch, properties, 0);

  delete this;
}

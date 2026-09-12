// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#include "teamselect.hpp"

#include "../../main.hpp"

#include "utils/database.hpp"

#include "../pagefactory.hpp"

using namespace blunted;

std::string GetNationalTeamsLeagueID() {
  DatabaseResult *result = GetDB()->Query("select id from leagues where name = 'National Teams' limit 1");
  std::string id;
  if (result->data.size() > 0) id = result->data.at(0).at(0); else id = "0";
  delete result;
  return id;
}

void AddCountries(Gui2IconSelector *selector) {
  // "National Teams" is a special first-stage option; national teams are picked
  // directly (no league stage) via the "national" entry id. Its icon is a game
  // asset (media/textures), not generated data — the data pipeline must not
  // own or overwrite it.
  selector->AddEntry("national", "National Teams", "media/textures/nationalteams.png");

  // only list countries that actually have leagues: the "International"
  // pseudo-country (and any future league-less country) would leave the league
  // stage empty and crash the team query. Flags are keyed by the stable TM
  // country id (countries.tm_id), not the rowid, so they survive DB rebuilds.
  DatabaseResult *result = GetDB()->Query(
      "select distinct c.id, c.tm_id, c.name from countries c "
      "join leagues l on l.country_id = c.id order by c.name");

  for (unsigned int r = 0; r < result->data.size(); r++) {
    int id = atoi(result->data.at(r).at(0).c_str());
    std::string tmId = result->data.at(r).at(1);
    std::string name = result->data.at(r).at(2).c_str();

    std::string flagPath = "databases/default/images_countries/" + tmId + ".png";
    if (!boost::filesystem::exists(flagPath)) flagPath = "media/textures/orange.jpg";
    selector->AddEntry(int_to_str(id), name, flagPath);
  }

  delete result;
  selector->Redraw();
  selector->Show();
}

void AddLeagues(Gui2IconSelector *selector, const std::string &country_id) {
  selector->ClearEntries();

  if (country_id.empty() || country_id == "0") {
    selector->Redraw();
    selector->Show();
    return;
  }

  DatabaseResult *result = GetDB()->Query("select id, name, logo_url from leagues where country_id = " + country_id + " order by coalesce(tier, 99999), name");

  for (unsigned int r = 0; r < result->data.size(); r++) {
    int id = atoi(result->data.at(r).at(0).c_str());
    std::string name = result->data.at(r).at(1).c_str();
    std::string logo_url = result->data.at(r).at(2).c_str();

    std::string logoPath = "databases/default/" + logo_url;
    if (!boost::filesystem::exists(logoPath)) logoPath = "media/textures/orange.jpg";
    selector->AddEntry(int_to_str(id), name, logoPath);
  }

  delete result;
  selector->Redraw();
  selector->Show();
}

void AddTeams(Gui2IconSelector *selector, const std::string &competition_id) {
  selector->ClearEntries();

  if (competition_id.empty() || competition_id == "0") {
    selector->Redraw();
    selector->Show();
    return;
  }

  // only list teams that can field a full starting XI (11 players); fewer makes the match freeze
  DatabaseResult *result = GetDB()->Query("select id, name, logo_url, kit_url from teams t where league_id = " + competition_id +
                                          " and (select count(*) from players p where p.team_id = t.id or p.nationalteam_id = t.id) >= 11 order by name");

  for (unsigned int r = 0; r < result->data.size(); r++) {
    int id = atoi(result->data.at(r).at(0).c_str());
    std::string name = result->data.at(r).at(1).c_str();
    std::string logo_url = result->data.at(r).at(2).c_str();

    std::string logoPath = "databases/default/" + logo_url;
    if (!boost::filesystem::exists(logoPath)) logoPath = "media/textures/orange.jpg";
    selector->AddEntry(int_to_str(id), name, logoPath);
  }

  delete result;

  selector->Redraw();
  selector->Show();
}

TeamSelectPage::TeamSelectPage(Gui2WindowManager *windowManager, const Gui2PageData &pageData) : Gui2Page(windowManager, pageData) {
  team2Initialized = false;

  Gui2Image *bg1 = new Gui2Image(windowManager, "teamselect_image_bg1", 19, 24, 30, 42);
  this->AddView(bg1);
  bg1->LoadImage("media/menu/backgrounds/black.png");
  bg1->Show();

  bg2 = new Gui2Image(windowManager, "teamselect_image_bg2", 51, 24, 30, 42);
  this->AddView(bg2);
  bg2->LoadImage("media/menu/backgrounds/black.png");

  Gui2Caption *teamEmblemCredits = new Gui2Caption(windowManager, "teamselect_emblemcredits", 19, 70, 28, 3, "Team emblems by TureckiRumun, broxopios, balder, and NLP !");
  this->AddView(teamEmblemCredits);
  teamEmblemCredits->SetColor(Vector3(200, 200, 200));
  teamEmblemCredits->SetTransparency(0.5f);
  teamEmblemCredits->SetPosition(50 - teamEmblemCredits->GetTextWidthPercent() / 2, 70);
  teamEmblemCredits->Show();

  Gui2Caption *p1 = new Gui2Caption(windowManager, "teamselect_caption_p1", 19, 20, 28, 3, "Player 1");
  p2 = new Gui2Caption(windowManager, "teamselect_caption_p2", 51, 20, 28, 3, "Player 2");
  Gui2Grid *grid1 = new Gui2Grid(windowManager, "teamselect_grid_team1", 19, 24, 30, 41);
  grid2 = new Gui2Grid(windowManager, "teamselect_grid_team2", 51, 24, 30, 41);

  countrySelect1 = new Gui2IconSelector(windowManager, "teamselect_iconselector_country1", 0, 0, 29, 18, "Country select");
  countrySelect2 = new Gui2IconSelector(windowManager, "teamselect_iconselector_country2", 0, 0, 29, 18, "Country select");
  competitionSelect1 = new Gui2IconSelector(windowManager, "teamselect_iconselector_competition1", 0, 0, 29, 18, "Competition select");
  competitionSelect2 = new Gui2IconSelector(windowManager, "teamselect_iconselector_competition2", 0, 0, 29, 18, "Competition select");
  teamSelect1 = new Gui2IconSelector(windowManager, "teamselect_iconselector_team1", 0, 0, 29, 18, "Team select");
  teamSelect2 = new Gui2IconSelector(windowManager, "teamselect_iconselector_team2", 0, 0, 29, 18, "Team select");

  // many league/team logos are mostly dark and blend into the dark selector
  // background; a white outline around the logo shape keeps them visible
  competitionSelect1->SetDrawOutline(true);
  competitionSelect2->SetDrawOutline(true);
  teamSelect1->SetDrawOutline(true);
  teamSelect2->SetDrawOutline(true);
  buttonStart1 = new Gui2Button(windowManager, "teamselect_button_start1", 0, 0, 29, 3, "Ready");
  buttonStart2 = new Gui2Button(windowManager, "teamselect_button_start2", 0, 0, 29, 3, "Ready");

  countrySelect1->sig_OnClick.connect(boost::bind(&TeamSelectPage::FocusCompetitionSelect1, this));
  competitionSelect1->sig_OnClick.connect(boost::bind(&TeamSelectPage::FocusTeamSelect1, this));
  teamSelect1->sig_OnClick.connect(boost::bind(&TeamSelectPage::FocusStart1, this));
  buttonStart1->sig_OnClick.connect(boost::bind(&TeamSelectPage::FocusCompetitionSelect2, this));
  countrySelect2->sig_OnClick.connect(boost::bind(&TeamSelectPage::FocusCompetitionSelect2, this));
  competitionSelect2->sig_OnClick.connect(boost::bind(&TeamSelectPage::FocusTeamSelect2, this));
  teamSelect2->sig_OnClick.connect(boost::bind(&TeamSelectPage::FocusStart2, this));
  buttonStart2->sig_OnClick.connect(boost::bind(&TeamSelectPage::GoOptionsMenu, this));

  countrySelect1->sig_OnChange.connect(boost::bind(&TeamSelectPage::SetupCompetitionSelect1, this));
  competitionSelect1->sig_OnChange.connect(boost::bind(&TeamSelectPage::SetupTeamSelect1, this));
  countrySelect2->sig_OnChange.connect(boost::bind(&TeamSelectPage::SetupCompetitionSelect2, this));
  competitionSelect2->sig_OnChange.connect(boost::bind(&TeamSelectPage::SetupTeamSelect2, this));

  this->AddView(p1);
  p1->Show();
  this->AddView(grid1);
  grid1->AddView(countrySelect1, 0, 0);
  grid1->AddView(competitionSelect1, 1, 0);
  grid1->AddView(teamSelect1, 2, 0);
  grid1->AddView(buttonStart1, 3, 0);
  grid1->UpdateLayout(0.5);
  grid1->Show();

  AddCountries(countrySelect1);
  countrySelect1->SetSelectedEntry(0); // default: "National Teams"
  SetupCompetitionSelect1();

  this->AddView(p2);
  this->AddView(grid2);
  grid2->AddView(countrySelect2, 0, 0);
  grid2->AddView(competitionSelect2, 1, 0);
  grid2->AddView(teamSelect2, 2, 0);
  grid2->AddView(buttonStart2, 3, 0);
  grid2->UpdateLayout(0.5);
  // team 2 selectors are populated lazily in FocusCompetitionSelect2 to keep
  // page creation fast

  countrySelect1->SetFocus();

  SetActiveController(-1, true);

  p2->Hide();
  grid2->Hide();
  bg2->Hide();

  this->Show();
}

TeamSelectPage::~TeamSelectPage() {
  GetMenuTask()->SetActiveJoystickID(0);
  GetMenuTask()->EnableKeyboard();
}

void TeamSelectPage::FocusCompetitionSelect1() {
  // national teams skip the league stage
  if (countrySelect1->GetSelectedEntryID() == "national") teamSelect1->SetFocus();
  else competitionSelect1->SetFocus();
}

void TeamSelectPage::FocusTeamSelect1() {
  teamSelect1->SetFocus();
}

void TeamSelectPage::FocusStart1() {
  buttonStart1->SetFocus();
}

void TeamSelectPage::FocusCompetitionSelect2() {
  if (!team2Initialized) {
    team2Initialized = true;
    AddCountries(countrySelect2);
    countrySelect2->SetSelectedEntry(0); // default: "National Teams"
    SetupCompetitionSelect2();
  }

  p2->Show();
  grid2->Show();
  bg2->Show();

  // national teams skip the league stage
  if (countrySelect2->GetSelectedEntryID() == "national") teamSelect2->SetFocus();
  else competitionSelect2->SetFocus();

  SetActiveController(1, true);
}

void TeamSelectPage::FocusTeamSelect2() {
  teamSelect2->SetFocus();
}

void TeamSelectPage::FocusStart2() {
  buttonStart2->SetFocus();
}

void TeamSelectPage::SetupCompetitionSelect1() {
  if (countrySelect1->GetSelectedEntryID() == "national") {
    competitionSelect1->ClearEntries();
    competitionSelect1->SetSelectable(false);
    teamSelect1->ClearEntries();
    AddTeams(teamSelect1, GetNationalTeamsLeagueID());
  } else {
    competitionSelect1->SetSelectable(true);
    AddLeagues(competitionSelect1, countrySelect1->GetSelectedEntryID());
    teamSelect1->ClearEntries();
    AddTeams(teamSelect1, competitionSelect1->GetSelectedEntryID());
  }
}

void TeamSelectPage::SetupCompetitionSelect2() {
  if (countrySelect2->GetSelectedEntryID() == "national") {
    competitionSelect2->ClearEntries();
    competitionSelect2->SetSelectable(false);
    teamSelect2->ClearEntries();
    AddTeams(teamSelect2, GetNationalTeamsLeagueID());
  } else {
    competitionSelect2->SetSelectable(true);
    AddLeagues(competitionSelect2, countrySelect2->GetSelectedEntryID());
    teamSelect2->ClearEntries();
    AddTeams(teamSelect2, competitionSelect2->GetSelectedEntryID());
  }
}

void TeamSelectPage::SetupTeamSelect1() {
  teamSelect1->ClearEntries();
  AddTeams(teamSelect1, competitionSelect1->GetSelectedEntryID());
}

void TeamSelectPage::SetupTeamSelect2() {
  teamSelect2->ClearEntries();
  AddTeams(teamSelect2, competitionSelect2->GetSelectedEntryID());
}

void TeamSelectPage::GoOptionsMenu() {
  GetMenuTask()->SetTeamIDs(teamSelect1->GetSelectedEntryID(), teamSelect2->GetSelectedEntryID());
  //printf("teams: %i vs %i\n", atoi(teamSelect1->GetSelectedEntryID().c_str()), atoi(teamSelect2->GetSelectedEntryID().c_str()));

  this->Exit();

  Properties properties;
  windowManager->GetPageFactory()->CreatePage((int)e_PageID_MatchOptions, properties, 0);

  delete this;
}

void TeamSelectPage::ProcessWindowingEvent(WindowingEvent *event) {
  if (event->IsEscape()) {
    if (windowManager->GetFocus() == countrySelect1) {
      Gui2Page::ProcessWindowingEvent(event);
    } else if (windowManager->GetFocus() == competitionSelect1) {
      windowManager->SetFocus(countrySelect1);
    } else if (windowManager->GetFocus() == teamSelect1) {
      if (countrySelect1->GetSelectedEntryID() == "national") windowManager->SetFocus(countrySelect1);
      else windowManager->SetFocus(competitionSelect1);
    } else if (windowManager->GetFocus() == buttonStart1) {
      windowManager->SetFocus(teamSelect1);
    } else if (windowManager->GetFocus() == countrySelect2) {
      windowManager->SetFocus(buttonStart1);

      p2->Hide();
      grid2->Hide();
      bg2->Hide();

      SetActiveController(-1, true);

    } else if (windowManager->GetFocus() == competitionSelect2) {
      windowManager->SetFocus(countrySelect2);
    } else if (windowManager->GetFocus() == teamSelect2) {
      if (countrySelect2->GetSelectedEntryID() == "national") windowManager->SetFocus(countrySelect2);
      else windowManager->SetFocus(competitionSelect2);
    } else if (windowManager->GetFocus() == buttonStart2) {
      windowManager->SetFocus(teamSelect2);
    }

  }

}

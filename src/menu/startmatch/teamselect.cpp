// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#include "teamselect.hpp"

#include "../../main.hpp"

#include "utils/database.hpp"

#include "hid/gamepad.hpp"

#include "managers/environmentmanager.hpp"
#include "managers/usereventmanager.hpp"

#include "../pagefactory.hpp"

#include <SDL3/SDL.h>

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

namespace {

  struct TeamSelectionPath {
    bool national = false;
    std::string country;
    std::string league;
    std::string team;
  };

  // Resolve the country/league/team entry ids a stored team id belongs to, so a
  // returning page can preselect the same carousel positions. National teams
  // live in the league-less "National Teams" bucket (country_id NULL).
  bool ResolveTeamSelection(int teamID, TeamSelectionPath &path) {
    if (teamID <= 0) return false;

    DatabaseResult *result = GetDB()->Query("select league_id from teams where id = " + int_to_str(teamID) + " limit 1");
    if (result->data.size() == 0) { delete result; return false; }
    std::string leagueID = result->data.at(0).at(0);
    delete result;
    if (leagueID.empty() || leagueID == "0") return false;

    result = GetDB()->Query("select country_id, name from leagues where id = " + leagueID + " limit 1");
    if (result->data.size() == 0) { delete result; return false; }
    std::string countryID = result->data.at(0).at(0);
    std::string leagueName = result->data.at(0).at(1);
    delete result;

    path.team = int_to_str(teamID);
    path.league = leagueID;
    if (countryID.empty() || countryID == "0" || leagueName == "National Teams") {
      path.national = true;
      path.country = "national";
    } else {
      path.national = false;
      path.country = countryID;
    }
    return true;
  }

}

TeamSelectPage::TeamSelectPage(Gui2WindowManager *windowManager, const Gui2PageData &pageData) : Gui2Page(windowManager, pageData) {
  for (int s = 0; s < 2; s++) {
    countrySelect[s] = 0;
    competitionSelect[s] = 0;
    teamSelect[s] = 0;
    readyButton[s] = 0;
    teamGrid[s] = 0;
    teamBg[s] = 0;
    panelCaption[s] = 0;
    cursorRow[s] = e_Row_Country;
    sideReady[s] = false;
    sideActive[s] = false;
    deviceKind[s] = -1;
    deviceGamepad[s] = -1;
    lastMove_ms[s] = 0;
    lastRowMove_ms[s] = 0;
  }

  // Resolve which device controls each side from the queued side selection. Two
  // locally controlled sides (with different devices) mean both players may pick
  // their team at the same time; otherwise the CPU opponent's team is picked in
  // turn with the single device.
  parallel = false;
  {
    const std::vector<SideSelection> sides = GetMenuTask()->GetControllerSetup();
    int kind[2] = { -1, -1 };
    int pad[2] = { -1, -1 };
    for (unsigned int i = 0; i < sides.size(); i++) {
      int idx = -1;
      if (sides.at(i).side < 0) idx = 0;
      else if (sides.at(i).side > 0) idx = 1;
      if (idx < 0) continue;
      int ci = sides.at(i).controllerID;
      if (ci >= 0 && ci < (signed int)GetControllers().size() &&
          GetControllers().at(ci)->GetDeviceType() == e_HIDeviceType_Gamepad) {
        kind[idx] = 1;
        pad[idx] = static_cast<HIDGamepad*>(GetControllers().at(ci))->GetGamepadID();
      } else {
        kind[idx] = 0;
        pad[idx] = -1;
      }
    }
    if (kind[0] < 0) kind[0] = 0; // fall back to the keyboard for the first human
    parallel = (kind[0] >= 0 && kind[1] >= 0);
    if (kind[0] == 0 && kind[1] == 0) parallel = false;                 // one keyboard can't drive two cursors
    if (kind[0] == 1 && kind[1] == 1 && pad[0] == pad[1]) parallel = false; // same gamepad twice
    for (int s = 0; s < 2; s++) { deviceKind[s] = kind[s]; deviceGamepad[s] = pad[s]; }
  }

  Gui2Caption *teamEmblemCredits = new Gui2Caption(windowManager, "teamselect_emblemcredits", 19, 70, 28, 3, "Team emblems by TureckiRumun, broxopios, balder, and NLP !");
  this->AddView(teamEmblemCredits);
  teamEmblemCredits->SetColor(Vector3(200, 200, 200));
  teamEmblemCredits->SetTransparency(0.5f);
  teamEmblemCredits->SetPosition(50 - teamEmblemCredits->GetTextWidthPercent() / 2, 70);
  teamEmblemCredits->Show();

  Gui2Caption *helpCaption = new Gui2Caption(windowManager, "teamselect_help", 20, 88, 60, 3, "Up/Down: level    Left/Right: select    Ready button: confirm    Esc/B: back");
  this->AddView(helpCaption);
  helpCaption->Show();

  BuildSide(0);
  BuildSide(1);

  // Returning from the pre-match hub keeps the teams each side had there.
  if (pageData.properties && pageData.properties->GetBool("restoreSelections")) {
    RestoreSelection(0, GetMenuTask()->GetTeamID(0));
    RestoreSelection(1, GetMenuTask()->GetTeamID(1));
  }

  cursorRow[0] = e_Row_Country;
  cursorRow[1] = e_Row_Country;
  sideActive[0] = true;
  sideActive[1] = parallel;

  ShowSide(0);
  if (parallel) ShowSide(1); else HideSide(1);

  HighlightSide(0);
  HighlightSide(1);

  // Drive both panels ourselves from raw keyboard/joystick events: the global
  // menu focus only ever has one cursor, so it must not translate input.
  GetMenuTask()->SetActiveJoystickID(-1);
  GetMenuTask()->DisableKeyboard();

  this->SetFocus();

  this->Show();
}

TeamSelectPage::~TeamSelectPage() {
  GetMenuTask()->SetActiveJoystickID(0);
  GetMenuTask()->EnableKeyboard();
}

void TeamSelectPage::BuildSide(int s) {
  float gx = (s == 0) ? 19 : 51;

  teamBg[s] = new Gui2Image(windowManager, "teamselect_image_bg" + int_to_str(s + 1), gx, 24, 30, 42);
  this->AddView(teamBg[s]);
  teamBg[s]->LoadImage("media/menu/backgrounds/black.png");

  panelCaption[s] = new Gui2Caption(windowManager, "teamselect_caption_p" + int_to_str(s + 1), gx, 20, 28, 3, (s == 0) ? "Player 1" : "Player 2");
  this->AddView(panelCaption[s]);

  countrySelect[s] = new Gui2IconSelector(windowManager, "teamselect_iconselector_country" + int_to_str(s + 1), 0, 0, 29, 18, "Country select");
  competitionSelect[s] = new Gui2IconSelector(windowManager, "teamselect_iconselector_competition" + int_to_str(s + 1), 0, 0, 29, 18, "Competition select");
  teamSelect[s] = new Gui2IconSelector(windowManager, "teamselect_iconselector_team" + int_to_str(s + 1), 0, 0, 29, 18, "Team select");

  // many league/team logos are mostly dark and blend into the dark selector
  // background; a white outline around the logo shape keeps them visible
  competitionSelect[s]->SetDrawOutline(true);
  teamSelect[s]->SetDrawOutline(true);

  readyButton[s] = new Gui2Button(windowManager, "teamselect_button_start" + int_to_str(s + 1), 0, 0, 29, 3, "Ready");
  readyButton[s]->SetToggleable(true);

  countrySelect[s]->sig_OnChange.connect(boost::bind(&TeamSelectPage::SetupCompetitionSelect, this, s));
  competitionSelect[s]->sig_OnChange.connect(boost::bind(&TeamSelectPage::SetupTeamSelect, this, s));

  teamGrid[s] = new Gui2Grid(windowManager, "teamselect_grid_team" + int_to_str(s + 1), gx, 24, 30, 41);
  teamGrid[s]->AddView(countrySelect[s], 0, 0);
  teamGrid[s]->AddView(competitionSelect[s], 1, 0);
  teamGrid[s]->AddView(teamSelect[s], 2, 0);
  teamGrid[s]->AddView(readyButton[s], 3, 0);
  teamGrid[s]->UpdateLayout(0.5);
  this->AddView(teamGrid[s]);

  AddCountries(countrySelect[s]);
  int nationalIndex = countrySelect[s]->FindEntryIndex("national");
  countrySelect[s]->SetSelectedEntry(nationalIndex >= 0 ? nationalIndex : 0);
  SetupCompetitionSelect(s);
}

void TeamSelectPage::RestoreSelection(int s, int teamID) {
  TeamSelectionPath path;
  if (!ResolveTeamSelection(teamID, path)) return;

  int countryIndex = countrySelect[s]->FindEntryIndex(path.country);
  if (countryIndex < 0) return;
  countrySelect[s]->SetSelectedEntry(countryIndex);
  SetupCompetitionSelect(s);

  if (!path.national) {
    int leagueIndex = competitionSelect[s]->FindEntryIndex(path.league);
    if (leagueIndex >= 0) competitionSelect[s]->SetSelectedEntry(leagueIndex);
    SetupTeamSelect(s);
  }

  int teamIndex = teamSelect[s]->FindEntryIndex(path.team);
  if (teamIndex >= 0) teamSelect[s]->SetSelectedEntry(teamIndex);
}

void TeamSelectPage::ShowSide(int s) {
  teamBg[s]->Show();
  panelCaption[s]->Show();
  teamGrid[s]->Show();
}

void TeamSelectPage::HideSide(int s) {
  teamBg[s]->Hide();
  panelCaption[s]->Hide();
  teamGrid[s]->Hide();
}

bool TeamSelectPage::IsNational(int s) {
  return countrySelect[s]->GetSelectedEntryID() == "national";
}

void TeamSelectPage::HighlightSide(int s) {
  bool active = sideActive[s] && !sideReady[s];
  countrySelect[s]->SetHighlighted(active && cursorRow[s] == e_Row_Country);
  competitionSelect[s]->SetHighlighted(active && cursorRow[s] == e_Row_Competition);
  teamSelect[s]->SetHighlighted(active && cursorRow[s] == e_Row_Team);
  readyButton[s]->SetHighlighted(active && cursorRow[s] == e_Row_Ready);
}

void TeamSelectPage::MoveSideSelection(int s, int delta) {
  if (sideReady[s]) return;
  Gui2IconSelector *sel = 0;
  if (cursorRow[s] == e_Row_Country) sel = countrySelect[s];
  else if (cursorRow[s] == e_Row_Competition) sel = competitionSelect[s];
  else if (cursorRow[s] == e_Row_Team) sel = teamSelect[s];
  if (sel) sel->MoveSelection(delta);
}

void TeamSelectPage::MoveSideRow(int s, int delta) {
  if (sideReady[s] || delta == 0) return;
  int row = cursorRow[s];
  for (int guard = 0; guard < 4; guard++) {
    row += delta;
    if (row < e_Row_Country) row = e_Row_Ready;
    if (row > e_Row_Ready) row = e_Row_Country;
    if (row != e_Row_Competition || !IsNational(s)) break; // national teams skip the league level
  }
  cursorRow[s] = row;
  HighlightSide(s);
}

void TeamSelectPage::ActivateSide(int s) {
  if (sideReady[s]) return;
  // Enter/A advances to the next section; Up/Down jump between sections too and
  // Ready confirms the currently shown selection.
  if (cursorRow[s] == e_Row_Country) {
    cursorRow[s] = IsNational(s) ? e_Row_Team : e_Row_Competition;
  } else if (cursorRow[s] == e_Row_Competition) {
    cursorRow[s] = e_Row_Team;
  } else if (cursorRow[s] == e_Row_Team) {
    cursorRow[s] = e_Row_Ready;
  } else if (cursorRow[s] == e_Row_Ready) {
    SetSideReady(s, true); // may navigate away and delete this
    return;
  }
  HighlightSide(s);
}

void TeamSelectPage::CancelSide(int s) {
  if (sideReady[s]) { SetSideReady(s, false); return; }
  if (cursorRow[s] == e_Row_Ready) {
    cursorRow[s] = e_Row_Team;
  } else if (cursorRow[s] == e_Row_Team) {
    cursorRow[s] = IsNational(s) ? e_Row_Country : e_Row_Competition;
  } else if (cursorRow[s] == e_Row_Competition) {
    cursorRow[s] = e_Row_Country;
  } else if (cursorRow[s] == e_Row_Country) {
    if (!parallel && s == 1) {
      // CPU-opponent case: step back to the home side's Ready instead of leaving
      sideActive[1] = false;
      sideActive[0] = true;
      sideReady[0] = false;
      readyButton[0]->SetToggled(false);
      deviceKind[1] = -1;
      deviceGamepad[1] = -1;
      cursorRow[0] = e_Row_Ready;
      cursorRow[1] = e_Row_Country;
      HideSide(1);
      HighlightSide(0);
      HighlightSide(1);
      return;
    }
    GoBack(); // may delete this
    return;
  }
  HighlightSide(s);
}

void TeamSelectPage::SetSideReady(int s, bool ready) {
  sideReady[s] = ready;
  readyButton[s]->SetToggled(ready);

  if (!ready) { HighlightSide(s); return; }

  if (parallel) {
    if (sideReady[0] && sideReady[1]) { GoOptionsMenu(); return; } // may delete this
  } else {
    if (s == 0) { RevealAway(); return; }
    GoOptionsMenu(); // may delete this
    return;
  }
  HighlightSide(s);
}

void TeamSelectPage::RevealAway() {
  // the single device now drives the CPU opponent's panel
  deviceKind[1] = deviceKind[0];
  deviceGamepad[1] = deviceGamepad[0];
  sideActive[0] = false;
  sideActive[1] = true;
  cursorRow[1] = e_Row_Country;
  ShowSide(1);
  HighlightSide(0);
  HighlightSide(1);
}

void TeamSelectPage::SetupCompetitionSelect(int s) {
  if (IsNational(s)) {
    competitionSelect[s]->ClearEntries();
    competitionSelect[s]->SetSelectable(false);
    teamSelect[s]->ClearEntries();
    AddTeams(teamSelect[s], GetNationalTeamsLeagueID());
  } else {
    competitionSelect[s]->SetSelectable(true);
    AddLeagues(competitionSelect[s], countrySelect[s]->GetSelectedEntryID());
    teamSelect[s]->ClearEntries();
    AddTeams(teamSelect[s], competitionSelect[s]->GetSelectedEntryID());
  }
}

void TeamSelectPage::SetupTeamSelect(int s) {
  teamSelect[s]->ClearEntries();
  AddTeams(teamSelect[s], competitionSelect[s]->GetSelectedEntryID());
}

HIDGamepad *TeamSelectPage::FindGamepad(int gamepadID) {
  const std::vector<IHIDevice*> &controllers = GetControllers();
  for (unsigned int c = 1; c < controllers.size(); c++) {
    if (controllers.at(c)->GetDeviceType() == e_HIDeviceType_Gamepad &&
        static_cast<HIDGamepad*>(controllers.at(c))->GetGamepadID() == gamepadID) {
      return static_cast<HIDGamepad*>(controllers.at(c));
    }
  }
  return 0;
}

void TeamSelectPage::GoOptionsMenu() {
  GetMenuTask()->SetTeamIDs(teamSelect[0]->GetSelectedEntryID(), teamSelect[1]->GetSelectedEntryID());
  // Remember (in this page's shared page data) that a later Back from the hub
  // must restore these teams instead of falling back to the defaults.
  if (pageData.properties) pageData.properties->SetBool("restoreSelections", true);

  this->Exit();

  Properties properties;
  windowManager->GetPageFactory()->CreatePage((int)e_PageID_PreMatch, properties, 0);

  delete this;
}

void TeamSelectPage::ProcessKeyboardEvent(KeyboardEvent *event) {
  int s = -1;
  for (int i = 0; i < 2; i++) {
    if (sideActive[i] && deviceKind[i] == 0) { s = i; break; }
  }
  if (s < 0) return;

  unsigned long now_ms = EnvironmentManager::GetInstance().GetTime_ms();
  bool up = event->GetKeyRepeated(SDLK_UP);
  bool down = event->GetKeyRepeated(SDLK_DOWN);
  if ((up || down) && now_ms - lastRowMove_ms[s] > 200) {
    if (up && !down) MoveSideRow(s, -1);
    else if (down && !up) MoveSideRow(s, 1);
    lastRowMove_ms[s] = now_ms;
  }

  if (event->GetKeyRepeated(SDLK_LEFT)) MoveSideSelection(s, -1);
  if (event->GetKeyRepeated(SDLK_RIGHT)) MoveSideSelection(s, 1);
  if (event->GetKeyOnce(SDLK_RETURN)) ActivateSide(s);       // may delete this
  else if (event->GetKeyOnce(SDLK_ESCAPE)) CancelSide(s);    // may delete this
}

void TeamSelectPage::Process() {
  Gui2View::Process();

  // Direction is polled every frame so that both the analog stick and the
  // digital D-pad move (the D-pad is not part of the default gamepad function
  // mapping, which only covers the left stick). Polling also makes a held
  // D-pad repeat, since it produces no per-frame joystick events.
  unsigned long now_ms = EnvironmentManager::GetInstance().GetTime_ms();
  for (int s = 0; s < 2; s++) {
    if (!sideActive[s] || deviceKind[s] != 1 || sideReady[s]) continue;
    HIDGamepad *gamepad = FindGamepad(deviceGamepad[s]);
    if (!gamepad) continue;
    if (now_ms - lastMove_ms[s] <= 250) continue;

    int joyID = gamepad->GetGamepadID();
    float up = gamepad->GetButtonValue(e_ButtonFunction_Up);
    float down = gamepad->GetButtonValue(e_ButtonFunction_Down);
    float left = gamepad->GetButtonValue(e_ButtonFunction_Left);
    float right = gamepad->GetButtonValue(e_ButtonFunction_Right);
    UserEventManager &userEvents = UserEventManager::GetInstance();
    if (userEvents.GetJoyButtonState(joyID, SDL_GAMEPAD_BUTTON_DPAD_UP)) up = 1.0f;
    if (userEvents.GetJoyButtonState(joyID, SDL_GAMEPAD_BUTTON_DPAD_DOWN)) down = 1.0f;
    if (userEvents.GetJoyButtonState(joyID, SDL_GAMEPAD_BUTTON_DPAD_LEFT)) left = 1.0f;
    if (userEvents.GetJoyButtonState(joyID, SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) right = 1.0f;

    if (up > 0.5f && up >= down) {
      MoveSideRow(s, -1);
      lastMove_ms[s] = now_ms;
    } else if (down > 0.5f) {
      MoveSideRow(s, 1);
      lastMove_ms[s] = now_ms;
    } else if (left > 0.5f) {
      MoveSideSelection(s, -1);
      lastMove_ms[s] = now_ms;
    } else if (right > 0.5f) {
      MoveSideSelection(s, 1);
      lastMove_ms[s] = now_ms;
    }
  }
}

void TeamSelectPage::ProcessJoystickEvent(JoystickEvent *event) {
  for (int s = 0; s < 2; s++) {
    if (!sideActive[s] || deviceKind[s] != 1) continue;
    HIDGamepad *gamepad = FindGamepad(deviceGamepad[s]);
    if (!gamepad) continue;
    int joyID = gamepad->GetGamepadID();

    if (event->GetButton(joyID, gamepad->GetControllerMapping(e_ControllerButton_B))) {
      CancelSide(s); // may delete this
      return;
    }
    if (event->GetButton(joyID, gamepad->GetControllerMapping(e_ControllerButton_A))) {
      ActivateSide(s); // may delete this
      return;
    }
  }
}

void TeamSelectPage::ProcessWindowingEvent(WindowingEvent *event) {
  // input is routed per side from raw events above; the base page's Escape
  // handling (GoBack) must not fire on top of that
  event->Ignore();
}

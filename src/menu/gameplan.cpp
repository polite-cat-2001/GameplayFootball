// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#include "gameplan.hpp"

#include "tacticschemes.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>

#include <SDL3/SDL.h>

#include "../main.hpp"

#include "../net/netclient.hpp"
#include "../net/netmessages.hpp"
#include "../net/netserver.hpp"
#include "../onthepitch/match.hpp"
#include "../onthepitch/team.hpp"
#include "../hid/gamepad.hpp"
#include "../hid/keyboard.hpp"
#include "../managers/environmentmanager.hpp"
#include "../managers/usereventmanager.hpp"

using namespace blunted;

namespace {
const float pitchY = 12.0f, pitchH = 61.0f;

// single editable team + read-only opponent (1 player / vs AI / pre-match)
const float s_px = 0.5f, s_pw = 37.0f;
const float s_bx = 39.0f, s_bw = 20.0f;
const float s_oppx = 62.0f, s_oppw = 37.0f;

// two editable teams side by side (local 2 players); bench right of each pitch.
// Benches are wide enough for a full squad name plus the rating/condition columns.
const float d0_px = 1.0f, d0_pw = 29.0f, d0_bx = 30.5f, d0_bw = 18.0f;
const float d1_px = 52.0f, d1_pw = 29.0f, d1_bx = 81.5f, d1_bw = 18.0f;

// Pitch card: the photo keeps its size and defines the card width; role+rating
// and the name are strips of exactly that width (PES-style), and a thin fatigue
// bar caps the bottom. The name can never spill onto a neighbouring card.
const float photoSize = 5.5f;
// portrait photo aspect (height / width): the shipped cut-outs are 160x208
const float photoAspect = 208.0f / 160.0f;
const float badgeH = 1.9f;
const float cardNameH = 2.1f;
const float cardGap = 0.15f;
const float fatigueBarH = 0.55f;
// gap between the card content and the selection frame: the frame is bigger
// than the content on every side, so its border never clips the name or the
// fatigue bar
const float framePad = 0.9f;
// content height (photo + role + name + bar) and worst-case content height
const float pitchCardContentH = photoSize * photoAspect + cardGap * 3.0f + badgeH + cardNameH + fatigueBarH;
const float pitchCardH = pitchCardContentH;

const float benchPosW = 3.0f;
const float benchRatingW = 3.0f;
const float benchFatigueW = 3.4f;
const float benchRowH = 2.5f;

// bottom section bar (Tactics / Positions / Roles), one per side
const float sectionBtnW = 15.0f;
const float sectionBtnH = 3.4f;
const float sectionBtnGap = 1.5f;
const float sectionBarY = 92.4f;
const float sectionCaptionY = 96.2f;
const float sectionCaptionH = 2.4f;
// the bar of a side sits in its screen half, centered on the side's panel
const float sectionHalfCenterL = 25.0f;
const float sectionHalfCenterR = 75.0f;

// tactics section: scheme rows fill the bench column
const float schemeRowMaxH = 4.6f;

// roles section: role rows fill the band between the pitch and the section bar.
// Display order (captain, then set-piece takers) is the user-facing one, not the
// enum's internal order.
const e_TeamRole planRoleOrder[e_TeamRole_SIZE] = {
  e_TeamRole_Captain,
  e_TeamRole_FreeKickTakerFar,
  e_TeamRole_FreeKickTakerNear,
  e_TeamRole_PenaltyTaker,
  e_TeamRole_CornerTakerLeft,
  e_TeamRole_CornerTakerRight
};
const float roleRowMaxH = 2.6f;
const float roleBandGap = 0.8f;
const float rolePickerPhotoW = 7.5f;
const float rolePickerPhotoH = rolePickerPhotoW * photoAspect;

// bottom info band (focused player): portrait photo with the badge/name strips
// centered below it
const float infoPhotoW = 6.0f;
const float infoPhotoH = infoPhotoW * photoAspect;
const float infoPhotoY = 78.4f;
const float infoBadgeH = 2.2f;
const float infoNameH = 2.4f;

Vector3 PitchToScreen(const Vector3 &pos, float x, float y, float w, float h) {
  float depth = pos.coords[0] * 0.5f + 0.5f; // 0 == own goal (bottom), 1 == opponent goal (top)
  float side = pos.coords[1] * 0.5f + 0.5f;  // 0 == left, 1 == right
  return Vector3(x + (1.0f - side) * w, y + (1.0f - depth) * h, 0);
}

float Clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Wide players are pulled this far inward for on-screen cards, so the portrait
// cards stay inside the pitch line. The tactical schemes bake the same limit
// into their LM/RM positions, but database formations (the initially set
// tactic) have wings at +/-0.8..0.9 and would otherwise hug the pitch edge.
// Display only: the real formation is not modified.
const float pitchCardMaxSide = 0.75f;

Vector3 CardAnchorPosition(const Vector3 &formationPos, e_PlayerRole role) {
  Vector3 pos = formationPos;
  if (role != e_PlayerRole_GK) {
    pos.coords[0] = pos.coords[0] * 0.8f + 0.1f;
    pos.coords[1] = Clampf(pos.coords[1], -pitchCardMaxSide, pitchCardMaxSide);
  }
  return pos;
}

// Card content height for a given photo width (photo + role + name + bar).
float PitchCardHeight(float cardW) {
  return cardW * photoAspect + cardGap * 3.0f + badgeH + cardNameH + fatigueBarH;
}

// Frame height: content plus the gap on both sides.
float PitchCardFrameH(float cardW) {
  return PitchCardHeight(cardW) + framePad * 2.0f;
}

std::string ShortName(const std::string &name) {
  if (name.length() <= 10) return name;
  return name.substr(0, 10);
}

// Player portrait keyed by TM id; falls back to the generic placeholder when the
// player has no TM id or the file is not shipped.
std::string PlayerFacePath(PlayerData *player) {
  static const std::string placeholder = "media/menu/player_placeholder.png";
  if (!player) return placeholder;
  int tmId = player->GetRaw().tmId;
  if (tmId <= 0) return placeholder;
  std::string path = "databases/default/faces/" + int_to_str(tmId) + ".png";
  std::ifstream file(path.c_str());
  if (!file.good()) return placeholder;
  return path;
}

int PlayerRating(PlayerData *player) {
  float baseStat = player->GetRaw().baseStat;
  if (baseStat <= 0.0f) return -1;
  return int(baseStat * 100.0f + 0.5f);
}

std::string RatingText(PlayerData *player) {
  int rating = PlayerRating(player);
  return rating >= 0 ? int_to_str(rating) : "";
}

std::string RoleRatingText(e_PlayerRole role, PlayerData *player) {
  std::string text = GetRoleName(role);
  std::string rating = RatingText(player);
  if (!rating.empty()) text += " " + rating;
  return text;
}

e_PlayerRole NaturalRole(PlayerData *player) {
  const std::vector<e_PlayerRole> &roles = player->GetRoles();
  return roles.empty() ? e_PlayerRole_CM : roles.front();
}

int FatiguePercent(float fatigueFactorInv) {
  return int(Clampf(fatigueFactorInv, 0.0f, 1.0f) * 100.0f + 0.5f);
}

// Condition scale: blue (fresh) -> green -> yellow -> orange -> red (spent).
Vector3 FatigueColor(int percent) {
  if (percent >= 90) return Vector3(90, 200, 255);
  if (percent >= 70) return Vector3(80, 220, 90);
  if (percent >= 50) return Vector3(235, 220, 60);
  if (percent >= 30) return Vector3(240, 150, 45);
  return Vector3(225, 60, 55);
}

std::string FatigueText(int percent) {
  return int_to_str(percent) + "%";
}
}

GamePlanPage::GamePlanPage(Gui2WindowManager *windowManager, const Gui2PageData &pageData) : Gui2Page(windowManager, pageData) {

  int requestedTeamID = pageData.properties->GetInt("teamID", 0);

  moveCooldown_ms = 180;
  rebuildPending = false;
  rebuildFocusTeam = requestedTeamID;
  rebuildFocusSlot = -1;
  dualPanel = false;
  panelDevice[0] = panelDevice[1] = -1;
  opponentReady = 0;

  networkPrematch = !InMatch() && (GetMenuTask()->GetNetServer() != 0 || GetMenuTask()->GetNetClient() != 0);
  seenPlanRevision = GetMenuTask()->GetPlanRevision();

  SetupPanels();

  // Host/local: restore already-queued (not yet applied) subs so reopening the
  // plan still marks them. A thin client cannot see the host queue.
  if (InMatch() && !GetMenuTask()->GetNetClient()) {
    Match *match = GetGameTask()->GetMatch();
    if (match) {
      const std::vector<Substitution> &pending = match->GetPendingSubstitutions();
      for (unsigned int i = 0; i < pending.size(); i++) {
        for (unsigned int p = 0; p < panels.size(); p++) {
          if (pending.at(i).teamID == panels.at(p).teamID)
            panels.at(p).pendingSubs.push_back(std::make_pair(pending.at(i).outSlot, pending.at(i).inSlot));
        }
      }
      for (unsigned int p = 0; p < panels.size(); p++) NormalizePending(panels.at(p));
    }
  }

  Gui2Image *bg = new Gui2Image(windowManager, "gameplan_bg", 0, 0, 100, 100);
  bg->LoadImage("media/menu/backgrounds/black_strong.png");
  this->AddView(bg);
  bg->Show();

  // Header doubles as the leave-vote banner (the old "Game plan" title moved to
  // the page name only); it stays empty until a side is ready to leave.
  header = new Gui2Caption(windowManager, "gameplan_header", 0, 5, 0, 4, "");
  this->AddView(header);
  header->Show();

  Gui2Caption *sectionHint = new Gui2Caption(windowManager, "gameplan_section_hint", 0, 5, 0, 3, "");
  this->AddView(sectionHint);
  sectionHint->Show();
  CenterCaption(sectionHint, 80.0f, 5.0f, 3.0f, "Back: sections, Enter: open");

  BuildPlan();

  // Bottom detail: same widget as the pitch cards (photo, position+rating, name).
  float infoBadgeY = infoPhotoY + infoPhotoH + 0.2f;
  float infoNameY = infoBadgeY + infoBadgeH + 0.2f;
  infoPhotoA = new Gui2Image(windowManager, "gameplan_info_photo_a", 4, infoPhotoY, infoPhotoW, infoPhotoH);
  infoPhotoA->SetOverlay(true); // photos stay above the players'/text layer
  infoPhotoA->LoadImage("media/menu/player_placeholder.png");
  this->AddView(infoPhotoA);
  infoPhotoA->Show();
  infoBadgeA = new Gui2Caption(windowManager, "gameplan_info_badge_a", 2, infoBadgeY, 11.5f, infoBadgeH, "");
  this->AddView(infoBadgeA);
  infoBadgeA->Show();
  infoNameA = new Gui2Caption(windowManager, "gameplan_info_name_a", 2, infoNameY, 11.5f, infoNameH, "");
  this->AddView(infoNameA);
  infoNameA->Show();

  infoPhotoB = new Gui2Image(windowManager, "gameplan_info_photo_b", 49.5f, infoPhotoY, infoPhotoW, infoPhotoH);
  infoPhotoB->SetOverlay(true); // photos stay above the players'/text layer
  infoPhotoB->LoadImage("media/menu/player_placeholder.png");
  this->AddView(infoPhotoB);
  infoPhotoB->Show();
  infoBadgeB = new Gui2Caption(windowManager, "gameplan_info_badge_b", 46, infoBadgeY, 11.5f, infoBadgeH, "");
  this->AddView(infoBadgeB);
  infoBadgeB->Show();
  infoNameB = new Gui2Caption(windowManager, "gameplan_info_name_b", 46, infoNameY, 11.5f, infoNameH, "");
  this->AddView(infoNameB);
  infoNameB->Show();

  // Single layout: the local team can sit on the right (network away game), so
  // keep the focused-player info under the side the local player controls.
  // (UpdateInfoDetail recenters the badge/name on the photo each refresh.)
  if (!dualPanel && !panels.empty() && panels.at(0).px >= 50.0f) {
    infoPhotoA->SetPosition(54.0f, infoPhotoY);
    infoPhotoB->SetPosition(67.0f, infoPhotoY);
  }

  for (unsigned int p = 0; p < panels.size(); p++) {
    PlanPanel &panel = panels.at(p);
    if (!panel.entryIndices.empty()) SelectCursor(panel, panel.entryIndices.front());
  }
  Refresh();

  // Neutral entry: each side starts on its own section bar with no player focus.
  // In dual mode no button holds the single GUI focus; keep it on the page so
  // the per-device keyboard/joystick handlers receive events.
  for (unsigned int p = 0; p < panels.size(); p++) FocusSectionBar(panels.at(p));
  if (dualPanel) this->SetFocus();

  this->Show();
}

GamePlanPage::~GamePlanPage() {
}

bool GamePlanPage::InMatch() {
  return GetGameTask()->GetMatch() != 0;
}

TeamData *GamePlanPage::GetTeamDataFor(int teamID) {
  if (GetGameTask()->GetMatch()) return GetGameTask()->GetMatch()->GetTeam(teamID)->GetTeamData();
  MatchData *matchData = GetMenuTask()->GetMatchData();
  return matchData ? matchData->GetTeamData(teamID) : 0;
}

Team *GamePlanPage::GetTeamFor(int teamID) {
  return GetGameTask()->GetMatch() ? GetGameTask()->GetMatch()->GetTeam(teamID) : 0;
}

void GamePlanPage::SetupPanels() {

  int requestedTeamID = 0;
  // single mode team id: the team that opened the screen
  {
    // reuse rebuildFocusTeam, set from pageData in the constructor
    requestedTeamID = rebuildFocusTeam;
  }

  std::vector<int> teams;
  std::vector<int> devices;

  // Both in-match and pre-match: if two local humans control the two sides, show
  // and edit both teams. (Pre-match edits TeamData in MatchData, in-match the
  // live Team; either way each panel is driven by its own device.)
  const bool networkMatch = GetMenuTask()->GetNetServer() != 0 || GetMenuTask()->GetNetClient() != 0;
  if (networkMatch) {
    // Network sides live in the lobby, not in the local controller setup (the
    // mirrored side screen never fills ControllerSetup). Edit only the team this
    // peer owns; the opponent stays read-only.
    int localTeam = GetMenuTask()->GetLocalNetworkTeamID();
    if (localTeam >= 0) { teams.push_back(localTeam); devices.push_back(-1); }
  } else {
    // Order the panels left-to-right by the chosen side, not by controller id:
    // the keyboard is not necessarily controller 0, so iteration order could put
    // the right-side team on the left panel (local 2P got the sides swapped).
    std::vector<SideSelection> sides = GetMenuTask()->GetControllerSetup();
    std::sort(sides.begin(), sides.end(),
              [](const SideSelection &a, const SideSelection &b) { return a.side < b.side; });
    for (unsigned int s = 0; s < sides.size(); s++) {
      if (sides.at(s).side == 0) continue; // spectator/centre
      int tid = int(round(sides.at(s).side * 0.5 + 0.5));
      bool dup = false;
      for (unsigned int k = 0; k < teams.size(); k++) if (teams.at(k) == tid) dup = true;
      if (!dup) { teams.push_back(tid); devices.push_back(sides.at(s).controllerID); }
    }
  }

  if (teams.size() > 2) { teams.resize(2); devices.resize(2); }
  if (teams.size() >= 2) {
    dualPanel = true;
  } else if (teams.empty()) {
    teams.push_back(requestedTeamID);
    devices.push_back(-1);
  }

  panels.clear();
  for (unsigned int i = 0; i < teams.size(); i++) {
    PlanPanel panel;
    panel.teamID = teams.at(i);
    panel.teamData = GetTeamDataFor(panel.teamID);
    panel.team = GetTeamFor(panel.teamID);
    panel.editable = true;

    if (dualPanel && i == 0) {
      panel.px = d0_px; panel.pw = d0_pw; panel.bx = d0_bx; panel.bw = d0_bw;
    } else if (dualPanel && i == 1) {
      panel.px = d1_px; panel.pw = d1_pw; panel.bx = d1_bx; panel.bw = d1_bw;
    } else if (networkMatch && panel.teamID == 1) {
      // Own team is the away side: keep the real orientation (away on the right),
      // with its bench to the right; the home opponent is drawn on the left.
      panel.px = d1_px; panel.pw = d1_pw; panel.bx = d1_bx; panel.bw = d1_bw;
    } else if (networkMatch) {
      panel.px = d0_px; panel.pw = d0_pw; panel.bx = d0_bx; panel.bw = d0_bw;
    } else {
      panel.px = s_px; panel.pw = s_pw; panel.bx = s_bx; panel.bw = s_bw;
    }
    panel.py = pitchY; panel.ph = pitchH; panel.by = pitchY;
    panel.lastMove_ms = 0;

    // Neutral entry: the side starts on its own section bar, nothing opened.
    panel.activeSection = -1;
    panel.barCursor = 0;
    panel.barFocused = true;
    float mid = (panel.px + panel.bx + panel.bw) * 0.5f;
    panel.barCenterX = (mid < 50.0f) ? sectionHalfCenterL : sectionHalfCenterR;

    panelDevice[i] = devices.at(i);
    panels.push_back(panel);
  }

  // Single-panel opponent placement: mirror it to the opposite side of the peer.
  oppX = s_oppx;
  oppW = s_oppw;
  if (networkMatch && !dualPanel && !panels.empty()) {
    if (panels.at(0).teamID == 1) { oppX = d0_px; oppW = d0_pw; }
    else { oppX = d1_px; oppW = d1_pw; }
  }
}

void GamePlanPage::BuildPlan() {

  // pitch + bench backgrounds (and read-only opponent in single mode)
  for (unsigned int p = 0; p < panels.size(); p++) {
    PlanPanel &panel = panels.at(p);
    std::string suffix = int_to_str(panel.teamID);

    Gui2Image *pitchBg = new Gui2Image(windowManager, "gameplan_pitch_bg_" + suffix, panel.px, panel.py, panel.pw, panel.ph);
    pitchBg->LoadImage("media/menu/planmap_vertical.png");
    this->AddView(pitchBg);
    pitchBg->Show();

    Gui2Image *benchPanel = new Gui2Image(windowManager, "gameplan_bench_bg_" + suffix, panel.bx - 1, panel.by, panel.bw + 2, panel.ph);
    benchPanel->LoadImage("media/menu/backgrounds/black.png");
    this->AddView(benchPanel);
    benchPanel->Show();

    panel.benchHeader = new Gui2Caption(windowManager, "gameplan_bench_header_" + suffix, panel.bx, panel.by - 3, panel.bw, 3, "Subs");
    this->AddView(panel.benchHeader);
    panel.benchHeader->Show();

    Gui2Caption *teamHeader = new Gui2Caption(windowManager, "gameplan_team_header_" + suffix, panel.px, panel.py - 3, panel.pw, 3,
                                               panel.teamData ? panel.teamData->GetName() : "");
    this->AddView(teamHeader);
    teamHeader->Show();

    BuildSectionBar(panel);
    BuildSchemeList(panel);
    BuildRoleList(panel);
    BuildRolePicker(panel);
  }

  if (!dualPanel) {
    int opponentID = abs(panels.at(0).teamID - 1);
    Gui2Image *oppPitch = new Gui2Image(windowManager, "gameplan_opp_bg", oppX, pitchY, oppW, pitchH);
    oppPitch->LoadImage("media/menu/planmap_vertical.png");
    this->AddView(oppPitch);
    oppPitch->Show();

    // The opponent is never locally editable (AI or a remote peer): show READY
    // instead of a section bar.
    float oppMid = oppX + oppW * 0.5f;
    BuildOpponentReady((oppMid < 50.0f) ? sectionHalfCenterL : sectionHalfCenterR);
  }

  BuildEntries();
}

void GamePlanPage::BuildSectionBar(PlanPanel &panel) {
  int count = e_GamePlanSection_Size;
  float totalW = count * sectionBtnW + (count - 1) * sectionBtnGap;
  float x = panel.barCenterX - totalW * 0.5f;
  int pi = PanelIndex(panel);
  std::string suffix = int_to_str(panel.teamID);
  for (int i = 0; i < count; i++) {
    Gui2Button *button = new Gui2Button(windowManager, "gameplan_section_" + suffix + "_" + int_to_str(i), x, sectionBarY, sectionBtnW, sectionBtnH, SectionName(i));
    button->SetToggleable(true);
    button->sig_OnClick.connect(boost::bind(&GamePlanPage::SectionClicked, this, pi, i));
    this->AddView(button);
    button->Show();
    panel.sectionButtons.push_back(button);
    x += sectionBtnW + sectionBtnGap;
  }

  panel.sectionCaption = new Gui2Caption(windowManager, "gameplan_section_caption_" + suffix, 0, sectionCaptionY, 0, sectionCaptionH, "");
  this->AddView(panel.sectionCaption);
  panel.sectionCaption->Show();
  RefreshSectionBar(panel);
}

void GamePlanPage::BuildOpponentReady(float centerX) {
  opponentReady = new Gui2Caption(windowManager, "gameplan_opponent_ready", 0, sectionBarY, 0, sectionBtnH, "");
  this->AddView(opponentReady);
  opponentReady->Show();
  CenterCaption(opponentReady, centerX, sectionBarY, sectionBtnH, "READY");
}

void GamePlanPage::BuildSchemeList(PlanPanel &panel) {
  const std::vector<TacticalScheme> &schemes = GetTacticalSchemes();
  int count = (int)schemes.size();
  if (count <= 0) return;

  float step = panel.ph / count;
  float rowH = std::min(schemeRowMaxH, step - 1.0f);
  if (rowH < 1.5f) rowH = 1.5f;

  int pi = PanelIndex(panel);
  std::string suffix = int_to_str(panel.teamID);
  panel.schemeButtons.clear();
  for (int i = 0; i < count; i++) {
    float y = panel.by + i * step + (step - rowH) * 0.5f;
    Gui2Button *button = new Gui2Button(windowManager, "gameplan_scheme_" + suffix + "_" + int_to_str(i), panel.bx + 0.5f, y, panel.bw - 1.0f, rowH, schemes.at(i).name);
    button->SetToggleable(true);
    button->sig_OnClick.connect(boost::bind(&GamePlanPage::SchemeClicked, this, pi, i));
    this->AddView(button);
    button->Hide();
    panel.schemeButtons.push_back(button);
  }
}

void GamePlanPage::LayoutSchemes(PlanPanel &panel) {
  bool tactics = TacticsActive(panel);
  if (panel.benchHeader) panel.benchHeader->SetCaption(tactics ? "Scheme" : "Subs");

  // The scheme list takes over the bench column: hide the substitute rows.
  if (tactics) {
    for (unsigned int k = 0; k < panel.entryIndices.size(); k++) {
      PlanEntry &entry = entries.at(panel.entryIndices.at(k));
      if (entry.onPitch) continue;
      if (entry.button) entry.button->Hide();
      if (entry.roleCaption) entry.roleCaption->Hide();
      if (entry.ratingCaption) entry.ratingCaption->Hide();
      if (entry.fatigueCaption) entry.fatigueCaption->Hide();
    }
  }

  for (unsigned int i = 0; i < panel.schemeButtons.size(); i++) {
    if (tactics) panel.schemeButtons.at(i)->Show();
    else panel.schemeButtons.at(i)->Hide();
  }
}

void GamePlanPage::RefreshSchemes(PlanPanel &panel) {
  bool tactics = TacticsActive(panel);
  for (unsigned int i = 0; i < panel.schemeButtons.size(); i++) {
    bool committed = ((int)i == panel.committedScheme);
    bool cursor = tactics && !panel.barFocused && ((int)i == panel.schemeCursor);
    panel.schemeButtons.at(i)->SetToggled(committed);
    panel.schemeButtons.at(i)->SetHighlighted(committed || cursor);
    if (cursor) panel.schemeButtons.at(i)->SetColor(windowManager->GetStyle()->GetColor(e_DecorationType_Bright2));
    else panel.schemeButtons.at(i)->SetColor(windowManager->GetStyle()->GetColor(e_DecorationType_Bright1));
  }
}

void GamePlanPage::MoveSchemeCursor(PlanPanel &panel, int delta) {
  const std::vector<TacticalScheme> &schemes = GetTacticalSchemes();
  if (schemes.empty()) return;
  int count = (int)schemes.size();
  int next = panel.schemeCursor + delta;
  if (next < 0) next = 0;
  if (next > count - 1) next = count - 1;
  if (next == panel.schemeCursor) return;
  panel.schemeCursor = next;
  if (!dualPanel && !panel.schemeButtons.empty()) panel.schemeButtons.at(panel.schemeCursor)->SetFocus();
  // Move the existing cards in place: a full rebuild here made both teams'
  // squads flicker (widgets deleted/recreated while the render thread samples).
  PreviewSchemes(panel);
  RefreshPanel(panel);
}

void GamePlanPage::CommitScheme(PlanPanel &panel) {
  const std::vector<TacticalScheme> &schemes = GetTacticalSchemes();
  if (panel.schemeCursor < 0 || panel.schemeCursor >= (int)schemes.size()) return;

  if (GetMenuTask()->GetNetClient() || GetMenuTask()->GetNetServer()) {
    // Host-authoritative: send the intent; the applied scheme comes back via the
    // plan revision and the rebuild below.
    SendPlanScheme(panel.teamID, panel.schemeCursor);
  } else if (InMatch()) {
    ApplySchemeToTeam(panel.team, panel.schemeCursor);
  } else {
    ApplySchemeToTeamData(panel.teamData, panel.schemeCursor);
  }

  panel.committedScheme = panel.schemeCursor;
  rebuildFocusTeam = panel.teamID;
  rebuildFocusSlot = -1;
  rebuildPending = true;
}

void GamePlanPage::SchemeClicked(int panelIndex, int scheme) {
  if (panelIndex < 0 || panelIndex >= (int)panels.size()) return;
  PlanPanel &panel = panels.at(panelIndex);
  const std::vector<TacticalScheme> &schemes = GetTacticalSchemes();
  if (scheme < 0 || scheme >= (int)schemes.size()) return;
  if (panel.schemeCursor != scheme) panel.schemeCursor = scheme;
  CommitScheme(panel);
}

void GamePlanPage::BuildRoleList(PlanPanel &panel) {
  float top = panel.py + panel.ph + roleBandGap;
  float bottom = sectionBarY - roleBandGap;
  float step = (bottom - top) / e_TeamRole_SIZE;
  float rowH = std::min(roleRowMaxH, step - 0.25f);
  if (rowH < 1.4f) rowH = 1.4f;

  int pi = PanelIndex(panel);
  std::string suffix = int_to_str(panel.teamID);
  panel.roleButtons.clear();
  panel.rolePlayerCaptions.clear();
  for (int i = 0; i < e_TeamRole_SIZE; i++) {
    float y = top + i * step + (step - rowH) * 0.5f;
    Gui2Button *button = new Gui2Button(windowManager, "gameplan_rolerow_" + suffix + "_" + int_to_str(i),
                                        panel.px, y, panel.pw * 0.60f, rowH, GetTeamRoleName(planRoleOrder[i]));
    button->sig_OnClick.connect(boost::bind(&GamePlanPage::RoleClicked, this, pi, i));
    this->AddView(button);
    button->Hide();
    panel.roleButtons.push_back(button);

    Gui2Caption *playerCaption = new Gui2Caption(windowManager, "gameplan_roleplayer_" + suffix + "_" + int_to_str(i),
                                                 panel.px + panel.pw * 0.61f, y, panel.pw * 0.39f, rowH, "");
    this->AddView(playerCaption);
    playerCaption->Hide();
    panel.rolePlayerCaptions.push_back(playerCaption);
  }
}

void GamePlanPage::BuildRolePicker(PlanPanel &panel) {
  float centerX = panel.px + panel.pw * 0.5f;
  float hintY = panel.py + panel.ph + roleBandGap;
  float photoY = hintY + 3.0f;

  panel.rolePickerHint = new Gui2Caption(windowManager, "gameplan_rolepick_hint_" + int_to_str(panel.teamID), 0, hintY, 0, 2.6f, "");
  this->AddView(panel.rolePickerHint);
  panel.rolePickerHint->Hide();

  panel.rolePickerPhoto = new Gui2Image(windowManager, "gameplan_rolepick_photo_" + int_to_str(panel.teamID),
                                        centerX - rolePickerPhotoW * 0.5f, photoY, rolePickerPhotoW, rolePickerPhotoH);
  panel.rolePickerPhoto->SetOverlay(true); // photos stay above the players'/text layer
  panel.rolePickerPhoto->LoadImage("media/menu/player_placeholder.png");
  this->AddView(panel.rolePickerPhoto);
  panel.rolePickerPhoto->Hide();

  panel.rolePickerBadge = new Gui2Caption(windowManager, "gameplan_rolepick_badge_" + int_to_str(panel.teamID), 0, photoY + rolePickerPhotoH + 0.2f, 0, 2.2f, "");
  this->AddView(panel.rolePickerBadge);
  panel.rolePickerBadge->Hide();

  panel.rolePickerName = new Gui2Caption(windowManager, "gameplan_rolepick_name_" + int_to_str(panel.teamID), 0, photoY + rolePickerPhotoH + 2.6f, 0, 2.4f, "");
  this->AddView(panel.rolePickerName);
  panel.rolePickerName->Hide();
}

void GamePlanPage::LayoutRoles(PlanPanel &panel) {
  bool roles = RolesActive(panel);
  bool picking = roles && panel.rolePicking;

  for (unsigned int i = 0; i < panel.roleButtons.size(); i++) {
    if (roles && !picking) panel.roleButtons.at(i)->Show();
    else panel.roleButtons.at(i)->Hide();
    if (roles && !picking) panel.rolePlayerCaptions.at(i)->Show();
    else panel.rolePlayerCaptions.at(i)->Hide();
  }

  bool showPicker = picking && panel.cursorIndex >= 0 && panel.cursorIndex < (int)entries.size();
  if (panel.rolePickerHint) { if (showPicker) panel.rolePickerHint->Show(); else panel.rolePickerHint->Hide(); }
  if (panel.rolePickerPhoto) { if (showPicker) panel.rolePickerPhoto->Show(); else panel.rolePickerPhoto->Hide(); }
  if (panel.rolePickerBadge) { if (showPicker) panel.rolePickerBadge->Show(); else panel.rolePickerBadge->Hide(); }
  if (panel.rolePickerName) { if (showPicker) panel.rolePickerName->Show(); else panel.rolePickerName->Hide(); }
}

void GamePlanPage::RefreshRoles(PlanPanel &panel) {
  if (!RolesActive(panel)) return;
  const std::vector<PlayerData*> &playerData = panel.teamData->GetPlayerData();
  for (int i = 0; i < e_TeamRole_SIZE && i < (int)panel.roleButtons.size(); i++) {
    e_TeamRole role = planRoleOrder[i];
    int slot = RolePlayerSlot(panel, role);
    std::string playerName = (slot >= 0 && slot < (int)playerData.size()) ? ShortName(playerData.at(slot)->GetLastName()) : "-";
    panel.rolePlayerCaptions.at(i)->SetCaption(playerName);

    bool cursor = !panel.barFocused && ((int)i == panel.roleCursor);
    panel.roleButtons.at(i)->SetHighlighted(cursor);
    panel.roleButtons.at(i)->SetColor(cursor ? windowManager->GetStyle()->GetColor(e_DecorationType_Bright2)
                                             : windowManager->GetStyle()->GetColor(e_DecorationType_Bright1));
  }

  if (panel.rolePicking && panel.cursorIndex >= 0 && panel.cursorIndex < (int)entries.size()) {
    const PlanEntry &entry = entries.at(panel.cursorIndex);
    if (entry.index >= 0 && entry.index < (int)playerData.size()) {
      PlayerData *player = playerData.at(entry.index);
      e_TeamRole role = planRoleOrder[panel.rolePickingIndex];
      float centerX = panel.px + panel.pw * 0.5f;
      float hintY = panel.py + panel.ph + roleBandGap;
      float photoY = hintY + 3.0f;
      panel.rolePickerPhoto->LoadImage(PlayerFacePath(player));
      CenterCaption(panel.rolePickerHint, centerX, hintY, 2.6f, "Pick for " + GetTeamRoleName(role));
      CenterCaption(panel.rolePickerBadge, centerX, photoY + rolePickerPhotoH + 0.2f, 2.2f, RoleRatingText(entry.role, player));
      CenterCaption(panel.rolePickerName, centerX, photoY + rolePickerPhotoH + 2.6f, 2.4f, ShortName(player->GetLastName()));
    }
  }

  // Single layout: while browsing the role list, this section owns the GUI focus,
  // so no pitch card may keep it (a focused card would render its bright frame
  // under the list). The list itself is navigated through the focused role row.
  if (!dualPanel && !panel.rolePicking && !panel.roleButtons.empty())
    panel.roleButtons.at(panel.roleCursor)->SetFocus();
}

void GamePlanPage::MoveRoleCursor(PlanPanel &panel, int delta) {
  int next = panel.roleCursor + delta;
  if (next < 0) next = 0;
  if (next > e_TeamRole_SIZE - 1) next = e_TeamRole_SIZE - 1;
  if (next == panel.roleCursor) return;
  panel.roleCursor = next;
  if (!dualPanel && !panel.roleButtons.empty()) panel.roleButtons.at(panel.roleCursor)->SetFocus();
  RefreshPanel(panel);
}

void GamePlanPage::RoleClicked(int panelIndex, int role) {
  if (panelIndex < 0 || panelIndex >= (int)panels.size()) return;
  PlanPanel &panel = panels.at(panelIndex);
  if (role < 0 || role >= e_TeamRole_SIZE) return;
  panel.roleCursor = role;
  StartRolePicking(panel);
}

void GamePlanPage::StartRolePicking(PlanPanel &panel) {
  if (!RolesActive(panel)) return;
  panel.rolePicking = true;
  panel.rolePickingIndex = panel.roleCursor;

  // Start on the currently resolved taker when they are on the pitch.
  int slot = RolePlayerSlot(panel, planRoleOrder[panel.rolePickingIndex]);
  int focus = -1;
  for (unsigned int k = 0; k < panel.entryIndices.size(); k++) {
    int i = panel.entryIndices.at(k);
    if (!entries.at(i).onPitch) continue;
    if (focus < 0) focus = i;
    if (entries.at(i).index == slot) { focus = i; break; }
  }
  panel.cursorIndex = focus;
  if (!dualPanel) this->SetFocus();
  RefreshPanel(panel);
  RefreshSectionBar(panel);
  UpdateInfo();
}

void GamePlanPage::StopRolePicking(PlanPanel &panel) {
  if (!panel.rolePicking) return;
  panel.rolePicking = false;
  if (!dualPanel && !panel.roleButtons.empty()) panel.roleButtons.at(panel.roleCursor)->SetFocus();
  RefreshPanel(panel);
  RefreshSectionBar(panel);
  UpdateInfo();
}

void GamePlanPage::SelectPickCursor(PlanPanel &panel, int entryPosition) {
  if (entryPosition < 0 || entryPosition >= (int)entries.size()) return;
  panel.cursorIndex = entryPosition;
  if (!dualPanel) this->SetFocus();
  RefreshPanel(panel);
  UpdateInfo();
}

void GamePlanPage::ConfirmRolePick(PlanPanel &panel) {
  if (!panel.rolePicking) return;
  int entryPos = panel.cursorIndex;
  if (entryPos < 0 || entryPos >= (int)entries.size()) return;
  const PlanEntry &entry = entries.at(entryPos);
  if (!entry.onPitch) return;
  SetRole(panel, planRoleOrder[panel.rolePickingIndex], entry.index);
  panel.rolePicking = false;
  if (!dualPanel && !panel.roleButtons.empty()) panel.roleButtons.at(panel.roleCursor)->SetFocus();
  Refresh();
}

int GamePlanPage::RolePlayerSlot(PlanPanel &panel, e_TeamRole role) {
  if (panel.team) return panel.team->GetRoleSlot(role);
  if (!panel.teamData) return -1;
  MatchData *matchData = GetMenuTask()->GetMatchData();
  int stored = matchData ? matchData->GetRolePlayer(panel.teamID, role) : -1;
  if (stored >= 0 && stored < (int)panel.teamData->GetPlayerData().size()) return stored;
  return panel.teamData->SuggestRoleSlot(role);
}

void GamePlanPage::SetRole(PlanPanel &panel, e_TeamRole role, int slot) {
  if (GetMenuTask()->GetNetServer() || GetMenuTask()->GetNetClient()) {
    // Host-authoritative: send the intent; the applied role comes back through
    // the plan revision and the rebuild.
    SendPlanRole(panel.teamID, (int)role, slot);
  } else if (panel.team) {
    panel.team->SetRolePlayer(role, slot);
  } else {
    MatchData *matchData = GetMenuTask()->GetMatchData();
    if (matchData) matchData->SetRolePlayer(panel.teamID, role, slot);
  }
}

void GamePlanPage::SendPlanRole(int side, int role, int slot) {
  NetLobbyAction action;
  action.type = e_NetLobbyAction_PlanRole;
  action.side = side;
  action.value = role;
  action.value2 = slot;
  boost::shared_ptr<NetServer> server = GetMenuTask()->GetNetServer();
  if (server) { action.playerId = 0; server->ApplyLobbyAction(action); return; }
  boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
  if (client) client->SendLobbyAction(action);
}

Vector3 GamePlanPage::PitchAnchor(PlanPanel &panel, const FormationEntry &entry) {
  return PitchToScreen(CardAnchorPosition(entry.databasePosition, entry.role), panel.px, panel.py, panel.pw, panel.ph);
}

void GamePlanPage::PositionPitchCard(PlanEntry &entry, PlanPanel &panel, float anchorX, float anchorY, float maxBottom) {
  if (entry.cardPhotoH <= 0.0f) return;
  float cardH = PitchCardHeight(entry.cardW);
  entry.cardX = Clampf(anchorX - entry.cardW * 0.5f, panel.px, panel.px + panel.pw - entry.cardW);
  entry.cardY = anchorY - entry.cardPhotoH * 0.5f; // photo center sits on the anchor
  if (entry.cardY + cardH > maxBottom) entry.cardY = maxBottom - cardH;
  entry.cardY = Clampf(entry.cardY, panel.py, panel.py + panel.ph - cardH);
  entry.pos = Vector3(anchorX, anchorY, 0);
  ApplyPitchCardGeometry(entry);
}

void GamePlanPage::ApplyPitchCardGeometry(PlanEntry &entry) {
  float cx = entry.cardX, cy = entry.cardY;
  entry.cardCenterX = cx + entry.cardW * 0.5f;
  if (entry.photo) entry.photo->SetPosition(cx, cy);
  if (entry.photoOutline) entry.photoOutline->SetPosition(cx, cy);
  entry.roleY = cy + entry.cardW * photoAspect + cardGap;
  if (entry.roleCaption) entry.roleCaption->SetPosition(cx, entry.roleY);
  entry.nameY = entry.roleY + badgeH + cardGap;
  if (entry.button) entry.button->SetPosition(cx - framePad, cy - framePad); // frame is bigger than the content
  if (entry.nameCaption) entry.nameCaption->SetPosition(cx, entry.nameY);
  if (entry.fatigueBar) entry.fatigueBar->SetPosition(cx, entry.nameY + cardNameH + cardGap);
}

void GamePlanPage::PreviewSchemes(PlanPanel &panel) {
  const std::vector<TacticalScheme> &schemes = GetTacticalSchemes();
  bool preview = TacticsActive(panel) && !panel.barFocused && !schemes.empty();
  if (preview && (panel.schemeCursor < 0 || panel.schemeCursor >= (int)schemes.size())) panel.schemeCursor = 0;

  std::vector<int> pitchPositions;
  std::vector<SchemeCandidate> candidates;
  for (unsigned int k = 0; k < panel.entryIndices.size(); k++) {
    int entryPos = panel.entryIndices.at(k);
    PlanEntry &entry = entries.at(entryPos);
    if (!entry.onPitch) continue;
    std::map<int, FormationEntry>::iterator it = panel.pitchBase.find(entry.index);
    if (it == panel.pitchBase.end()) continue;
    pitchPositions.push_back(entryPos);
    SchemeCandidate candidate;
    candidate.slot = entry.index;
    candidate.role = it->second.role;
    candidate.position = it->second.databasePosition;
    candidates.push_back(candidate);
  }

  std::vector<FormationEntry> assignment;
  if (preview) assignment = AssignScheme(candidates, schemes.at(panel.schemeCursor));

  for (unsigned int i = 0; i < pitchPositions.size(); i++) {
    PlanEntry &entry = entries.at(pitchPositions.at(i));
    FormationEntry fe;
    if (preview && i < assignment.size()) fe = assignment.at(i);
    else fe = panel.pitchBase[entry.index];
    entry.role = fe.role;
    Vector3 s = PitchAnchor(panel, fe);
    float maxBottom = (fe.role == e_PlayerRole_GK) ? (panel.py + panel.ph) : panel.pitchBottomLimit;
    PositionPitchCard(entry, panel, s.coords[0], s.coords[1], maxBottom);
  }
}

void GamePlanPage::FocusSectionBar(PlanPanel &panel) {
  if (panel.sectionButtons.empty()) return;
  panel.barFocused = true;
  if (panel.activeSection >= 0) panel.barCursor = panel.activeSection;
  // Leaving the pitch clears the player focus, so returning to a section starts
  // from a clean slate instead of the last highlighted/held player.
  panel.heldIndex = -1;
  panel.cursorIndex = -1;
  panel.rolePicking = false;
  if (!dualPanel) panel.sectionButtons.at(panel.barCursor)->SetFocus();
  // Leaving the scheme list discards an uncommitted preview, in place.
  if (TacticsActive(panel)) PreviewSchemes(panel);
  RefreshPanel(panel);
  RefreshSectionBar(panel);
  UpdateInfo();
}

void GamePlanPage::FocusContent(PlanPanel &panel) {
  panel.barFocused = false;
  if (TacticsActive(panel)) {
    if (!dualPanel && !panel.schemeButtons.empty()) panel.schemeButtons.at(panel.schemeCursor)->SetFocus();
    PreviewSchemes(panel);
    RefreshPanel(panel);
    RefreshSectionBar(panel);
    UpdateInfo();
    return;
  }
  if (RolesActive(panel)) {
    if (!dualPanel && !panel.roleButtons.empty()) panel.roleButtons.at(panel.roleCursor)->SetFocus();
    RefreshPanel(panel);
    RefreshSectionBar(panel);
    UpdateInfo();
    return;
  }
  if (panel.cursorIndex < 0 && !panel.entryIndices.empty()) {
    SelectCursor(panel, panel.entryIndices.front()); // also refreshes + info
  } else {
    if (!dualPanel && panel.cursorIndex >= 0) entries.at(panel.cursorIndex).button->SetFocus();
    RefreshPanel(panel);
    UpdateInfo();
  }
  RefreshSectionBar(panel);
}

void GamePlanPage::MoveSectionFocus(PlanPanel &panel, int delta) {
  if (panel.sectionButtons.empty()) return;
  int count = (int)panel.sectionButtons.size();
  panel.barCursor = ((panel.barCursor + delta) % count + count) % count;
  if (!dualPanel) panel.sectionButtons.at(panel.barCursor)->SetFocus();
  RefreshSectionBar(panel);
}

void GamePlanPage::SectionClicked(int panelIndex, int section) {
  if (panelIndex < 0 || panelIndex >= (int)panels.size()) return;
  PlanPanel &panel = panels.at(panelIndex);
  SetSection(panel, section);
  // Sections with editable content take the focus off the bar. (Roles is always
  // editable now, so nothing is stubbed out here.)
  if (PositionsActive(panel) || TacticsActive(panel) || RolesActive(panel)) FocusContent(panel);
  else FocusSectionBar(panel);
}

void GamePlanPage::RefreshSectionBar(PlanPanel &panel) {
  for (unsigned int i = 0; i < panel.sectionButtons.size(); i++) {
    bool active = ((int)i == panel.activeSection);
    bool cursor = panel.barFocused && ((int)i == panel.barCursor);
    // The bar items are hidden while a section is open; they come back when the
    // player returns to the bar (Back).
    if (panel.barFocused) panel.sectionButtons.at(i)->Show();
    else panel.sectionButtons.at(i)->Hide();
    panel.sectionButtons.at(i)->SetToggled(active);
    panel.sectionButtons.at(i)->SetHighlighted(active || cursor);
  }
  if (panel.sectionCaption) {
    int shown = panel.barFocused ? panel.barCursor : panel.activeSection;
    if (shown < 0) shown = panel.barCursor;
    CenterCaption(panel.sectionCaption, panel.barCenterX, sectionCaptionY, sectionCaptionH, SectionDescription(shown));
  }
}

int GamePlanPage::PanelIndex(PlanPanel &panel) {
  for (unsigned int i = 0; i < panels.size(); i++) {
    if (&panels.at(i) == &panel) return (int)i;
  }
  return -1;
}

bool GamePlanPage::PositionsActive(const PlanPanel &panel) const {
  return panel.activeSection == e_GamePlanSection_Positions;
}

bool GamePlanPage::TacticsActive(const PlanPanel &panel) const {
  return panel.activeSection == e_GamePlanSection_Tactics;
}

bool GamePlanPage::RolesActive(const PlanPanel &panel) const {
  return panel.activeSection == e_GamePlanSection_Roles;
}

void GamePlanPage::SetSection(PlanPanel &panel, int section) {
  if (section < 0 || section >= e_GamePlanSection_Size) return;
  panel.activeSection = section;
  panel.barCursor = section;
  panel.heldIndex = -1;
  panel.rolePicking = false;
  // Restore/preview the pitch and re-lay the column in place (no widget churn),
  // so switching between the substitute list and the scheme list is not a frame
  // late and nothing flickers.
  PreviewSchemes(panel);
  LayoutBench(panel);
  RefreshSectionBar(panel);
  Refresh();
}

std::string GamePlanPage::SectionName(int section) {
  switch (section) {
    case e_GamePlanSection_Tactics: return "Tactics";
    case e_GamePlanSection_Positions: return "Positions";
    case e_GamePlanSection_Roles: return "Roles";
    default: return "";
  }
}

std::string GamePlanPage::SectionDescription(int section) {
  switch (section) {
    case e_GamePlanSection_Tactics: return "Choose a tactical scheme";
    case e_GamePlanSection_Positions: return "Change positions";
    case e_GamePlanSection_Roles: return "Captain and set-piece takers";
    default: return "";
  }
}

void GamePlanPage::BuildEntries() {
  ClearEntries();
  for (unsigned int p = 0; p < panels.size(); p++) BuildPanel(panels.at(p));
  if (!dualPanel) BuildOpponent(abs(panels.at(0).teamID - 1), oppX, pitchY, oppW, pitchH);
  for (unsigned int p = 0; p < panels.size(); p++) LayoutBench(panels.at(p));
}

void GamePlanPage::BuildPanel(PlanPanel &panel) {

  panel.entryIndices.clear();
  const std::vector<PlayerData*> &playerData = panel.teamData->GetPlayerData();
  int total = (int)playerData.size();

  auto placeCard = [&](int slot, int playerID, PlayerData *player, e_PlayerRole role, float anchorX, float anchorY,
                       bool onPitch, bool selectable, const std::string &roleText, float maxBottom) {
    float bx = panel.px, by = panel.py, bw = panel.pw, bh = panel.ph;
    if (!onPitch) { bx = panel.bx; by = panel.by; bw = panel.bw; bh = panel.ph; }

    // While the scheme list owns the bench column, bench rows are created hidden
    // so the render thread never samples a frame with both lists visible.
    bool visible = onPitch || !TacticsActive(panel);

    PlanEntry planEntry;
    planEntry.index = slot;
    planEntry.playerID = playerID;
    planEntry.teamID = panel.teamID;
    planEntry.role = role;
    planEntry.onPitch = onPitch;
    planEntry.selectable = selectable;
    planEntry.pos = Vector3(anchorX, anchorY, 0);
    planEntry.ratingCaption = 0;
    planEntry.fatigueCaption = 0;
    planEntry.nameCaption = 0;
    planEntry.photo = 0;
    planEntry.photoOutline = 0;
    planEntry.fatigueBar = 0;
    planEntry.cardX = 0.0f;
    planEntry.cardY = 0.0f;
    planEntry.cardW = 0.0f;
    planEntry.cardPhotoH = 0.0f;

    if (onPitch) {
      float cardW = photoSize;
      planEntry.cardW = cardW;
      planEntry.cardPhotoH = cardW * photoAspect;
      float frameW = cardW + framePad * 2.0f;
      float frameH = PitchCardFrameH(cardW);
      float cardH = PitchCardHeight(cardW);
      // Content sits on the formation anchor; the frame is drawn around it.
      float cx = Clampf(anchorX - cardW * 0.5f, bx, bx + bw - cardW);
      float cy = anchorY - planEntry.cardPhotoH * 0.5f; // photo center sits on the anchor
      if (cy + cardH > maxBottom) cy = maxBottom - cardH;
      cy = Clampf(cy, by, by + bh - cardH);
      float y = cy;
      float centerX = cx + cardW * 0.5f;

      Gui2Image *photo = new Gui2Image(windowManager, "gameplan_photo_" + int_to_str(panel.teamID) + "_" + int_to_str(slot), cx, y, cardW, planEntry.cardPhotoH);
      photo->SetOverlay(true); // cover neighbouring cards' name/role text on overlap
      photo->LoadImage(PlayerFacePath(player));
      this->AddView(photo);
      if (visible) photo->Show();
      planEntry.photo = photo;

      // White-outlined copy, shown when this card is the cursor/held player -
      // replaces the old coloured square frame. Precomputed, so selection
      // changes only swap visibility.
      Gui2Image *photoOutline = new Gui2Image(windowManager, "gameplan_photo_ol_" + int_to_str(panel.teamID) + "_" + int_to_str(slot), cx, y, cardW, planEntry.cardPhotoH);
      photoOutline->SetOverlay(true);
      photoOutline->SetDrawOutline(true, 4);
      photoOutline->LoadImage(PlayerFacePath(player));
      this->AddView(photoOutline);
      photoOutline->Hide();
      planEntry.photoOutline = photoOutline;

      y += planEntry.cardPhotoH + cardGap;

      planEntry.cardCenterX = centerX;
      planEntry.roleY = y;
      planEntry.roleCaption = new Gui2Caption(windowManager, "gameplan_role_" + int_to_str(panel.teamID) + "_" + int_to_str(slot), cx, y, cardW, badgeH, roleText);
      this->AddView(planEntry.roleCaption);
      if (visible) planEntry.roleCaption->Show();
      FitNameCaption(planEntry.roleCaption, centerX, y, roleText, "", cardW);

      y += badgeH + cardGap;

      planEntry.nameY = y;
      planEntry.nameCaption = new Gui2Caption(windowManager, "gameplan_name_" + int_to_str(panel.teamID) + "_" + int_to_str(slot), cx, y, cardW, cardNameH,
                                              player->GetLastName());
      y += cardNameH + cardGap;

      planEntry.fatigueBar = new Gui2Image(windowManager, "gameplan_fatigue_" + int_to_str(panel.teamID) + "_" + int_to_str(slot), cx, y, cardW, fatigueBarH);
      this->AddView(planEntry.fatigueBar);
      if (visible) planEntry.fatigueBar->Show();

      planEntry.button = new Gui2Button(windowManager, "gameplan_player_" + int_to_str(panel.teamID) + "_" + int_to_str(slot), cx - framePad, cy - framePad, frameW, frameH, "");
      planEntry.button->SetFrameOnly(true);
      planEntry.button->SetDrawFrame(false); // selection is the photo outline, not a square

    } else {
      float rowH = benchRowH;
      float posW = benchPosW, ratingW = benchRatingW, fatigueW = benchFatigueW;
      float nameW = bw - posW - ratingW - fatigueW - 0.6f;
      float ratingX = bx + posW + nameW + 0.4f;
      float cy = Clampf(anchorY - rowH * 0.5f, by, by + bh - rowH);

      planEntry.roleCaption = new Gui2Caption(windowManager, "gameplan_role_" + int_to_str(panel.teamID) + "_" + int_to_str(slot), bx, cy, posW, rowH, roleText);
      this->AddView(planEntry.roleCaption);
      if (visible) planEntry.roleCaption->Show();

      planEntry.button = new Gui2Button(windowManager, "gameplan_player_" + int_to_str(panel.teamID) + "_" + int_to_str(slot), bx + posW + 0.2f, cy, nameW, rowH,
                                        ShortName(player->GetLastName()));

      planEntry.ratingCaption = new Gui2Caption(windowManager, "gameplan_rating_" + int_to_str(panel.teamID) + "_" + int_to_str(slot), ratingX, cy, ratingW, rowH, RatingText(player));
      this->AddView(planEntry.ratingCaption);
      if (visible) planEntry.ratingCaption->Show();

      planEntry.fatigueCaption = new Gui2Caption(windowManager, "gameplan_fatigue_" + int_to_str(panel.teamID) + "_" + int_to_str(slot), ratingX + ratingW + 0.2f, cy, fatigueW, rowH, "");
      this->AddView(planEntry.fatigueCaption);
      if (visible) planEntry.fatigueCaption->Show();
    }

    int entryPos = (int)entries.size();
    planEntry.button->SetToggleable(true);
    planEntry.button->sig_OnClick.connect(boost::bind(&GamePlanPage::EntryClicked, this, entryPos));
    this->AddView(planEntry.button);
    if (visible) planEntry.button->Show();

    if (planEntry.nameCaption) {
      this->AddView(planEntry.nameCaption);
      if (visible) planEntry.nameCaption->Show();
      FitNameCaption(planEntry.nameCaption, planEntry.cardCenterX, planEntry.nameY,
                     player->GetLastName(), "", planEntry.cardW);
    }

    entries.push_back(planEntry);
    panel.entryIndices.push_back(entryPos);
  };

  // display state (in-match: active/inactive + runtime formation; pre-match: XI+bench)
  struct PlanPlayer { int slot; int playerID; e_PlayerRole role; Vector3 pos; bool onPitch; };
  std::vector<PlanPlayer> all;
  Team *team = panel.team;
  if (team) {
    const std::vector<Player*> &allPlayers = team->GetAllPlayers();
    for (int i = 0; i < (int)allPlayers.size(); i++) {
      PlanPlayer pp; pp.slot = i; pp.playerID = allPlayers.at(i)->GetID();
      if (allPlayers.at(i)->IsActive()) {
        FormationEntry fe = team->GetFormationEntry(pp.playerID);
        pp.role = fe.role; pp.pos = fe.databasePosition; pp.onPitch = true;
      } else {
        pp.role = NaturalRole(playerData.at(i)); pp.pos = Vector3(0); pp.onPitch = false;
      }
      all.push_back(pp);
    }
  } else {
    for (int i = 0; i < playerNum && i < total; i++) {
      FormationEntry fe = panel.teamData->GetFormationEntry(i);
      PlanPlayer pp; pp.slot = i; pp.playerID = -1; pp.role = fe.role; pp.pos = fe.databasePosition; pp.onPitch = true;
      all.push_back(pp);
    }
    for (int i = playerNum; i < total; i++) {
      PlanPlayer pp; pp.slot = i; pp.playerID = -1; pp.role = NaturalRole(playerData.at(i)); pp.pos = Vector3(0); pp.onPitch = false;
      all.push_back(pp);
    }
  }

  for (unsigned int s = 0; s < panel.pendingSubs.size(); s++) {
    int outSlot = panel.pendingSubs.at(s).first, inSlot = panel.pendingSubs.at(s).second;
    PlanPlayer *outP = 0, *inP = 0;
    for (unsigned int k = 0; k < all.size(); k++) {
      if (all.at(k).slot == outSlot) outP = &all.at(k);
      if (all.at(k).slot == inSlot) inP = &all.at(k);
    }
    if (!outP || !inP || !outP->onPitch) continue;
    inP->onPitch = true; inP->role = outP->role; inP->pos = outP->pos;
    outP->onPitch = false; outP->role = NaturalRole(playerData.at(outP->slot)); outP->pos = Vector3(0);
  }

  std::vector<PlanPlayer> pitchList, benchList;
  for (unsigned int k = 0; k < all.size(); k++) {
    if (all.at(k).onPitch) pitchList.push_back(all.at(k)); else benchList.push_back(all.at(k));
  }

  // Recognise the current formation and remember the real (non-preview)
  // positions, so the Tactics preview can move cards in place without a rebuild.
  panel.pitchBase.clear();
  {
    std::vector<FormationEntry> currentFormation;
    for (unsigned int i = 0; i < pitchList.size(); i++) {
      FormationEntry fe;
      fe.role = pitchList.at(i).role;
      fe.databasePosition = pitchList.at(i).pos;
      fe.position = pitchList.at(i).pos;
      currentFormation.push_back(fe);
      panel.pitchBase[pitchList.at(i).slot] = fe;
    }
    panel.committedScheme = MatchScheme(currentFormation);
  }

  float bottomLimit = panel.py + panel.ph - pitchCardH - 0.3f;
  for (unsigned int i = 0; i < pitchList.size(); i++) {
    if (pitchList.at(i).role != e_PlayerRole_GK) continue;
    Vector3 gs = PitchToScreen(pitchList.at(i).pos, panel.px, panel.py, panel.pw, panel.ph);
    bottomLimit = (std::min(gs.coords[1], panel.py + panel.ph) - pitchCardH) - 0.3f;
    break;
  }
  panel.pitchBottomLimit = bottomLimit;

  for (unsigned int i = 0; i < pitchList.size(); i++) {
    const PlanPlayer &pp = pitchList.at(i);
    Vector3 s = PitchToScreen(CardAnchorPosition(pp.pos, pp.role), panel.px, panel.py, panel.pw, panel.ph);
    float maxBottom = (pp.role == e_PlayerRole_GK) ? (panel.py + panel.ph) : bottomLimit;
    placeCard(pp.slot, pp.playerID, playerData.at(pp.slot), pp.role, s.coords[0], s.coords[1], true, true,
              RoleRatingText(pp.role, playerData.at(pp.slot)), maxBottom);
  }

  panel.benchCount = (int)benchList.size();
  panel.benchMaxVisible = std::max(1, (int)std::floor(panel.ph / 3.4f));
  if (panel.benchCount > panel.benchMaxVisible) panel.benchStep = 3.4f;
  else {
    panel.benchStep = panel.benchCount > 0 ? panel.ph / panel.benchCount : 9.0f;
    if (panel.benchStep > 9.0f) panel.benchStep = 9.0f;
  }
  panel.benchRowHeight = std::min(benchRowH, panel.benchStep - 0.4f);
  panel.benchScroll = 0;
  for (int r = 0; r < panel.benchCount; r++) {
    const PlanPlayer &pp = benchList.at(r);
    bool selectable = !team || !team->HasLeftPitch(pp.playerID);
    float rowY = panel.by + r * panel.benchStep;
    placeCard(pp.slot, pp.playerID, playerData.at(pp.slot), pp.role, panel.bx + panel.bw * 0.5f, rowY + panel.benchStep * 0.5f,
              false, selectable, GetRoleName(pp.role), rowY + panel.benchStep);
  }

  // Cards are created at the real formation; if the Tactics preview is open,
  // move them onto the highlighted scheme without another rebuild.
  PreviewSchemes(panel);

  panel.cursorIndex = panel.entryIndices.empty() ? -1 : panel.entryIndices.front();
  panel.heldIndex = -1;
}

void GamePlanPage::BuildOpponent(int teamID, float x, float y, float w, float h) {
  TeamData *opponent = GetTeamDataFor(teamID);
  if (!opponent) return;

  // In-match, show the opponent's actual lineup. Runtime substitutions live only
  // in the live Team/runtimeFormation, so reading the original TeamData XI left
  // the substituted-out player on the host's read-only opponent panel.
  struct OppPlayer { PlayerData *player; e_PlayerRole role; Vector3 pos; };
  std::vector<OppPlayer> list;
  Team *team = GetTeamFor(teamID);
  if (team) {
    const std::vector<Player*> &all = team->GetAllPlayers();
    for (int i = 0; i < (int)all.size(); i++) {
      if (!all.at(i)->IsActive()) continue;
      OppPlayer op;
      op.player = opponent->GetPlayerData(i);
      FormationEntry fe = team->GetFormationEntry(all.at(i)->GetID());
      op.role = fe.role;
      op.pos = fe.databasePosition;
      list.push_back(op);
    }
  } else {
    for (int i = 0; i < playerNum && i < opponent->GetPlayerNum(); i++) {
      OppPlayer op;
      op.player = opponent->GetPlayerData(i);
      op.role = opponent->GetFormationEntry(i).role;
      op.pos = opponent->GetFormationEntry(i).databasePosition;
      list.push_back(op);
    }
  }

  float oppCardW = photoSize;
  float oppPhotoH = oppCardW * photoAspect;
  float cardH = oppPhotoH + cardGap * 2.0f + badgeH + cardNameH; // no fatigue bar on the opponent

  float gkAnchorY = y + h;
  for (unsigned int i = 0; i < list.size(); i++) {
    if (list.at(i).role != e_PlayerRole_GK) continue;
    gkAnchorY = std::min(PitchToScreen(list.at(i).pos, x, y, w, h).coords[1], y + h);
    break;
  }
  float bottomLimit = (gkAnchorY - cardH) - 0.3f;

  for (unsigned int i = 0; i < list.size(); i++) {
    const OppPlayer &entry = list.at(i);
    Vector3 s = PitchToScreen(CardAnchorPosition(entry.pos, entry.role), x, y, w, h);
    float maxBottom = (entry.role == e_PlayerRole_GK) ? (y + h) : bottomLimit;
    float cx = Clampf(s.coords[0] - oppCardW * 0.5f, x, x + w - oppCardW);
    float cy = s.coords[1] - oppPhotoH * 0.5f;
    if (cy + cardH > maxBottom) cy = maxBottom - cardH;
    cy = Clampf(cy, y, y + h - cardH);
    float centerX = cx + oppCardW * 0.5f;

    Gui2Image *photo = new Gui2Image(windowManager, "gameplan_opp_photo_" + int_to_str((int)i), cx, cy, oppCardW, oppPhotoH);
    photo->SetOverlay(true); // cover neighbouring cards' name/role text on overlap
    photo->LoadImage(PlayerFacePath(entry.player));
    this->AddView(photo);
    photo->Show();
    opponentViews.push_back(photo);

    float roleY = cy + oppPhotoH + cardGap;
    Gui2Caption *role = new Gui2Caption(windowManager, "gameplan_opp_role_" + int_to_str((int)i), cx, roleY, oppCardW, badgeH,
                                        RoleRatingText(entry.role, entry.player));
    this->AddView(role);
    role->Show();
    FitNameCaption(role, centerX, roleY, RoleRatingText(entry.role, entry.player), "", oppCardW);
    opponentViews.push_back(role);

    float nameY = roleY + badgeH + cardGap;
    Gui2Caption *name = new Gui2Caption(windowManager, "gameplan_opp_name_" + int_to_str((int)i), cx, nameY, oppCardW, cardNameH,
                                        entry.player->GetLastName());
    this->AddView(name);
    name->Show();
    FitNameCaption(name, centerX, nameY, entry.player->GetLastName(), "", oppCardW);
    opponentViews.push_back(name);
  }
}

void GamePlanPage::ClearEntries() {
  for (unsigned int i = 0; i < entries.size(); i++) {
    PlanEntry &e = entries.at(i);
    if (e.photo) { e.photo->Exit(); delete e.photo; e.photo = 0; }
    if (e.photoOutline) { e.photoOutline->Exit(); delete e.photoOutline; e.photoOutline = 0; }
    if (e.fatigueBar) { e.fatigueBar->Exit(); delete e.fatigueBar; e.fatigueBar = 0; }
    if (e.nameCaption) { e.nameCaption->Exit(); delete e.nameCaption; e.nameCaption = 0; }
    if (e.ratingCaption) { e.ratingCaption->Exit(); delete e.ratingCaption; e.ratingCaption = 0; }
    if (e.fatigueCaption) { e.fatigueCaption->Exit(); delete e.fatigueCaption; e.fatigueCaption = 0; }
    if (e.roleCaption) { e.roleCaption->Exit(); delete e.roleCaption; e.roleCaption = 0; }
    if (e.button) { e.button->Exit(); delete e.button; e.button = 0; }
  }
  entries.clear();

  for (unsigned int i = 0; i < opponentViews.size(); i++) {
    opponentViews.at(i)->Exit();
    delete opponentViews.at(i);
  }
  opponentViews.clear();
}

void GamePlanPage::LayoutBench(PlanPanel &panel) {
  int maxScroll = std::max(0, panel.benchCount - panel.benchMaxVisible);
  panel.benchScroll = std::max(0, std::min(panel.benchScroll, maxScroll));

  float nameW = panel.bw - benchPosW - benchRatingW - benchFatigueW - 0.6f;
  float ratingX = panel.bx + benchPosW + nameW + 0.4f;
  int row = 0;
  for (unsigned int k = 0; k < panel.entryIndices.size(); k++) {
    PlanEntry &entry = entries.at(panel.entryIndices.at(k));
    if (entry.onPitch) continue;

    float cy = panel.by + (row - panel.benchScroll) * panel.benchStep;
    if (TacticsActive(panel) || row < panel.benchScroll || row >= panel.benchScroll + panel.benchMaxVisible) {
      entry.button->Hide();
      if (entry.roleCaption) entry.roleCaption->Hide();
      if (entry.ratingCaption) entry.ratingCaption->Hide();
      if (entry.fatigueCaption) entry.fatigueCaption->Hide();
    } else {
      entry.button->Show();
      entry.button->SetPosition(panel.bx + benchPosW + 0.2f, cy);
      if (entry.roleCaption) { entry.roleCaption->Show(); entry.roleCaption->SetPosition(panel.bx, cy); }
      if (entry.ratingCaption) { entry.ratingCaption->Show(); entry.ratingCaption->SetPosition(ratingX, cy); }
      if (entry.fatigueCaption) { entry.fatigueCaption->Show(); entry.fatigueCaption->SetPosition(ratingX + benchRatingW + 0.2f, cy); }
    }
    entry.pos = Vector3(panel.bx + panel.bw * 0.5f, cy + panel.benchRowHeight * 0.5f, 0);
    row++;
  }

  LayoutSchemes(panel);
  LayoutRoles(panel);
}

void GamePlanPage::Rebuild(int focusTeam, int focusSlot) {
  ClearEntries();
  for (unsigned int p = 0; p < panels.size(); p++) BuildPanel(panels.at(p));
  if (!dualPanel) BuildOpponent(abs(panels.at(0).teamID - 1), oppX, pitchY, oppW, pitchH);
  for (unsigned int p = 0; p < panels.size(); p++) LayoutBench(panels.at(p));

  for (unsigned int p = 0; p < panels.size(); p++) {
    PlanPanel &panel = panels.at(p);
    if (TacticsActive(panel)) {
      // The scheme list, not the pitch, owns the focus in this section.
      panel.cursorIndex = -1;
      panel.heldIndex = -1;
      if (!dualPanel && !panel.barFocused && !panel.schemeButtons.empty())
        panel.schemeButtons.at(panel.schemeCursor)->SetFocus();
      continue;
    }
    if (RolesActive(panel)) {
      if (panel.rolePicking) {
        // Keep hovering the pitch: re-seat the pick cursor (entry positions are
        // gone after the rebuild), preferring the previously hovered slot.
        int focus = -1;
        for (unsigned int k = 0; k < panel.entryIndices.size(); k++) {
          int i = panel.entryIndices.at(k);
          if (!entries.at(i).onPitch) continue;
          if (focus < 0) focus = i;
          if (panel.teamID == focusTeam && entries.at(i).index == focusSlot) { focus = i; break; }
        }
        panel.cursorIndex = focus;
        if (!dualPanel) this->SetFocus();
      } else {
        panel.cursorIndex = -1;
        panel.heldIndex = -1;
        if (!dualPanel && !panel.barFocused && !panel.roleButtons.empty())
          panel.roleButtons.at(panel.roleCursor)->SetFocus();
      }
      continue;
    }
    int focus = -1;
    if (panel.teamID == focusTeam) {
      for (unsigned int k = 0; k < panel.entryIndices.size(); k++) {
        if (entries.at(panel.entryIndices.at(k)).index == focusSlot) { focus = panel.entryIndices.at(k); break; }
      }
    }
    if (focus < 0 && !panel.entryIndices.empty()) focus = panel.entryIndices.front();
    SelectCursor(panel, focus);
  }
  Refresh();
}

int GamePlanPage::FindNextEntry(PlanPanel &panel, int from, const Vector3 &direction, bool onlyPitch) {
  if (from < 0 || from >= (int)entries.size()) return -1;
  Vector3 cur = entries.at(from).pos;
  bool fromPitch = entries.at(from).onPitch;
  Vector3 dir = direction.GetNormalized(0);

  // Pick the nearest entry *along* the pushed direction, with a cap on the
  // sideways offset. (A plain dot-product/distance mix makes far-but-aligned
  // players win over near ones on the vertically stretched pitch, which is why
  // navigation skipped centre-backs.)
  int same = -1; float sameScore = 1e9f;
  int any = -1; float anyScore = 1e9f;
  for (unsigned int k = 0; k < panel.entryIndices.size(); k++) {
    int i = panel.entryIndices.at(k);
    if (i == from) continue;
    if (!entries.at(i).selectable) continue;
    if (onlyPitch && !entries.at(i).onPitch) continue; // role picking is pitch-only
    Vector3 delta = entries.at(i).pos - cur;
    float along = delta.coords[0] * dir.coords[0] + delta.coords[1] * dir.coords[1];
    if (along <= 0.001f) continue;
    float perp = fabs(delta.coords[0] * (-dir.coords[1]) + delta.coords[1] * dir.coords[0]);
    if (perp > along * 0.9f + 4.0f) continue; // too far to the side
    float score = along + perp * 0.75f;
    if (score < anyScore) { anyScore = score; any = i; }
    if (entries.at(i).onPitch == fromPitch && score < sameScore) { sameScore = score; same = i; }
  }
  return same != -1 ? same : any;
}

void GamePlanPage::SelectCursor(PlanPanel &panel, int entryPosition) {
  if (entryPosition < 0 || entryPosition >= (int)entries.size()) return;
  panel.cursorIndex = entryPosition;
  if (!entries.at(entryPosition).onPitch) {
    int r = 0;
    for (unsigned int k = 0; k < panel.entryIndices.size(); k++) {
      int i = panel.entryIndices.at(k);
      if (i == entryPosition) break;
      if (!entries.at(i).onPitch) r++;
    }
    if (r < panel.benchScroll) panel.benchScroll = r;
    if (r >= panel.benchScroll + panel.benchMaxVisible) panel.benchScroll = r - panel.benchMaxVisible + 1;
    LayoutBench(panel);
  }
  if (!dualPanel && !panel.barFocused) entries.at(entryPosition).button->SetFocus();
  RefreshPanel(panel);
  UpdateInfo();
}

void GamePlanPage::NormalizePending(PlanPanel &panel) {
  std::vector<std::pair<int, int> > &pendingSubs = panel.pendingSubs;
  bool changed = true;
  while (changed) {
    changed = false;
    for (unsigned int i = 0; i < pendingSubs.size(); i++) {
      if (pendingSubs.at(i).first == pendingSubs.at(i).second) { pendingSubs.erase(pendingSubs.begin() + i); changed = true; break; }
    }
    if (changed) continue;
    for (unsigned int i = 0; i < pendingSubs.size() && !changed; i++) {
      for (unsigned int j = 0; j < pendingSubs.size(); j++) {
        if (i == j) continue;
        if (pendingSubs.at(i).second == pendingSubs.at(j).first) {
          pendingSubs.at(i).second = pendingSubs.at(j).second;
          pendingSubs.erase(pendingSubs.begin() + j);
          changed = true; break;
        }
      }
    }
    if (changed) continue;
    for (unsigned int i = 0; i < pendingSubs.size() && !changed; i++) {
      for (unsigned int j = i + 1; j < pendingSubs.size(); j++) {
        if (pendingSubs.at(i).first == pendingSubs.at(j).first || pendingSubs.at(i).second == pendingSubs.at(j).second) {
          pendingSubs.erase(pendingSubs.begin() + j);
          changed = true; break;
        }
      }
    }
  }
}

bool GamePlanPage::IsPending(PlanPanel &panel, int entryPosition, bool &isOut) {
  int slot = entries.at(entryPosition).index;
  for (unsigned int i = 0; i < panel.pendingSubs.size(); i++) {
    if (panel.pendingSubs.at(i).first == slot) { isOut = true; return true; }
    if (panel.pendingSubs.at(i).second == slot) { isOut = false; return true; }
  }
  return false;
}

void GamePlanPage::QueueSubstitution(PlanPanel &panel, int outIndex, int inIndex) {
  const PlanEntry &out = entries.at(outIndex);
  const PlanEntry &in = entries.at(inIndex);
  if (!out.onPitch || in.onPitch) return;

  boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
  if (client) {
    NetLobbyAction action;
    action.type = e_NetLobbyAction_RequestSubstitution;
    action.side = panel.teamID;
    action.value = out.index;
    action.value2 = in.index;
    client->SendLobbyAction(action);
  } else {
    Match *match = GetGameTask()->GetMatch();
    if (!match) return;
    match->QueueSubstitution(panel.teamID, out.playerID, in.playerID);
  }
  panel.pendingSubs.push_back(std::make_pair(out.index, in.index));
  NormalizePending(panel);
  rebuildFocusTeam = panel.teamID;
  rebuildFocusSlot = in.index;
  rebuildPending = true;
}

bool GamePlanPage::CancelQueued(PlanPanel &panel, int outIndex, int inIndex) {
  const PlanEntry &out = entries.at(outIndex);
  const PlanEntry &in = entries.at(inIndex);
  for (unsigned int i = 0; i < panel.pendingSubs.size(); i++) {
    if (panel.pendingSubs.at(i).first != out.index || panel.pendingSubs.at(i).second != in.index) continue;

    boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
    if (client) {
      NetLobbyAction action;
      action.type = e_NetLobbyAction_CancelSubstitution;
      action.side = panel.teamID;
      action.value = out.index;
      client->SendLobbyAction(action);
    } else if (GetGameTask()->GetMatch()) {
      GetGameTask()->GetMatch()->CancelSubstitution(panel.teamID, out.index);
    }
    panel.pendingSubs.erase(panel.pendingSubs.begin() + i);
    NormalizePending(panel);
    rebuildFocusTeam = panel.teamID;
    rebuildFocusSlot = out.index;
    rebuildPending = true;
    return true;
  }
  return false;
}

void GamePlanPage::PerformAction(PlanPanel &panel, int a, int b) {
  if (!PositionsActive(panel)) return;
  if (a < 0 || b < 0 || a == b) return;
  if (!entries.at(a).selectable || !entries.at(b).selectable) return;

  if (!InMatch()) {
    // pre-match: swap the two slots in TeamData
    int idA = panel.teamData->GetPlayerData(entries.at(a).index)->GetDatabaseID();
    int idB = panel.teamData->GetPlayerData(entries.at(b).index)->GetDatabaseID();
    if (networkPrematch) {
      // Host-authoritative: send the intent; the host applies it and relays the
      // authoritative swap back, which triggers the rebuild below.
      SendPlanSwap(panel.teamID, idA, idB);
      return;
    }
    panel.teamData->SwitchPlayers(idA, idB);
    Refresh();
    return;
  }

  bool aPitch = entries.at(a).onPitch;
  bool bPitch = entries.at(b).onPitch;
  if (aPitch == bPitch) return;
  int outIndex = aPitch ? a : b;
  int inIndex = aPitch ? b : a;

  if (!CancelQueued(panel, outIndex, inIndex)) QueueSubstitution(panel, outIndex, inIndex);
}

void GamePlanPage::EntryClicked(int entryPosition) {
  if (entryPosition < 0 || entryPosition >= (int)entries.size()) return;
  if (!entries.at(entryPosition).selectable) return;
  PlanPanel *panel = 0;
  for (unsigned int p = 0; p < panels.size(); p++) {
    for (unsigned int k = 0; k < panels.at(p).entryIndices.size(); k++) {
      if (panels.at(p).entryIndices.at(k) == entryPosition) { panel = &panels.at(p); break; }
    }
    if (panel) break;
  }
  if (!panel) return;
  if (!PositionsActive(*panel)) return;

  if (panel->heldIndex == -1) panel->heldIndex = entryPosition;
  else if (panel->heldIndex == entryPosition) panel->heldIndex = -1;
  else { PerformAction(*panel, panel->heldIndex, entryPosition); panel->heldIndex = -1; }
  Refresh();
}

void GamePlanPage::Refresh() {
  for (unsigned int p = 0; p < panels.size(); p++) RefreshPanel(panels.at(p));
  UpdateInfo();
}

void GamePlanPage::RefreshPanel(PlanPanel &panel) {
  for (unsigned int k = 0; k < panel.entryIndices.size(); k++) {
    int pos = panel.entryIndices.at(k);
    PlanEntry &entry = entries.at(pos);
    PlayerData *player = panel.teamData->GetPlayerData(entry.index);
    e_PlayerRole role = entry.role;

    bool pending = false, pendingOut = false;
    if (InMatch()) pending = IsPending(panel, pos, pendingOut);
    entry.button->SetToggled(pos == panel.heldIndex || pending);

    std::string suffix = pending ? (pendingOut ? " -" : " +") : "";
    if (!entry.selectable) suffix += " (out)";

    // The pitch is interactive in Positions and while picking a role; in the
    // Roles list (and other sections) the pitch is just a dim backdrop.
    bool pitchEditing = PositionsActive(panel) || (RolesActive(panel) && panel.rolePicking);

    if (!pitchEditing && !TacticsActive(panel)) entry.button->SetColor(windowManager->GetStyle()->GetColor(e_DecorationType_Dark2));
    else if (!entry.selectable) entry.button->SetColor(Vector3(110, 110, 110));
    else if (pos == panel.cursorIndex) entry.button->SetColor(windowManager->GetStyle()->GetColor(e_DecorationType_Bright2));
    else entry.button->SetColor(windowManager->GetStyle()->GetColor(e_DecorationType_Bright1));

    // Local two-player has no GUI focus on the pitch, so the cursor entry would
    // otherwise render in the dim "unfocused" state. Highlight it so the focus
    // colour matches the single/network layout.
    entry.button->SetHighlighted(pitchEditing && pos == panel.cursorIndex);

    // Selection is a white outline around the photo (the square frame is gone):
    // swap to the precomputed outlined copy for the cursor/held card.
    if (entry.photoOutline) {
      bool selected = pitchEditing && (pos == panel.cursorIndex || pos == panel.heldIndex);
      if (selected) { entry.photoOutline->Show(); if (entry.photo) entry.photo->Hide(); }
      else { entry.photoOutline->Hide(); if (entry.photo) entry.photo->Show(); }
    }

    int fatigue = FatiguePercent(EntryFatigue(panel, entry));

    if (entry.onPitch) {
      // Keep the portrait in sync with the slot's player. A pre-match swap
      // (PerformAction -> TeamData::SwitchPlayers) refreshes in place without a
      // rebuild, so otherwise the card kept the previous player's photo;
      // LoadImage is a no-op when the path is unchanged.
      std::string face = PlayerFacePath(player);
      if (entry.photo) entry.photo->LoadImage(face);
      if (entry.photoOutline) entry.photoOutline->LoadImage(face);
      FitNameCaption(entry.roleCaption, entry.cardCenterX, entry.roleY, RoleRatingText(role, player), "", entry.cardW);
      DrawFatigueBar(entry, fatigue);
      FitNameCaption(entry.nameCaption, entry.cardCenterX, entry.nameY, player->GetLastName(), suffix, entry.cardW);
    } else {
      entry.button->SetCaption(ShortName(player->GetLastName()) + suffix);
      if (entry.roleCaption) entry.roleCaption->SetCaption(GetRoleName(role));
      if (entry.ratingCaption) entry.ratingCaption->SetCaption(RatingText(player));
      if (entry.fatigueCaption) {
        entry.fatigueCaption->SetCaption(FatigueText(fatigue));
        entry.fatigueCaption->SetColor(FatigueColor(fatigue));
      }
    }
  }

  // Apply the bench/scheme/role visibility here too: SetSection refreshes without
  // a full rebuild, so otherwise the substitute list would flash for one frame
  // when the Tactics/Roles tab opens.
  LayoutSchemes(panel);
  RefreshSchemes(panel);
  LayoutRoles(panel);
  RefreshRoles(panel);
}

void GamePlanPage::CenterCaption(Gui2Caption *caption, float centerX, float y, float height, const std::string &text) {
  if (!caption) return;
  caption->SetCaption(text);
  float width = caption->GetTextWidthPercent();
  caption->SetPosition(centerX - width * 0.5f, y);
}

// PES-like: the card name is cut to the card width (ellipsis) instead of
// spilling over the neighbouring card. The full name stays available in the
// bottom info band. `suffix` (" -", " +", " (out)") is kept intact.
void GamePlanPage::FitNameCaption(Gui2Caption *caption, float centerX, float y, const std::string &name, const std::string &suffix, float maxWidth) {
  if (!caption) return;
  float suffixW = 0.0f;
  if (!suffix.empty()) {
    caption->SetCaption(suffix);
    suffixW = caption->GetTextWidthPercent((int)suffix.length());
  }
  float avail = maxWidth - suffixW;
  caption->SetCaption(name);
  std::string shown = name;
  if (avail > 0.0f && name.length() > 1 && caption->GetTextWidthPercent((int)name.length()) > avail) {
    // Walk UTF-8 character boundaries so a truncated name never splits a
    // multi-byte sequence (SDL_ttf then renders nothing).
    std::vector<int> bounds;
    for (unsigned int i = 0; i < name.length();) {
      bounds.push_back((int)i);
      unsigned char c = (unsigned char)name.at(i);
      unsigned int step = ((c & 0x80) == 0x00) ? 1 : ((c & 0xE0) == 0xC0) ? 2 : ((c & 0xF0) == 0xE0) ? 3 : ((c & 0xF8) == 0xF0) ? 4 : 1;
      i += step;
    }
    int cut = 0;
    for (unsigned int k = 0; k < bounds.size(); k++) {
      if (caption->GetTextWidthPercent(bounds.at(k)) <= avail) cut = bounds.at(k);
      else break;
    }
    shown = name.substr(0, cut);
    if (!shown.empty()) shown += ".";
  }
  caption->SetCaption(shown + suffix);
  caption->SetPosition(centerX - caption->GetTextWidthPercent() * 0.5f, y);
}

float GamePlanPage::EntryFatigue(PlanPanel &panel, const PlanEntry &entry) {
  // Bench players and the pre-match squad never simulate, so they stay at 1.0.
  if (panel.team && entry.playerID >= 0) {
    Player *player = panel.team->GetPlayer(entry.playerID);
    if (player) return player->GetFatigueFactorInv();
  }
  return 1.0f;
}

void GamePlanPage::DrawFatigueBar(PlanEntry &entry, int percent) {
  if (!entry.fatigueBar) return;
  boost::intrusive_ptr<Image2D> img = entry.fatigueBar->GetImage2D();
  if (!img) return;
  Vector3 size = img->GetSize();
  int w = int(size.coords[0]);
  int h = int(size.coords[1]);
  if (w < 1 || h < 1) return;
  float pct = Clampf(percent / 100.0f, 0.0f, 1.0f);
  img->DrawRectangle(0, 0, w, h, Vector3(0, 0, 0), 0);             // clear
  img->DrawRectangle(0, 0, w, h, Vector3(25, 25, 25), 255);        // opaque track
  img->DrawRectangle(0, 0, int(w * pct), h, FatigueColor(percent), 255); // fill
  img->OnChange();
}

e_PlayerRole GamePlanPage::GetEntryRole(int entryPosition) {
  return entries.at(entryPosition).role;
}

void GamePlanPage::UpdateInfo() {
  if (panels.empty()) {
    UpdateInfoDetail(infoPhotoA, infoBadgeA, infoNameA, -1, false);
    UpdateInfoDetail(infoPhotoB, infoBadgeB, infoNameB, -1, false);
    return;
  }

  if (dualPanel) {
    PlanPanel &p0 = panels.at(0), &p1 = panels.at(1);
    // The Roles section owns the info band: it draws its own row list (and, while
    // picking, the hovered player's card) there instead of the shared info view.
    bool showA = !RolesActive(p0);
    bool showB = !RolesActive(p1);
    UpdateInfoDetail(infoPhotoA, infoBadgeA, infoNameA, showA ? (p0.heldIndex != -1 ? p0.heldIndex : p0.cursorIndex) : -1, showA);
    UpdateInfoDetail(infoPhotoB, infoBadgeB, infoNameB, showB ? (p1.heldIndex != -1 ? p1.heldIndex : p1.cursorIndex) : -1, showB);
    return;
  }

  PlanPanel &p = panels.at(0);
  if (p.cursorIndex < 0 || RolesActive(p)) {
    UpdateInfoDetail(infoPhotoA, infoBadgeA, infoNameA, -1, false);
    UpdateInfoDetail(infoPhotoB, infoBadgeB, infoNameB, -1, false);
    return;
  }
  bool comparing = p.heldIndex != -1 && p.heldIndex != p.cursorIndex;
  UpdateInfoDetail(infoPhotoA, infoBadgeA, infoNameA, p.heldIndex != -1 ? p.heldIndex : p.cursorIndex, true);
  UpdateInfoDetail(infoPhotoB, infoBadgeB, infoNameB, comparing ? p.cursorIndex : -1, comparing);
}

void GamePlanPage::UpdateInfoDetail(Gui2Image *photo, Gui2Caption *badge, Gui2Caption *name, int entryPosition, bool visible) {
  if (!photo || !badge || !name) return;
  if (!visible || entryPosition < 0 || entryPosition >= (int)entries.size()) {
    photo->Hide(); badge->Hide(); name->Hide();
    return;
  }
  photo->Show(); badge->Show(); name->Show();
  const PlanEntry &entry = entries.at(entryPosition);
  TeamData *td = GetTeamDataFor(entry.teamID);
  PlayerData *player = td ? td->GetPlayerData(entry.index) : 0;
  if (!player) return;
  photo->LoadImage(PlayerFacePath(player));
  float photoX, photoY;
  photo->GetPosition(photoX, photoY);
  float centerX = photoX + infoPhotoW * 0.5f;
  float badgeY = infoPhotoY + infoPhotoH + 0.2f;
  CenterCaption(badge, centerX, badgeY, infoBadgeH, RoleRatingText(entry.role, player));
  // Full name (PES-style): the card name is ellipsized, the info band shows the
  // whole name of the focused player.
  CenterCaption(name, centerX, badgeY + infoBadgeH + 0.2f, infoNameH, player->GetLastName());
}

void GamePlanPage::HandlePanelInput(PlanPanel &panel, const Vector3 &direction, bool accept, bool back, unsigned long now_ms) {
  // This side's section bar is focused: Left/Right picks a section, Enter opens
  // it, Back votes to leave (a second Back retracts the vote).
  if (panel.barFocused) {
    if (panel.voteReady && (accept || back || direction.GetLength() > 0.5f)) {
      panel.voteReady = false;
      RefreshExitStatus();
      RefreshPanel(panel);
      return;
    }
    if (back) {
      panel.voteReady = true;
      RefreshExitStatus();
      RefreshPanel(panel);
      TryLeave();
      return;
    }
    if (accept) {
      int pi = PanelIndex(panel);
      if (pi >= 0) SectionClicked(pi, panel.barCursor);
      return;
    }
    if (direction.GetLength() > 0.5f) {
      if (now_ms - panel.lastMove_ms < 180) return;
      if (direction.coords[0] < -0.5f) { MoveSectionFocus(panel, -1); panel.lastMove_ms = now_ms; }
      else if (direction.coords[0] > 0.5f) { MoveSectionFocus(panel, 1); panel.lastMove_ms = now_ms; }
      else if (direction.coords[1] > 0.5f && (PositionsActive(panel) || TacticsActive(panel) || RolesActive(panel))) { FocusContent(panel); panel.lastMove_ms = now_ms; }
      return;
    }
    return;
  }

  // Tactics: Up/Down scrolls the scheme list (live preview on the pitch), Enter
  // commits the highlighted scheme, Back returns to the bar and drops a preview.
  if (TacticsActive(panel)) {
    if (back) { FocusSectionBar(panel); return; }
    if (accept) { CommitScheme(panel); return; }
    if (direction.GetLength() > 0.5f) {
      if (now_ms - panel.lastMove_ms < 180) return;
      if (direction.coords[1] < -0.5f) { MoveSchemeCursor(panel, -1); panel.lastMove_ms = now_ms; }
      else if (direction.coords[1] > 0.5f) { MoveSchemeCursor(panel, 1); panel.lastMove_ms = now_ms; }
      return;
    }
    return;
  }

  // Roles: Up/Down scrolls the role list; Enter opens player picking; Back
  // returns to the bar. While picking, arrows hover the pitch (pitch players
  // only), Enter assigns the hovered player and Back cancels back to the list.
  if (RolesActive(panel)) {
    if (panel.rolePicking) {
      if (back) { StopRolePicking(panel); return; }
      if (accept) { ConfirmRolePick(panel); return; }
      if (direction.GetLength() > 0.5f) {
        if (now_ms - panel.lastMove_ms < 180) return;
        int next = FindNextEntry(panel, panel.cursorIndex, direction, true);
        if (next != -1) { SelectPickCursor(panel, next); panel.lastMove_ms = now_ms; }
        return;
      }
      return;
    }
    if (back) { FocusSectionBar(panel); return; }
    if (accept) { StartRolePicking(panel); return; }
    if (direction.GetLength() > 0.5f) {
      if (now_ms - panel.lastMove_ms < 180) return;
      if (direction.coords[1] < -0.5f) { MoveRoleCursor(panel, -1); panel.lastMove_ms = now_ms; }
      else if (direction.coords[1] > 0.5f) { MoveRoleCursor(panel, 1); panel.lastMove_ms = now_ms; }
      return;
    }
    return;
  }

  // Non-position section: nothing to edit; Back returns to the bar.
  if (!PositionsActive(panel)) {
    if (back) FocusSectionBar(panel);
    return;
  }

  // Any fresh input (a button, or a new stick push - not the stick going back to
  // centre) cancels a pending "ready" and returns the player to squad selection.
  if (panel.voteReady && (accept || back || direction.GetLength() > 0.5f)) {
    panel.voteReady = false;
    RefreshExitStatus();
    RefreshPanel(panel);
    return;
  }

  if (back) {
    if (panel.heldIndex != -1) { panel.heldIndex = -1; RefreshPanel(panel); return; }
    // Back returns to this side's section bar; a further Back votes to leave.
    FocusSectionBar(panel);
    return;
  }

  if (accept) {
    if (panel.cursorIndex < 0) return;
    if (panel.heldIndex == -1) panel.heldIndex = panel.cursorIndex;
    else if (panel.heldIndex == panel.cursorIndex) panel.heldIndex = -1;
    else { PerformAction(panel, panel.heldIndex, panel.cursorIndex); panel.heldIndex = -1; }
    Refresh();
    return;
  }

  if (direction.GetLength() > 0.5f) {
    if (now_ms - panel.lastMove_ms < 180) return;
    int next = FindNextEntry(panel, panel.cursorIndex, direction);
    if (next != -1) { SelectCursor(panel, next); panel.lastMove_ms = now_ms; return; }
    // Down at the bottom of the pitch drops to this side's section bar.
    if (direction.coords[1] > 0.5f) FocusSectionBar(panel);
  }
}

int GamePlanPage::PanelForDevice(int controllerIndex, bool keyboard) {
  for (unsigned int p = 0; p < panels.size(); p++) {
    if (keyboard && panelDevice[p] == 0) return (int)p;
    if (!keyboard && panelDevice[p] == controllerIndex) return (int)p;
  }
  return -1;
}

void GamePlanPage::RefreshExitStatus() {
  if (!header) return;
  int ready = 0, total = (int)panels.size();
  for (unsigned int p = 0; p < panels.size(); p++) if (panels.at(p).voteReady) ready++;
  if (ready == 0) CenterCaption(header, 50.0f, 5.0f, 4.0f, "");
  else CenterCaption(header, 50.0f, 5.0f, 4.0f, "READY TO LEAVE " + int_to_str(ready) + "/" + int_to_str(total));
}

void GamePlanPage::TryLeave() {
  if (!dualPanel) { GoBack(); return; }
  for (unsigned int p = 0; p < panels.size(); p++) {
    if (!panels.at(p).voteReady) { RefreshExitStatus(); return; }
  }
  GoBack();
}

void GamePlanPage::ProcessKeyboardEvent(KeyboardEvent *event) {
  if (!dualPanel) { Gui2Page::ProcessKeyboardEvent(event); return; }
  int pi = PanelForDevice(0, true);
  if (pi < 0) return;
  PlanPanel &panel = panels.at(pi);

  const std::vector<IHIDevice*> &controllers = GetControllers();
  HIDKeyboard *keyboard = (!controllers.empty() && controllers.at(0)->GetDeviceType() == e_HIDeviceType_Keyboard)
                              ? static_cast<HIDKeyboard*>(controllers.at(0)) : 0;

  bool accept = event->GetKeyOnce(SDLK_RETURN) || event->GetKeyOnce(SDLK_KP_ENTER) ||
                (keyboard && event->GetKeyOnce(keyboard->GetFunctionMapping(e_ButtonFunction_Shot)));
  bool back = event->GetKeyOnce(SDLK_ESCAPE);
  Vector3 direction(0, 0, 0);
  if (event->GetKeyRepeated(SDLK_LEFT) || (keyboard && event->GetKeyRepeated(keyboard->GetFunctionMapping(e_ButtonFunction_Left)))) direction.coords[0] -= 1;
  if (event->GetKeyRepeated(SDLK_RIGHT) || (keyboard && event->GetKeyRepeated(keyboard->GetFunctionMapping(e_ButtonFunction_Right)))) direction.coords[0] += 1;
  if (event->GetKeyRepeated(SDLK_UP) || (keyboard && event->GetKeyRepeated(keyboard->GetFunctionMapping(e_ButtonFunction_Up)))) direction.coords[1] -= 1;
  if (event->GetKeyRepeated(SDLK_DOWN) || (keyboard && event->GetKeyRepeated(keyboard->GetFunctionMapping(e_ButtonFunction_Down)))) direction.coords[1] += 1;

  HandlePanelInput(panel, direction, accept, back, EnvironmentManager::GetInstance().GetTime_ms());
}

void GamePlanPage::ProcessJoystickEvent(JoystickEvent *event) {
  const std::vector<IHIDevice*> &controllers = GetControllers();

  if (!dualPanel) { Gui2Page::ProcessJoystickEvent(event); return; }

  unsigned long now_ms = EnvironmentManager::GetInstance().GetTime_ms();

  for (unsigned int c = 1; c < controllers.size(); c++) {
    if (controllers.at(c)->GetDeviceType() != e_HIDeviceType_Gamepad) continue;
    HIDGamepad *gamepad = static_cast<HIDGamepad*>(controllers.at(c));
    int joyID = gamepad->GetGamepadID();
    int pi = PanelForDevice((int)c, false);
    if (pi < 0) continue;
    PlanPanel &panel = panels.at(pi);

    bool accept = event->GetButton(joyID, gamepad->GetControllerMapping(e_ControllerButton_A));
    bool back = event->GetButton(joyID, gamepad->GetControllerMapping(e_ControllerButton_B));

    Vector3 direction(0, 0, 0);
    if (gamepad->GetButtonValue(e_ButtonFunction_Left) > 0.5f) direction.coords[0] -= 1;
    if (gamepad->GetButtonValue(e_ButtonFunction_Right) > 0.5f) direction.coords[0] += 1;
    if (gamepad->GetButtonValue(e_ButtonFunction_Up) > 0.5f) direction.coords[1] -= 1;
    if (gamepad->GetButtonValue(e_ButtonFunction_Down) > 0.5f) direction.coords[1] += 1;
    // The D-pad is not part of the function mapping (movement functions map to
    // the stick), so read its semantic buttons directly - same as team select.
    UserEventManager &userEvents = UserEventManager::GetInstance();
    if (userEvents.GetJoyButtonState(joyID, SDL_GAMEPAD_BUTTON_DPAD_UP)) direction.coords[1] -= 1;
    if (userEvents.GetJoyButtonState(joyID, SDL_GAMEPAD_BUTTON_DPAD_DOWN)) direction.coords[1] += 1;
    if (userEvents.GetJoyButtonState(joyID, SDL_GAMEPAD_BUTTON_DPAD_LEFT)) direction.coords[0] -= 1;
    if (userEvents.GetJoyButtonState(joyID, SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) direction.coords[0] += 1;

    HandlePanelInput(panel, direction, accept, back, now_ms);
  }
}

void GamePlanPage::SendPlanSwap(int side, int dbA, int dbB) {
  NetLobbyAction action;
  action.type = e_NetLobbyAction_PlanSwap;
  action.side = side;
  action.value = dbA;
  action.value2 = dbB;
  boost::shared_ptr<NetServer> server = GetMenuTask()->GetNetServer();
  if (server) { action.playerId = 0; server->ApplyLobbyAction(action); return; }
  boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
  if (client) client->SendLobbyAction(action);
}

void GamePlanPage::SendPlanScheme(int side, int scheme) {
  NetLobbyAction action;
  action.type = e_NetLobbyAction_PlanScheme;
  action.side = side;
  action.value = scheme;
  boost::shared_ptr<NetServer> server = GetMenuTask()->GetNetServer();
  if (server) { action.playerId = 0; server->ApplyLobbyAction(action); return; }
  boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
  if (client) client->SendLobbyAction(action);
}

void GamePlanPage::ProcessWindowingEvent(WindowingEvent *event) {
  if (dualPanel) { event->Ignore(); return; }

  PlanPanel &panel = panels.at(0);
  // Role picking is a level below the role list: Back cancels back to the list
  // (not all the way out to the section bar).
  if (RolesActive(panel) && panel.rolePicking && event->IsEscape()) {
    StopRolePicking(panel);
    event->Accept();
    return;
  }
  if (event->IsEscape()) {
    if (panel.heldIndex != -1) { panel.heldIndex = -1; Refresh(); event->Accept(); return; }
    // Back from the pitch returns to the section bar; from the bar it leaves.
    if (!panel.barFocused) { FocusSectionBar(panel); event->Accept(); return; }
    Gui2Page::ProcessWindowingEvent(event);
    return;
  }

  if (panel.barFocused) {
    Vector3 direction = event->GetDirection();
    if (direction.GetLength() > 0.5f) {
      if (moveCooldown_ms < 180) { event->Accept(); return; }
      if (direction.coords[0] < -0.5f) { MoveSectionFocus(panel, -1); moveCooldown_ms = 0; event->Accept(); return; }
      if (direction.coords[0] > 0.5f) { MoveSectionFocus(panel, 1); moveCooldown_ms = 0; event->Accept(); return; }
      if (direction.coords[1] > 0.5f && (PositionsActive(panel) || TacticsActive(panel) || RolesActive(panel))) { FocusContent(panel); moveCooldown_ms = 0; event->Accept(); return; }
      event->Accept();
      return;
    }
    // Enter/A is consumed by the focused bar button's own activate handler.
    Gui2Page::ProcessWindowingEvent(event);
    return;
  }

  if (TacticsActive(panel)) {
    Vector3 direction = event->GetDirection();
    if (direction.GetLength() > 0.5f) {
      if (moveCooldown_ms < 180) { event->Accept(); return; }
      if (direction.coords[1] < -0.5f) { MoveSchemeCursor(panel, -1); moveCooldown_ms = 0; event->Accept(); return; }
      if (direction.coords[1] > 0.5f) { MoveSchemeCursor(panel, 1); moveCooldown_ms = 0; event->Accept(); return; }
      event->Accept();
      return;
    }
    // Enter/A is consumed by the focused scheme button's own activate handler.
    Gui2Page::ProcessWindowingEvent(event);
    return;
  }

  if (RolesActive(panel)) {
    if (panel.rolePicking) {
      // Picking: the list is hidden and the page owns the focus, so Enter/A and
      // Esc/B are handled here (not by a button).
      if (event->IsEscape()) { StopRolePicking(panel); event->Accept(); return; }
      if (event->IsActivate()) { ConfirmRolePick(panel); event->Accept(); return; }
      Vector3 direction = event->GetDirection();
      if (direction.GetLength() > 0.5f) {
        if (moveCooldown_ms < 180) { event->Accept(); return; }
        int next = FindNextEntry(panel, panel.cursorIndex, direction, true);
        if (next != -1) { SelectPickCursor(panel, next); moveCooldown_ms = 0; event->Accept(); return; }
        event->Accept();
        return;
      }
      event->Accept();
      return;
    }
    Vector3 direction = event->GetDirection();
    if (direction.GetLength() > 0.5f) {
      if (moveCooldown_ms < 180) { event->Accept(); return; }
      if (direction.coords[1] < -0.5f) { MoveRoleCursor(panel, -1); moveCooldown_ms = 0; event->Accept(); return; }
      if (direction.coords[1] > 0.5f) { MoveRoleCursor(panel, 1); moveCooldown_ms = 0; event->Accept(); return; }
      event->Accept();
      return;
    }
    // Enter/A is consumed by the focused role button's own activate handler.
    Gui2Page::ProcessWindowingEvent(event);
    return;
  }

  if (PositionsActive(panel)) {
    Vector3 direction = event->GetDirection();
    if (direction.GetLength() > 0.5f) {
      if (moveCooldown_ms < 180) { event->Accept(); return; }
      int next = FindNextEntry(panel, panel.cursorIndex, direction);
      if (next != -1) { SelectCursor(panel, next); moveCooldown_ms = 0; event->Accept(); return; }
      // Down at the bottom of the pitch drops to the section bar.
      if (direction.coords[1] > 0.5f) { FocusSectionBar(panel); event->Accept(); return; }
    }
  }

  Gui2Page::ProcessWindowingEvent(event);
}

void GamePlanPage::Process() {
  // Each peer owns their local plan screen; rebuild when the host relays an
  // authoritative edit (lineup swap or tactical scheme, own or the other peer's).
  if (GetMenuTask()->GetPlanRevision() != seenPlanRevision) {
    seenPlanRevision = GetMenuTask()->GetPlanRevision();
    if (!panels.empty()) Rebuild(panels.at(0).teamID, -1);
  }

  if (rebuildPending) {
    rebuildPending = false;
    Rebuild(rebuildFocusTeam, rebuildFocusSlot);
  }
  if (moveCooldown_ms < 180) moveCooldown_ms += windowManager->GetTimeStep_ms();

  Gui2Page::Process();
}

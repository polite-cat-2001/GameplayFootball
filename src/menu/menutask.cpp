// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#include "menutask.hpp"

#include "../onthepitch/match.hpp"

#include "../data/matchdata.hpp"
#include "../data/teamdata.hpp"

#include "pagefactory.hpp"

#include "tacticschemes.hpp"

#include "mainmenu.hpp"
#include "ingame/ingame.hpp"
#include "visualoptions.hpp"
#include "ingame/replaymenu.hpp"
#include "ingame/phasemenu.hpp"
#include "ingame/gameover.hpp"

#include "gametask.hpp"

#include "../net/netclient.hpp"
#include "../net/netserver.hpp"

#include "main.hpp"

#include "framework/scheduler.hpp"
#include "managers/resourcemanagerpool.hpp"

using namespace blunted;

MenuTask::MenuTask(float aspectRatio, float margin, TTF_Font *defaultFont, TTF_Font *defaultOutlineFont) : Gui2Task(GetScene2D(), aspectRatio, margin) {

  Gui2Style *style = windowManager->GetStyle();

  style->SetFont(e_TextType_Default, defaultFont);
  style->SetFont(e_TextType_DefaultOutline, defaultOutlineFont);
  style->SetFont(e_TextType_Caption, defaultFont);
  style->SetFont(e_TextType_Title, defaultFont);
  style->SetFont(e_TextType_ToolTip, defaultFont);

/* previous colorset
  style->SetColor(e_DecorationType_Dark1, Vector3(0, 0, 0));
  style->SetColor(e_DecorationType_Dark2, Vector3(63, 63, 63));
  style->SetColor(e_DecorationType_Bright1, Vector3(240, 255, 210));
  style->SetColor(e_DecorationType_Bright2, Vector3(214, 194, 154));
  style->SetColor(e_DecorationType_Toggled, Vector3(255, 20, 70));
*/

  // huisstijl:
  // blauw: 0, 100, 220
  // orange: 240, 100, 0
  style->SetColor(e_DecorationType_Dark1, Vector3(20, 35, 55));
  style->SetColor(e_DecorationType_Dark2, Vector3(60, 35, 20));
  style->SetColor(e_DecorationType_Bright1, Vector3(150, 180, 220));
  style->SetColor(e_DecorationType_Bright2, Vector3(240, 150, 100));
  style->SetColor(e_DecorationType_Toggled, Vector3(240, 60, 60));

  windowManager->SetTimeStep_ms(10);

  Gui2Root *root = windowManager->GetRoot();
  root->Show();

  PageFactory *pageFactory = new PageFactory();
  windowManager->SetPageFactory(pageFactory);

  if (!QuickStart()) {

    queuedFixture->team1KitNum = 1;
    queuedFixture->team2KitNum = 2;

    menuAction = e_MenuAction_Menu;

  } else {

    int size = GetControllers().size();
    for (int i = 0; i < size; i++) {
      SideSelection side;
      side.controllerID = i;
      if ((size > 1 && i == 1) || (size == 1 && i == 0)) {
        side.side = -1;
      } else {
        side.side = 0;
      }
      queuedFixture->sides.push_back(side);
    }

    // 1 == ajax
    // 2 == arsenal
    // 3 == barcelona
    // 4 == bayern
    // 5 == borussia
    // 6 == man utd
    // 7 == psv
    // 8 == real madrid
    queuedFixture->teamID1 = "3";
    queuedFixture->teamID2 = "8";
    queuedFixture->team1KitNum = 2;
    queuedFixture->team2KitNum = 2;

    menuAction = e_MenuAction_Menu;

  }

}

MenuTask::~MenuTask() {
  if (Verbose()) printf("exiting menutask.. ");

  delete windowManager->GetPageFactory();

  if (Verbose()) printf("done\n");
}

void MenuTask::ProcessPhase() {

  Gui2Task::ProcessPhase();

  if (menuAction == e_MenuAction_Menu) {

    // Leaving a network match (forfeit / game over) must tear the session down.
    // Otherwise a stale server/client makes the next local match look networked
    // and its pause menu opens the old mirrored lobby.
    if (netServer) { netServer->Stop(); netServer.reset(); }
    if (netClient) { netClient->Disconnect(); netClient.reset(); }

    windowManager->GetPagePath()->Clear();

    GetGameTask()->Action(e_GameTaskMessage_StopMatch);
    GetGameTask()->Action(e_GameTaskMessage_StartMenuScene);

    Properties properties;
    if (!QuickStart()) {
      if (!IsReleaseVersion()) {
        windowManager->GetPageFactory()->CreatePage((int)e_PageID_MainMenu, properties, 0);
      } else {
        windowManager->GetPageFactory()->CreatePage((int)e_PageID_Intro, properties, 0);
      }
    } else {
      windowManager->GetPageFactory()->CreatePage((int)e_PageID_LoadingMatch, properties, 0);
    }

  } else if (menuAction == e_MenuAction_Game) {

    GetGameTask()->Action(e_GameTaskMessage_StopMenuScene);
    GetGameTask()->Action(e_GameTaskMessage_StartMatch);

  }

  UpdateNetworkOverlay();
  UpdateGamepadMissingOverlay();
  ProcessNetworkPlanEdits();
  ProcessNetworkHubVotes();

  menuAction = e_MenuAction_None;
}

void MenuTask::UpdateNetworkOverlay() {
  // The menu layer owns page navigation; GameTask only exposes the match state.
  // While a network match wants side selection, surface the mirrored overlay on
  // every peer (even while the pause menu, not GamePage, is the active page).
  boost::shared_ptr<GameTask> gameTask = GetGameTask();
  if (!gameTask) return;
  NetMatchSession *session = gameTask->GetNetSession();
  if (!session || !session->IsActive() || !gameTask->GetMatch()) return;
  if (session->GetState() != e_NetMatchPhaseState_SideSelect) return;

  const std::vector<Gui2PageData> &pageStack = windowManager->GetPagePath()->GetPath();
  if (!pageStack.empty() && pageStack.back().pageID == e_PageID_SideSelect) return;

  Properties properties;
  properties.SetBool("isInGame", true);
  properties.SetBool("resumeOnClose", true);
  // Open through the top page (Gui2Page::CreatePage) so the current page is
  // properly replaced in the stack; opening via the page factory directly would
  // leave the previous page in the root and leak it.
  Gui2Page *topPage = windowManager->GetPageFactory()->GetMostRecentlyCreatedPage();
  if (topPage) {
    topPage->CreatePage((int)e_PageID_SideSelect, properties, 0);
  } else {
    windowManager->GetPageFactory()->CreatePage((int)e_PageID_SideSelect, properties, 0);
  }
}

int MenuTask::GetLocalNetworkTeamID() {
  NetLobbyState state;
  int localId = 0;
  if (netServer) {
    state = netServer->GetLobbyState();
  } else if (netClient) {
    state = netClient->GetLobbyState();
    localId = (int)netClient->GetPlayerId();
  } else {
    return -1;
  }

  for (unsigned int i = 0; i < state.players.size(); i++) {
    if ((int)state.players.at(i).id != localId) continue;
    if (state.players.at(i).side == e_NetSide_Home) return 0;
    if (state.players.at(i).side == e_NetSide_Away) return 1;
    return -1; // spectator/centre
  }
  return -1;
}

void MenuTask::GetLocalSideDevices(int device[2]) {
  device[0] = device[1] = -1;
  const std::vector<SideSelection> sides = GetControllerSetup();
  for (unsigned int i = 0; i < sides.size(); i++) {
    int idx = -1;
    if (sides.at(i).side < 0) idx = 0;
    else if (sides.at(i).side > 0) idx = 1;
    if (idx >= 0) device[idx] = sides.at(i).controllerID;
  }
}

bool MenuTask::HasTwoLocalPlayers() {
  int device[2];
  GetLocalSideDevices(device);
  if (device[0] < 0 || device[1] < 0 || device[0] == device[1]) return false;

  const std::vector<IHIDevice*> &controllers = GetControllers();
  if (device[0] >= (int)controllers.size() || device[1] >= (int)controllers.size()) return false;
  // A single keyboard can't drive two sides at once.
  if (controllers.at(device[0])->GetDeviceType() == e_HIDeviceType_Keyboard &&
      controllers.at(device[1])->GetDeviceType() == e_HIDeviceType_Keyboard) return false;
  return true;
}

bool MenuTask::PlayerOwnsSide(uint32_t playerId, int side) {
  NetLobbyState state;
  if (netServer) state = netServer->GetLobbyState();
  else if (netClient) state = netClient->GetLobbyState();
  else return false;

  for (unsigned int i = 0; i < state.players.size(); i++) {
    if (state.players.at(i).id != playerId) continue;
    return (side == 0 && state.players.at(i).side == e_NetSide_Home) ||
           (side == 1 && state.players.at(i).side == e_NetSide_Away);
  }
  return false;
}

void MenuTask::ApplyPlanSwap(int side, int dbA, int dbB) {
  MatchData *matchData = GetMatchData();
  if (!matchData || side < 0 || side > 1 || dbA < 0 || dbB < 0 || dbA == dbB) return;
  TeamData *teamData = matchData->GetTeamData(side);
  if (!teamData) return;
  teamData->SwitchPlayers(dbA, dbB);
  planRevision++;
}

void MenuTask::ApplyPlanScheme(int side, int scheme) {
  Match *match = GetGameTask() ? GetGameTask()->GetMatch() : 0;
  if (match) {
    // In-match: edit the live team's runtime formation.
    Team *team = match->GetTeam(side);
    if (!team) return;
    ApplySchemeToTeam(team, scheme);
  } else {
    // Pre-match: edit the shared MatchData lineup.
    MatchData *matchData = GetMatchData();
    TeamData *teamData = matchData ? matchData->GetTeamData(side) : 0;
    if (!teamData) return;
    ApplySchemeToTeamData(teamData, scheme);
  }
  planRevision++;
}

void MenuTask::ApplyPlanRole(int side, int role, int slot) {
  if (side < 0 || side > 1) return;
  if (role < 0 || role >= e_TeamRole_SIZE) return;
  Match *match = GetGameTask() ? GetGameTask()->GetMatch() : 0;
  if (match) {
    // In-match: write through the live team (shared MatchData).
    Team *team = match->GetTeam(side);
    if (!team) return;
    team->SetRolePlayer((e_TeamRole)role, slot);
  } else {
    // Pre-match: edit the shared MatchData directly.
    MatchData *matchData = GetMatchData();
    if (!matchData) return;
    matchData->SetRolePlayer(side, (e_TeamRole)role, slot);
  }
  planRevision++;
}

void MenuTask::ProcessNetworkPlanEdits() {
  if (netServer) {
    // Host: apply client edits under ownership check, then relay to everyone.
    NetPlanSwapRequest request;
    while (netServer->ConsumePlanSwapRequest(request)) {
      if (!PlayerOwnsSide(request.playerId, request.side)) continue;
      ApplyPlanSwap(request.side, request.dbA, request.dbB);
      NetPlanSwap swap;
      swap.side = request.side;
      swap.dbA = request.dbA;
      swap.dbB = request.dbB;
      netServer->BroadcastPlanSwap(swap);
    }
    NetPlanSchemeRequest schemeRequest;
    while (netServer->ConsumePlanSchemeRequest(schemeRequest)) {
      if (!PlayerOwnsSide(schemeRequest.playerId, schemeRequest.side)) continue;
      ApplyPlanScheme(schemeRequest.side, schemeRequest.scheme);
      NetPlanScheme scheme;
      scheme.side = schemeRequest.side;
      scheme.scheme = schemeRequest.scheme;
      netServer->BroadcastPlanScheme(scheme);
    }
    NetPlanRoleRequest roleRequest;
    while (netServer->ConsumePlanRoleRequest(roleRequest)) {
      if (!PlayerOwnsSide(roleRequest.playerId, roleRequest.side)) continue;
      ApplyPlanRole(roleRequest.side, roleRequest.role, roleRequest.slot);
      NetPlanRole role;
      role.side = roleRequest.side;
      role.role = roleRequest.role;
      role.slot = roleRequest.slot;
      netServer->BroadcastPlanRole(role);
    }
  } else if (netClient) {
    NetPlanSwap swap;
    while (netClient->ConsumePlanSwap(swap)) ApplyPlanSwap(swap.side, swap.dbA, swap.dbB);
    NetPlanScheme scheme;
    while (netClient->ConsumePlanScheme(scheme)) ApplyPlanScheme(scheme.side, scheme.scheme);
    NetPlanRole role;
    while (netClient->ConsumePlanRole(role)) ApplyPlanRole(role.side, role.role, role.slot);
  }
}

void MenuTask::ProcessNetworkHubVotes() {
  if (!netServer) return;
  int vote = e_NetHubVote_None;
  while (netServer->ConsumeHubVoteResult(vote)) {
    if (vote == e_NetHubVote_BackToTeams) {
      NetLobbyAction action;
      action.type = e_NetLobbyAction_BackToTeams;
      action.playerId = 0;
      netServer->ApplyLobbyAction(action);
    } else if (vote == e_NetHubVote_StartMatch) {
      hubStartRequested = true;
    }
  }
}

void MenuTask::UpdateGamepadMissingOverlay() {
  // The game thread rescans gamepads each tick (GameTask::ProcessPhase), which
  // runs after MenuTask::Process, so this sees the previous tick's controller
  // list — a frame of lag, fine for a check already rate-limited to 1 s.
  Match *match = GetGameTask() ? GetGameTask()->GetMatch() : 0;
  if (!match) return;

  // A network match handles roster/device changes through the mirrored side
  // screen (host authority); a local controller-select overlay must not appear.
  if (GetNetServer() || GetNetClient()) return;

  unsigned long now_ms = EnvironmentManager::GetInstance().GetTime_ms();
  if (now_ms - lastGamepadCheckTime_ms <= 1000) return;
  lastGamepadCheckTime_ms = now_ms;

  bool anyGamerDeviceMissing = false;
  const std::vector<SideSelection> sides = GetControllerSetup();
  const std::vector<IHIDevice*> &controllers = GetControllers();
  for (unsigned int i = 0; i < sides.size(); i++) {
    if (sides.at(i).side == 0) continue;
    if (sides.at(i).joystickID == 0) continue; // keyboard
    bool found = false;
    for (unsigned int c = 1; c < controllers.size(); c++) {
      if (static_cast<HIDGamepad*>(controllers.at(c))->GetJoystickID() == sides.at(i).joystickID) { found = true; break; }
    }
    if (!found) { anyGamerDeviceMissing = true; break; }
  }

  // only open the window if it is not already on top of the page stack
  bool controllerSelectOpen = false;
  const std::vector<Gui2PageData> &pageStack = windowManager->GetPagePath()->GetPath();
  if (!pageStack.empty() && pageStack.back().pageID == e_PageID_SideSelect) controllerSelectOpen = true;
  if (!anyGamerDeviceMissing || controllerSelectOpen) return;

  // pause the match (unless it is already paused) and show controller select on
  // top. If we paused it ourselves, mark resumeOnClose so the window resumes the
  // match when it closes.
  bool wasPaused = match->GetPause();
  if (!wasPaused) match->Pause(true);
  Properties csProps;
  csProps.SetBool("isInGame", true);
  csProps.SetBool("resumeOnClose", !wasPaused);
  // open through the top page (Gui2Page::CreatePage) so the current page is
  // properly replaced in the stack; opening via the page factory directly would
  // leave the previous page in the root and leak it.
  Gui2Page *topPage = windowManager->GetPageFactory()->GetMostRecentlyCreatedPage();
  if (topPage) {
    topPage->CreatePage((int)e_PageID_SideSelect, csProps, 0);
  } else {
    windowManager->GetPageFactory()->CreatePage((int)e_PageID_SideSelect, csProps, 0);
  }
}

bool MenuTask::QuickStart() {
  return !IsReleaseVersion() && EnvironmentManager::GetInstance().GetTime_ms() < 10000; // after 5 seconds, quickstart disabled (== after > 0 matches have been played)
}

void MenuTask::QuitGame() {
  EnvironmentManager::GetInstance().SignalQuit();
}

void MenuTask::ReleaseAllButtons() {
  // when going back to game, depress all buttons, so we don't go around doing passes we don't want
  for (int joyID = 0; joyID < UserEventManager::GetInstance().GetJoystickCount(); joyID++) {
    for (unsigned int buttonID = 0; buttonID < blunted::_JOYSTICK_MAXBUTTONS; buttonID++) {
      UserEventManager::GetInstance().SetJoyButtonState(joyID, buttonID, false);
    }
  }
  UserEventManager::GetInstance().SetKeyboardState(SDLK_ESCAPE, false);
}

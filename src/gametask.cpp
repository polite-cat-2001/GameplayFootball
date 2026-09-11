// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#include "gametask.hpp"

#include "main.hpp"

#include "base/log.hpp"

#include "framework/scheduler.hpp"
#include "managers/taskmanager.hpp"
#include "managers/resourcemanagerpool.hpp"
#include "managers/environmentmanager.hpp"
#include "menu/pagefactory.hpp"
#include "base/properties.hpp"

#include "net/netclient.hpp"
#include "net/netserver.hpp"

#include "blunted.hpp"

void GameTask::RebindNetworkControllers() {
  if (!match || match->IsRemotePresentation()) return;
  netSession.RebindControllers(match);
}

void GameTask::ApplyNetworkControllers() {
  if (!match || match->IsRemotePresentation()) return;
  netSession.SetupControllers(match);
}

void GameTask::OpenNetworkSideSelect() {
  const std::vector<Gui2PageData> &pageStack = GetMenuTask()->GetWindowManager()->GetPagePath()->GetPath();
  if (!pageStack.empty() && pageStack.back().pageID == e_PageID_NetworkLobby) return;

  Properties props;
  props.SetBool("isInGame", true);
  props.SetBool("resumeOnClose", true);
  // Open through the top page (Gui2Page::CreatePage) so the current page is
  // properly replaced in the stack; opening via the page factory directly would
  // leave the previous page in the root and leak it.
  Gui2Page *topPage = GetMenuTask()->GetWindowManager()->GetPageFactory()->GetMostRecentlyCreatedPage();
  if (topPage) {
    topPage->CreatePage((int)e_PageID_NetworkLobby, props, 0);
  } else {
    GetMenuTask()->GetWindowManager()->GetPageFactory()->CreatePage((int)e_PageID_NetworkLobby, props, 0);
  }
}

void UploadFullbodyModel::Update() {
  for (unsigned int i = 0; i < geometryToUpload.size(); i++) {
    geometryToUpload.at(i)->OnUpdateGeometryData(false);
  }
}

GameTask::GameTask() {

  match = 0;
  menuScene = 0;

  // prohibits deletion of the scene before this object is dead
  scene3D = GetScene3D();
}

GameTask::~GameTask() {
  if (Verbose()) printf("exiting gametask.. ");
  Exit();
  if (Verbose()) printf("done\n");
}

void GameTask::Exit() {

  Action(e_GameTaskMessage_StopMatch);
  Action(e_GameTaskMessage_StopMenuScene);

  ResourceManagerPool::GetInstance().CleanUp();

  scene3D.reset();
}

void GameTask::Action(e_GameTaskMessage message) {

  switch (message) {

    case e_GameTaskMessage_StartMatch:
      {
        if (Verbose()) printf("*gametaskmessage: starting match\n");

        GetGraphicsSystem()->getPhaseMutex.lock();
        MatchData *matchData = GetMenuTask()->GetMatchData();
        assert(matchData);
        Match *tmpMatch = new Match(matchData, GetControllers());

        // A LAN client never simulates: it replays host snapshots through the
        // normal render pipeline instead.
        if (GetMenuTask()->GetNetClient()) {
          tmpMatch->SetRemotePresentation(true);
          tmpMatch->SetLocalPeerId((int)GetMenuTask()->GetNetClient()->GetPlayerId());
        }

        matchLifetimeMutex.lock();
        matchPutBufferMutex.lock();
        assert(!match);
        match = tmpMatch;
        GetScheduler()->ResetTaskSequenceTime("game");
        matchPutBufferMutex.unlock();
        matchLifetimeMutex.unlock();
        GetGraphicsSystem()->getPhaseMutex.unlock();

        // Network glue: (host) bind controllers, clear roster churn, publish the
        // animation table + environment; (client) reset the input queue.
        netSession.StartMatch(match);
      }
      break;

    case e_GameTaskMessage_StopMatch:
      if (Verbose()) printf("*gametaskmessage: stopping match\n");

      netSession.StopMatch();

      GetGraphicsSystem()->getPhaseMutex.lock();
      matchLifetimeMutex.lock();
      matchPutBufferMutex.lock();
      //assert(match);
      if (match) {
        match->Exit();
        delete match;
        match = 0;
      }
      matchPutBufferMutex.unlock();
      matchLifetimeMutex.unlock();
      GetGraphicsSystem()->getPhaseMutex.unlock();
      break;

    case e_GameTaskMessage_StartMenuScene:
      if (Verbose()) printf("*gametaskmessage: starting menu scene\n");

      GetGraphicsSystem()->getPhaseMutex.lock();
      menuSceneLifetimeMutex.lock();
      assert(!menuScene);
      menuScene = new MenuScene();
      GetScheduler()->ResetTaskSequenceTime("game");
      menuSceneLifetimeMutex.unlock();
      GetGraphicsSystem()->getPhaseMutex.unlock();
      break;

    case e_GameTaskMessage_StopMenuScene:
      if (Verbose()) printf("*gametaskmessage: stopping menu scene\n");

      GetGraphicsSystem()->getPhaseMutex.lock();
      menuSceneLifetimeMutex.lock();
      //assert(menuScene);
      if (menuScene) {
        delete menuScene;
        menuScene = 0;
      }
      menuSceneLifetimeMutex.unlock();
      GetGraphicsSystem()->getPhaseMutex.unlock();
      break;

    default:
      break;

  }
}

void GameTask::GetPhase() {

  // process messageQueue
  if (match) match->Get();
  if (menuScene) menuScene->Get();
}

void GameTask::ProcessPhase() {

  bool gamepadsChanged = RefreshGamepads();

  // if a gamepad was plugged/unplugged, re-bind human gamers so the match never
  // reads a destroyed HIDGamepad (RefreshGamepads deletes missing devices).
  // A network match manages its gamers explicitly (local + NetHIDDevice), so a
  // local rescan must not wipe them.
  const bool networkMatch = GetMenuTask()->GetNetServer() || GetMenuTask()->GetNetClient();
  if (gamepadsChanged && match && !networkMatch) {
    match->UpdateControllerSetup();
  }

  // if a human gamepad was unplugged mid-match: pause and open controller select on top
  if (match) {
    unsigned long now_ms = EnvironmentManager::GetInstance().GetTime_ms();
    if (now_ms - lastGamepadCheckTime_ms > 1000) {
      lastGamepadCheckTime_ms = now_ms;
      bool anyGamerDeviceMissing = false;
      const std::vector<SideSelection> sides = GetMenuTask()->GetControllerSetup();
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
      const std::vector<Gui2PageData> &pageStack = GetMenuTask()->GetWindowManager()->GetPagePath()->GetPath();
      if (!pageStack.empty() && pageStack.back().pageID == e_PageID_ControllerSelect) controllerSelectOpen = true;
      if (anyGamerDeviceMissing && !controllerSelectOpen) {
        // pause the match (unless it is already paused) and show controller
        // select on top. If we paused it ourselves, mark resumeOnClose so the
        // window can resume the match when it closes.
        bool wasPaused = match->GetPause();
        if (!wasPaused) match->Pause(true);
        Properties csProps;
        csProps.SetBool("isInGame", true);
        csProps.SetBool("resumeOnClose", !wasPaused);
        // open through the top page (Gui2Page::CreatePage) so the current page
        // is properly replaced in the stack; opening via the page factory
        // directly would leave the previous page in the root and leak it.
        Gui2Page *topPage = GetMenuTask()->GetWindowManager()->GetPageFactory()->GetMostRecentlyCreatedPage();
        if (topPage) {
          topPage->CreatePage((int)e_PageID_ControllerSelect, csProps, 0);
        } else {
          GetMenuTask()->GetWindowManager()->GetPageFactory()->CreatePage((int)e_PageID_ControllerSelect, csProps, 0);
        }
      }
    }
  }

  for (unsigned int i = 0; i < GetControllers().size(); i++) {
    GetControllers().at(i)->Process();
  }

  if (match) {
    if (netSession.IsActive()) {
      // Host simulates / client applies snapshots; both then prepare the Put
      // buffers (locked) and the host relays the render state.
      netSession.Process(match);

      matchPutBufferMutex.lock();
      match->PreparePutBuffers();
      matchPutBufferMutex.unlock();

      netSession.BroadcastSnapshot(match);

      // Mirrored side selection surfaces on every peer, even while the pause
      // menu (not GamePage) is the active page. Open centrally from the state.
      if (netSession.GetState() == e_NetMatchPhaseState_SideSelect) {
        const std::vector<Gui2PageData> &pageStack = GetMenuTask()->GetWindowManager()->GetPagePath()->GetPath();
        if (pageStack.empty() || pageStack.back().pageID != e_PageID_NetworkLobby) OpenNetworkSideSelect();
      }
    } else {
      match->Process();

      matchPutBufferMutex.lock();
      match->PreparePutBuffers();
      matchPutBufferMutex.unlock();
    }
  }

  if (menuScene) {
    menuScene->Process();
  }

}

void GameTask::PutPhase() {

  std::vector < boost::intrusive_ptr<UpdateFullbodyModel> > updateFullbodyModels;
  std::vector < boost::intrusive_ptr<UploadFullbodyModel> > uploadFullbodyModels;
  std::vector<PlayerBase*> playersToProcess;

  matchLifetimeMutex.lock();

  if (match) {

    matchPutBufferMutex.lock();
    match->FetchPutBuffers();
    matchPutBufferMutex.unlock();

    match->Put();

    std::vector<Player*> players;
    match->GetActiveTeamPlayers(0, players);
    match->GetActiveTeamPlayers(1, players);
    std::vector<PlayerBase*> officials;
    match->GetOfficialPlayers(officials);

    for (unsigned int i = 0; i < players.size(); i++) {
      if (match->GetPause() || players.at(i)->NeedsModelUpdate()) playersToProcess.push_back(players.at(i));
    }
    for (unsigned int i = 0; i < officials.size(); i++) {
      playersToProcess.push_back(officials.at(i));
    }

    //printf("%i players, %i threads.\n", playersToProcess.size(), threadCount);
    unsigned int playersPerThread = 7;
    unsigned int playerStartIndex = 0;
    while (playerStartIndex < playersToProcess.size()) {
      std::vector<PlayerBase*> playersToProcessInThread;
      for (unsigned int p = 0; p < playersPerThread; p++) {
        if (playerStartIndex + p >= playersToProcess.size()) break;
        playersToProcessInThread.push_back(playersToProcess.at(playerStartIndex + p));
        //printf("adding player %i\n", playerStartIndex + p);
        // unthreaded version: playersToProcess.at(playerStartIndex + p)->UpdateFullbodyModel();
      }
      playerStartIndex += playersPerThread;

      boost::intrusive_ptr<UpdateFullbodyModel> updateFullbodyModel(new UpdateFullbodyModel(playersToProcessInThread));
      updateFullbodyModels.push_back(updateFullbodyModel);
      TaskManager::GetInstance().EnqueueWork(updateFullbodyModel, true);
    }

    match->UploadGoalNetting(); // won't this block the whole process thing too? (opengl busy == wait, while mutex locked == no process)

  }


  for (unsigned int t = 0; t < updateFullbodyModels.size(); t++) {
    updateFullbodyModels.at(t)->Wait();
  }

  if (match) {

    unsigned int playersPerThread = 7;
    unsigned int playerStartIndex = 0;
    while (playerStartIndex < playersToProcess.size()) {
      std::vector < boost::intrusive_ptr<Geometry> > geometryToUploadInThread;
      for (unsigned int p = 0; p < playersPerThread; p++) {
        if (playerStartIndex + p >= playersToProcess.size()) break;
        geometryToUploadInThread.push_back(boost::static_pointer_cast<Geometry>(playersToProcess.at(playerStartIndex + p)->GetFullbodyNode()->GetObject("fullbody")));
      }
      playerStartIndex += playersPerThread;

      boost::intrusive_ptr<UploadFullbodyModel> uploadFullbodyModel(new UploadFullbodyModel(geometryToUploadInThread));
      uploadFullbodyModels.push_back(uploadFullbodyModel);
      TaskManager::GetInstance().EnqueueWork(uploadFullbodyModel, true);

      //working on: maybe we need to use the gfx system get pointer somewhere here? too tired to analyse this now :p
    }

  } // !match

  matchLifetimeMutex.unlock();

  menuSceneLifetimeMutex.lock();
  if (menuScene) menuScene->Put();
  menuSceneLifetimeMutex.unlock();

}

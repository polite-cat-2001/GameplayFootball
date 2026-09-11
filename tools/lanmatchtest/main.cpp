// Headless integration test for the host side of a LAN match: a real Match +
// NetServer + a raw NetClient peer, driven through NetMatchSession.
//
// Covers the gameplay-level flows that the netlib-only `nettest` cannot reach:
// match start / controller binding, pause request, resume vote, mirrored side
// selection open/cancel, and disconnect (roster change -> pause + side select).
//
// The client-side remote presentation is intentionally not instantiated (two
// Matches cannot coexist in one process: PlayerBase::id / controllers / MenuTask
// are process-global). Client-side protocol behaviour is covered by `nettest`.
//
// Prints PASS/FAIL and exits 0/1 (via ::exit, like determinism_runner, because
// tearing down the GUI/rendering globals headlessly would crash).

#ifdef WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <windows.h>
#endif

#include "blunted.hpp"
#include "gamecontext.hpp"
#include "gametask.hpp"
#include "menu/menutask.hpp"
#include "menu/pagefactory.hpp"
#include "data/matchdata.hpp"
#include "onthepitch/match.hpp"
#include "net/matchsnapshot.hpp"
#include "net/netmatchsession.hpp"
#include "net/netclient.hpp"
#include "net/netserver.hpp"

#include "SDL3_ttf/SDL_ttf.h"

#include <boost/make_shared.hpp>

#include <chrono>
#include <cstdlib>
#include <functional>
#include <string>
#include <thread>

using namespace blunted;

#if defined(WIN32) && defined(__MINGW32__)
#undef main
#endif

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond, msg) \
  do { \
    g_checks++; \
    if (!(cond)) { std::printf("FAIL: %s\n", msg); g_failures++; } \
  } while (0)

static void SleepMs(int ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

int main(int argc, char **argv) {
  uint16_t port = 27600;
  if (argc > 1) port = (uint16_t)std::atoi(argv[1]);

  Properties config;
  config.LoadFile("football.config");
  config.Set("graphics3d_renderer", "mock");
  config.Set("audio_renderer", "mock");
  config.Set("match_difficulty", 0.8f);
  config.Set("match_duration", 1.0f);

  Initialize(config);

#ifdef __APPLE__
  if (!InitGameSystems(config)) ::exit(1);
  graphicsSystem->GetRenderer3D()->Run();
#endif

  if (!InitGameContext(config)) ::exit(1);

  gameTask = boost::shared_ptr<GameTask>(new GameTask());

  std::string fontFile = config.Get("font_filename", "media/fonts/alegreya/AlegreyaSansSC-ExtraBold.ttf");
  TTF_Font *defaultFont = TTF_OpenFont(fontFile.c_str(), 32);
  if (!defaultFont) { std::printf("FAIL: could not load font\n"); ::exit(1); }
  TTF_Font *defaultOutlineFont = TTF_OpenFont(fontFile.c_str(), 32);
  TTF_SetFontOutline(defaultOutlineFont, 2);
  menuTask = boost::shared_ptr<MenuTask>(new MenuTask(5.0f / 4.0f, 0, defaultFont, defaultOutlineFont));

  boost::shared_ptr<TaskSequence> gameSequence(new TaskSequence("game", 10, false));
  gameSequence->AddUserTaskEntry(menuTask, e_TaskPhase_Get);
  GetScheduler()->RegisterTaskSequence(gameSequence);

  menuTask->SetTeamIDs("3", "8");
  Properties loadingProps;
  menuTask->GetWindowManager()->GetPageFactory()->CreatePage((int)e_PageID_LoadingMatch, loadingProps, 0);

  MatchData *matchData = new MatchData(menuTask->GetTeamID(0), menuTask->GetTeamID(1));
  menuTask->SetMatchData(matchData);

  randomize(42);

  Match *match = new Match(matchData, GetControllers());

  // Snapshot round-trip: covers the header the thin client decodes, including
  // the relayed on-screen message (goal scorer).
  match->SpamMessage("host test message", 1000);
  {
    NetBuffer buffer;
    match->CaptureRemoteSnapshot(buffer);
    buffer.ResetRead();
    Snapshot snapshot = ReadSnapshot(buffer);
    CHECK(!buffer.Failed(), "snapshot buffer failed");
    CHECK(snapshot.matchTime_ms == match->GetMatchTime_ms(), "snapshot match time mismatch");
    CHECK(snapshot.message == "host test message", "snapshot message mismatch");
    CHECK(snapshot.messageCounter == match->GetSpamMessageCounter(), "snapshot message counter mismatch");
    CHECK(snapshot.maxRtt_ms == 0, "snapshot maxRtt should be 0 without clients");
  }

  // Client-side interpolation: discrete state comes from the newer snapshot,
  // continuous transforms blend at t; frameNum only blends within one anim.
  {
    Snapshot older;
    Snapshot newer;
    older.ballPosition = Vector3(0, 0, 0);
    newer.ballPosition = Vector3(10, 0, 0);
    SnapshotPlayer pa;
    pa.team = 0;
    pa.slot = 0;
    pa.animID = 1;
    pa.frameNum = 0;
    pa.position = Vector3(0, 0, 0);
    SnapshotPlayer pb = pa;
    pb.frameNum = 10;
    pb.position = Vector3(10, 0, 0);
    older.players.push_back(pa);
    newer.players.push_back(pb);
    newer.score[0] = 3;

    Snapshot mid = BlendSnapshots(older, newer, 0.5f);
    CHECK(mid.ballPosition.GetDistance(Vector3(5, 0, 0)) < 1e-4f, "blend ball midpoint mismatch");
    CHECK(mid.players.at(0).position.GetDistance(Vector3(5, 0, 0)) < 1e-4f, "blend player midpoint mismatch");
    CHECK(mid.players.at(0).frameNum == 5, "blend frame midpoint mismatch");
    CHECK(mid.score[0] == 3, "blend should keep newer discrete state");

    newer.players.at(0).animID = 2; // different animation: frame snaps to newer
    Snapshot snapped = BlendSnapshots(older, newer, 0.5f);
    CHECK(snapped.players.at(0).frameNum == 10, "blend should snap frame across animations");
  }

  // Snapshot carries the host's animation blend state, and the position is not
  // blended across a noPos change.
  {
    Snapshot s;
    SnapshotPlayer p;
    p.team = 0;
    p.slot = 0;
    p.animID = 1;
    p.frameNum = 4;
    p.smooth = true;
    p.smoothFactor = 0.6f;
    p.noPos = true;
    p.position = Vector3(1, 2, 3);
    s.players.push_back(p);

    NetBuffer buffer;
    WriteSnapshot(buffer, s);
    buffer.ResetRead();
    Snapshot restored = ReadSnapshot(buffer);
    CHECK(!buffer.Failed(), "snapshot blend-state buffer failed");
    CHECK(restored.players.size() == 1 &&
          restored.players.at(0).smooth == true &&
          restored.players.at(0).smoothFactor == 0.6f &&
          restored.players.at(0).noPos == true,
          "snapshot blend-state round-trip mismatch");

    Snapshot older = s;
    older.players.at(0).position = Vector3(-5, 0, 0);
    older.players.at(0).noPos = false;
    Snapshot newer = s;
    newer.players.at(0).position = Vector3(5, 0, 0);
    newer.players.at(0).noPos = true;
    Snapshot merged = BlendSnapshots(older, newer, 0.5f);
    CHECK(merged.players.at(0).position.GetDistance(Vector3(5, 0, 0)) < 1e-4f,
          "position must snap (not blend) across a noPos change");
  }

  // --- network setup: host server + one raw client peer ---------------------
  boost::shared_ptr<NetServer> server = boost::make_shared<NetServer>(port);
  if (!server->Start()) { std::printf("FAIL: server could not bind %u\n", (unsigned int)port); ::exit(1); }
  menuTask->SetNetServer(server);

  NetClient client;
  client.Connect(NetAddress("127.0.0.1", port));

  bool connected = false;
  for (int i = 0; i < 200 && !connected; i++) {
    SleepMs(10);
    connected = client.GetState() == e_NetConnectionState_Connected;
  }
  CHECK(connected, "client did not connect");
  CHECK(server->GetLobbyState().players.size() == 2, "lobby should have host + client");

  // Client picks the opposite side before the match starts.
  {
    NetLobbyAction action;
    action.type = e_NetLobbyAction_SetSide;
    action.side = e_NetSide_Away;
    client.SendLobbyAction(action);
    bool applied = false;
    for (int i = 0; i < 200 && !applied; i++) {
      SleepMs(10);
      NetLobbyState state = server->GetLobbyState();
      for (unsigned int p = 0; p < state.players.size(); p++) {
        if (state.players.at(p).id == client.GetPlayerId()) applied = state.players.at(p).side == e_NetSide_Away;
      }
    }
    CHECK(applied, "client side not applied before match");
  }

  NetMatchSession session;
  session.StartMatch(match);

  auto tick = [&]() {
    session.Process(match);
    match->PreparePutBuffers();
    session.BroadcastSnapshot(match);
  };
  auto pumpUntil = [&](const std::function<bool()> &predicate, int timeoutMs) {
    int waited = 0;
    while (waited < timeoutMs) {
      tick();
      if (predicate()) return true;
      SleepMs(5);
      waited += 5;
    }
    return predicate();
  };

  // --- 1. match runs --------------------------------------------------------
  for (int i = 0; i < 10; i++) tick();
  CHECK(session.GetState() == e_NetMatchPhaseState_Playing, "initial state should be Playing");
  CHECK(!match->GetPause(), "match should not be paused at start");

  // --- 2. client pauses -----------------------------------------------------
  client.SendPauseRequest(true);
  CHECK(pumpUntil([&]() { return match->GetPause(); }, 2000), "pause request not applied");
  CHECK(session.GetState() == e_NetMatchPhaseState_Paused, "state should be Paused");

  // --- 3. resume vote: both peers must agree --------------------------------
  {
    NetLobbyAction vote;
    vote.type = e_NetLobbyAction_SetResumeReady;
    vote.value = 1;
    client.SendLobbyAction(vote);
    SleepMs(100);
    tick();
    CHECK(match->GetPause(), "match must stay paused with only one vote");

    vote.playerId = 0; // host
    server->ApplyLobbyAction(vote);
    CHECK(pumpUntil([&]() { return !match->GetPause(); }, 2000), "all-ready vote did not resume");
  }
  CHECK(session.GetState() == e_NetMatchPhaseState_Playing, "state should be Playing after resume");

  // --- 4. side selection open / cancel (while paused) -----------------------
  client.SendPauseRequest(true);
  CHECK(pumpUntil([&]() { return match->GetPause(); }, 2000), "second pause not applied");

  {
    NetLobbyAction action;
    action.type = e_NetLobbyAction_RequestSideSelect;
    action.value = 1;
    client.SendLobbyAction(action);
  }
  CHECK(pumpUntil([&]() { return session.GetState() == e_NetMatchPhaseState_SideSelect; }, 2000),
        "side selection did not open");

  {
    NetLobbyAction action;
    action.type = e_NetLobbyAction_RequestSideSelect;
    action.value = 0; // cancel
    client.SendLobbyAction(action);
  }
  // Cancel applies sides but keeps the match paused (back to the pause menu).
  CHECK(pumpUntil([&]() { return session.GetState() == e_NetMatchPhaseState_Paused; }, 2000),
        "side selection cancel should return to Paused");
  CHECK(match->GetPause(), "cancel must not resume the match");

  // --- 5. resume again, then disconnect ------------------------------------
  {
    NetLobbyAction vote;
    vote.type = e_NetLobbyAction_SetResumeReady;
    vote.value = 1;
    vote.playerId = 0;
    server->ApplyLobbyAction(vote);
    NetLobbyAction clientVote;
    clientVote.type = e_NetLobbyAction_SetResumeReady;
    clientVote.value = 1;
    client.SendLobbyAction(clientVote);
    CHECK(pumpUntil([&]() { return !match->GetPause(); }, 2000), "resume before disconnect failed");
  }

  client.Disconnect();
  // Roster change: host pauses and re-opens side selection for everyone.
  CHECK(pumpUntil([&]() { return session.GetState() == e_NetMatchPhaseState_SideSelect; }, 5000),
        "disconnect did not trigger side selection");
  CHECK(match->GetPause(), "disconnect should pause the match");
  CHECK(server->GetLobbyState().players.size() == 1, "disconnected client should leave the lobby");

  // The host leaves side selection (cancel): applies sides, stays paused.
  {
    NetLobbyAction action;
    action.type = e_NetLobbyAction_RequestSideSelect;
    action.value = 0;
    action.playerId = 0;
    server->ApplyLobbyAction(action);
  }
  CHECK(pumpUntil([&]() { return session.GetState() == e_NetMatchPhaseState_Paused; }, 2000),
        "host cancel after disconnect should return to Paused");
  CHECK(match->GetPause(), "cancel after disconnect must not resume");

  // Only the host remains, so its resume vote completes immediately.
  {
    NetLobbyAction vote;
    vote.type = e_NetLobbyAction_SetResumeReady;
    vote.value = 1;
    vote.playerId = 0;
    server->ApplyLobbyAction(vote);
    CHECK(pumpUntil([&]() { return session.GetState() == e_NetMatchPhaseState_Playing; }, 2000),
          "host-only vote did not resume after disconnect");
  }

  session.StopMatch();
  client.Disconnect();
  server->Stop();

  if (g_failures == 0) {
    std::printf("PASS (%d checks)\n", g_checks);
    ::exit(0);
  }
  std::printf("FAIL (%d/%d checks failed)\n", g_failures, g_checks);
  ::exit(1);
}

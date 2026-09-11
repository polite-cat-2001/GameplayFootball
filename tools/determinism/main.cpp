// Written for the determinism gate: runs the match simulation headless and
// produces a SHA-1 fingerprint of the final state. Two modes:
//   run              -> print hash
//   check <hash>     -> compare with expected, exit code 0/1
//
// The runner intentionally exits via ::exit() after producing the result:
// teardown of the GUI/rendering globals would crash in headless mode, and the
// OS reclaims everything on exit anyway.
//
// Determinism note: EnvState serializes only the data of Vector3/Quaternion
// (see src/utils/envstate.cpp) because those classes carry a vptr (virtual
// destructor); memcpy of the whole object would capture the vtable address,
// which is stable within one binary but changes between rebuilds.

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
#include "utils/capturestate.hpp"

#include "SDL3_ttf/SDL_ttf.h"

#include <string>
#include <iostream>
#include <cstdlib>

#if defined(WIN32) && defined(__MINGW32__)
#undef main
#endif

using namespace blunted;

int main(int argc, char **argv) {
  std::string mode = (argc > 1) ? argv[1] : "run";
  std::string expected = (argc > 2) ? argv[2] : "";

  Properties config;
  config.LoadFile("football.config");
  config.Set("graphics3d_renderer", "mock");  // headless: no window / GL context
  config.Set("audio_renderer", "mock");       // headless: no audio device (CI runners have none)
  // The in-game settings screen writes match_difficulty/match_duration back to
  // football.config on exit, which would shift the simulation hash. Pin the
  // simulation-affecting keys to the code defaults so the reference is stable
  // regardless of the on-disk config state.
  config.Set("match_difficulty", 0.8f);
  config.Set("match_duration", 1.0f);

  Initialize(config);

#ifdef __APPLE__
  // macOS: InitGameContext does not create the graphics/audio systems (they are
  // created on the main thread by src/main.cpp before the scheduler thread
  // starts). The headless runner has no main() render loop, so create the
  // systems explicitly and run the (mock) renderer on a helper thread to drain
  // its message queue; without this the Texture/VertexBuffer/AudioSoundBuffer
  // resource managers never get registered and Match construction fails.
  if (!InitGameSystems(config)) ::exit(1);
  graphicsSystem->GetRenderer3D()->Run();
#endif

  if (!InitGameContext(config)) ::exit(1);

  // GameTask + MenuTask (needed by Match's constructor via GetMenuTask()).
  gameTask = boost::shared_ptr<GameTask>(new GameTask());

  std::string fontFile = config.Get("font_filename", "media/fonts/alegreya/AlegreyaSansSC-ExtraBold.ttf");
  TTF_Font *defaultFont = TTF_OpenFont(fontFile.c_str(), 32);
  if (!defaultFont) { Log(e_FatalError, "determinism", "main", "Could not load font " + fontFile); ::exit(1); }
  TTF_Font *defaultOutlineFont = TTF_OpenFont(fontFile.c_str(), 32);
  TTF_SetFontOutline(defaultOutlineFont, 2);
  menuTask = boost::shared_ptr<MenuTask>(new MenuTask(5.0f / 4.0f, 0, defaultFont, defaultOutlineFont));

  // Register a "game" task sequence so GetScheduler()->GetTaskSequenceInfo("game")
  // returns a valid structure (RegisterTaskSequence requires >= 1 entry).
  boost::shared_ptr<TaskSequence> gameSequence(new TaskSequence("game", 10, false));
  gameSequence->AddUserTaskEntry(menuTask, e_TaskPhase_Get);
  GetScheduler()->RegisterTaskSequence(gameSequence);

  // Create LoadingMatchPage so Match's constructor can close it.
  menuTask->SetTeamIDs("3", "8");  // Release builds do not QuickStart.
  Properties loadingProps;
  menuTask->GetWindowManager()->GetPageFactory()->CreatePage((int)e_PageID_LoadingMatch, loadingProps, 0);

  MatchData *matchData = new MatchData(menuTask->GetTeamID(0), menuTask->GetTeamID(1));
  menuTask->SetMatchData(matchData);

  randomize(42);

  Match *match = new Match(matchData, GetControllers());

  int steps = 5000;
  for (int i = 0; i < steps; i++) {
    match->Process();
  }

  std::string hash = CaptureMatchState(match);
  std::cout << hash << std::endl;
  std::cout.flush();

  if (mode == "check") {
    if (hash == expected) ::exit(0);
    std::cout << "FAIL got=" << hash << " expected=" << expected << std::endl;
    ::exit(1);
  }

  ::exit(0);
}

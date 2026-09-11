// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#ifdef WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <windows.h>
#include <shellapi.h>
#endif

#include <vector>
#include <string>

#include "main.hpp"

#include "gamecontext.hpp"

#include "base/utils.hpp"
#include "base/math/bluntmath.hpp"

#include "scene/scene2d/scene2d.hpp"
#include "scene/scene3d/scene3d.hpp"

#include "managers/resourcemanagerpool.hpp"
#include "utils/objectloader.hpp"
#include "scene/objectfactory.hpp"

#include "systems/audio/audio_system.hpp"

#include "framework/scheduler.hpp"

#include "managers/systemmanager.hpp"
#include "managers/scenemanager.hpp"

#include "base/log.hpp"

#include "types/thread.hpp"
#include "utils/threadhud.hpp"

#include "utils/orbitcamera.hpp"

#include "SDL3_ttf/SDL_ttf.h"

#if defined(WIN32) && defined(__MINGW32__)
#undef main
#endif

using namespace blunted;

boost::shared_ptr<TaskSequence> graphicsSequence;
boost::shared_ptr<TaskSequence> gameSequence;

void InitDebugImage() {
  SDL_Surface *sdlSurface = CreateSDLSurface(200, 150);

  boost::intrusive_ptr < Resource <Surface> > resource = ResourceManagerPool::GetInstance().GetManager<Surface>(e_ResourceType_Surface)->Fetch("debugimage", false, true);
  Surface *surface = resource->GetResource();

  surface->SetData(sdlSurface);

  debugImage = boost::static_pointer_cast<Image2D>(ObjectFactory::GetInstance().CreateObject("debugimage", e_ObjectType_Image2D));
  scene2D->CreateSystemObjects(debugImage);
  debugImage->SetImage(resource);

  int contextW, contextH, bpp; // context
  scene2D->GetContextSize(contextW, contextH, bpp);
  debugImage->SetPosition(contextW - 210, contextH - 160);

  scene2D->AddObject(debugImage);

  debugImage->DrawRectangle(0, 0, 200, 150, Vector3(40, 20, 20), 100);
  debugImage->OnChange();
}

void InitDebugOverlay() {
  int contextW, contextH, bpp; // context
  scene2D->GetContextSize(contextW, contextH, bpp);

  SDL_Surface *sdlSurface = CreateSDLSurface(contextW, contextH);

  boost::intrusive_ptr < Resource <Surface> > resource = ResourceManagerPool::GetInstance().GetManager<Surface>(e_ResourceType_Surface)->Fetch("debugoverlay", false, true);
  Surface *surface = resource->GetResource();

  surface->SetData(sdlSurface);

  debugOverlay = boost::static_pointer_cast<Image2D>(ObjectFactory::GetInstance().CreateObject("debugoverlay", e_ObjectType_Image2D));
  scene2D->CreateSystemObjects(debugOverlay);
  debugOverlay->SetImage(resource);

  debugOverlay->SetPosition(0, 0);

  scene2D->AddObject(debugOverlay);

  debugOverlay->DrawRectangle(0, 0, contextW, contextH, Vector3(0, 0, 0), 0);
  debugOverlay->OnChange();
}

class ThreadHudThread : public Thread {  public:
    ThreadHudThread() {
      hud = new ThreadHud(GetScene2D());
    }
    virtual ~ThreadHudThread() {
      delete hud;
    }

    virtual void operator()() {
      bool quit = false;
      while (!quit) {

        SetState(e_ThreadState_Busy);

        bool isMessage = false;
        boost::intrusive_ptr<Command> message = boost::intrusive_ptr<Command>();
        message = messageQueue.GetMessage(isMessage);
        if (isMessage) {
          if (!message->Handle(this)) quit = true;
          message.reset();
        }

        hud->Execute();

        SetState(e_ThreadState_Idle);

        boost::this_thread::yield();
      }
    }
  protected:
    ThreadHud *hud;

};


static ThreadHudThread *threadHudThread = 0;
static TTF_Font *defaultFont = 0;
static TTF_Font *defaultOutlineFont = 0;

// Everything between the manager initialization and the scheduler run. On
// Apple platforms this runs on a helper thread while the render loop takes
// the main thread; everywhere else it runs on the main thread. On Apple the
// graphics/audio systems were already created on the main thread by
// InitGameSystems() (called from main()), so InitGameContext() skips them.
void RunGame() {
  srand(time(NULL));
  rand(); // mingw32? buggy compiler? first value seems bogus
  randomseed(); // for the boost random
  fastrandomseed();

  int timeStep_ms = config->GetInt("physics_frametime_ms", 10);


  // shared game context (database, systems, scenes, pilons, controllers)

  if (!InitGameContext(*config)) {
    Log(e_FatalError, "football", "main", "Could not initialize game context");
    return;
  }

  if (SuperDebug()) InitDebugImage();
  if (GetDebugMode() == e_DebugMode_AI) InitDebugOverlay();

  threadHudThread = 0;
  if (!IsReleaseVersion() && 1 == 2) {
    threadHudThread = new ThreadHudThread();
    threadHudThread->Run();
  } else {
    threadHudThread = 0;
  }


  // sequences

  boost::mutex graphicsGameMutex; // todo: this mutex seems necessary for visual fluency, doesn't this imply that i'm setting positional stuff during something else than gametask put? (or reading during something else than graphics get)

  gameTask = boost::shared_ptr<GameTask>(new GameTask());

  // TTF_Font *defaultFont = TTF_OpenFont("media/fonts/archivonarrow/ArchivoNarrow-Regular.ttf", 28);
  // TTF_Font *defaultOutlineFont = TTF_OpenFont("media/fonts/archivonarrow/ArchivoNarrow-Regular.ttf", 28);
  std::string fontfilename = config->Get("font_filename", "media/fonts/alegreya/AlegreyaSansSC-ExtraBold.ttf");
  defaultFont = TTF_OpenFont(fontfilename.c_str(), 32);
  if (!defaultFont) Log(e_FatalError, "football", "main", "Could not load font " + fontfilename);
  defaultOutlineFont = TTF_OpenFont(fontfilename.c_str(), 32);
  TTF_SetFontOutline(defaultOutlineFont, 2);
  menuTask = boost::shared_ptr<MenuTask>(new MenuTask(5.0f / 4.0f, 0, defaultFont, defaultOutlineFont));
  if (controllers.size() > 1) menuTask->SetEventJoyButtons(static_cast<HIDGamepad*>(controllers.at(1))->GetControllerMapping(e_ControllerButton_A), static_cast<HIDGamepad*>(controllers.at(1))->GetControllerMapping(e_ControllerButton_B));


  gameSequence = boost::shared_ptr<TaskSequence>(new TaskSequence("game", timeStep_ms, false));

  // note: the whole locking stuff is now happening from within some of the code, iirc, 't is all very ugly and unclear. sorry

  //gameSequence->AddLockEntry(graphicsGameMutex, e_LockAction_Lock);   // ---------- lock -----

  gameSequence->AddUserTaskEntry(menuTask, e_TaskPhase_Get);
  gameSequence->AddUserTaskEntry(menuTask, e_TaskPhase_Process);
  gameSequence->AddUserTaskEntry(menuTask, e_TaskPhase_Put);

  //gameSequence->AddLockEntry(graphicsGameMutex, e_LockAction_Unlock); // ---------- unlock ---

  gameSequence->AddUserTaskEntry(gameTask, e_TaskPhase_Get);
  gameSequence->AddUserTaskEntry(gameTask, e_TaskPhase_Process);

//  gameSequence->AddLockEntry(graphicsGameMutex, e_LockAction_Unlock); // ---------- unlock ---

  GetScheduler()->RegisterTaskSequence(gameSequence);



  graphicsSequence = boost::shared_ptr<TaskSequence>(new TaskSequence("graphics", config->GetInt("graphics3d_frametime_ms", 0), true));

  graphicsSequence->AddUserTaskEntry(gameTask, e_TaskPhase_Put);

  //graphicsSequence->AddLockEntry(graphicsGameMutex, e_LockAction_Lock);   // ---------- lock -----

  graphicsSequence->AddSystemTaskEntry(graphicsSystem, e_TaskPhase_Get);

  //graphicsSequence->AddLockEntry(graphicsGameMutex, e_LockAction_Unlock); // ---------- unlock ---

  graphicsSequence->AddSystemTaskEntry(graphicsSystem, e_TaskPhase_Process);
  graphicsSequence->AddSystemTaskEntry(graphicsSystem, e_TaskPhase_Put);

  GetScheduler()->RegisterTaskSequence(graphicsSequence);


  // fire!

  Run();

#ifdef __APPLE__
  // macOS: the render loop lives on the main thread, so once the scheduler is
  // done, signal it to stop (it will be joined back in main()).
  boost::intrusive_ptr<Message_Shutdown> shutdown(new Message_Shutdown());
  graphicsSystem->GetRenderer3D()->messageQueue.PushMessage(shutdown);
#endif
}


int main(int argc, char** argv) {

  config = new Properties();
  if (argc > 1) configFile = argv[1];
  config->LoadFile(configFile.c_str());

  Initialize(*config);

#ifdef __APPLE__
  // macOS (Cocoa/AppKit) requires window creation, the GL context and SDL
  // event pumping on the main thread. Create the systems (and thus the
  // window/context) here, on the main thread, then run the whole game
  // (context init + scheduler) on a helper thread while the render loop takes
  // the main thread.
  if (!InitGameSystems(*config)) {
    Log(e_FatalError, "football", "main", "Could not initialize game systems");
    return 1;
  }

  boost::thread schedulerThread(&RunGame);
  graphicsSystem->GetRenderer3D()->operator()();
  schedulerThread.join();
#else
  RunGame();
#endif


  // exit

  if (SuperDebug()) scene2D->DeleteObject(debugImage);
  if (GetDebugMode() == e_DebugMode_AI) scene2D->DeleteObject(debugOverlay);

  gameTask.reset();
  menuTask.reset();

  gameSequence.reset();
  graphicsSequence.reset();

  greenPilon->Exit();
  greenPilon.reset();
  bluePilon->Exit();
  bluePilon.reset();
  yellowPilon->Exit();
  yellowPilon.reset();
  redPilon->Exit();
  redPilon.reset();
  smallDebugCircle1->Exit();
  smallDebugCircle1.reset();
  smallDebugCircle2->Exit();
  smallDebugCircle2.reset();
  largeDebugCircle->Exit();
  largeDebugCircle.reset();

  if (threadHudThread) {
    boost::intrusive_ptr<Message_Shutdown> shutdownMessage = new Message_Shutdown();
    threadHudThread->messageQueue.PushMessage(shutdownMessage);
    threadHudThread->Join();
    delete threadHudThread;
    shutdownMessage.reset();
  }

  scene2D.reset();
  scene3D.reset();

  ShutdownGameContext();

  TTF_CloseFont(defaultFont); // todo: better timed closefont?
  TTF_CloseFont(defaultOutlineFont); // todo: better timed closefont?

  delete config;

  Exit();

  return 0;
}

#if defined(WIN32) && !defined(__MINGW32__)
// SDL3 no longer provides a SDLmain shim that maps WinMain to main on
// Windows GUI (WIN32) executables, so provide one here. Command-line
// arguments are reconstructed from the Win32 command line.
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
  int argc = 0;
  LPWSTR *argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
  std::vector<std::string> argvStrings;
  std::vector<char*> argvPtrs;
  for (int i = 0; i < argc; i++) {
    std::wstring warg(argvW[i]);
    std::string arg(warg.begin(), warg.end());
    argvStrings.push_back(arg);
  }
  if (argvW) LocalFree(argvW);
  for (int i = 0; i < argc; i++) argvPtrs.push_back(const_cast<char*>(argvStrings[i].c_str()));
  return main(argc, &argvPtrs[0]);
}
#endif


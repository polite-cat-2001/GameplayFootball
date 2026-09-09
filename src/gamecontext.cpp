// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

// Definitions of the shared game context. See gamecontext.hpp for rationale.

#include "gamecontext.hpp"

#include "base/utils.hpp"
#include "base/math/bluntmath.hpp"

#include "gamedefines.hpp"

#include <fstream>
#include <sstream>

#include "scene/scene2d/scene2d.hpp"
#include "scene/scene3d/scene3d.hpp"

#include "managers/resourcemanagerpool.hpp"
#include "utils/objectloader.hpp"
#include "scene/objectfactory.hpp"

#include "systems/audio/audio_system.hpp"

#include "managers/systemmanager.hpp"
#include "managers/scenemanager.hpp"

#include "base/log.hpp"

#include "hid/keyboard.hpp"
#include "hid/gamepad.hpp"

#include "managers/usereventmanager.hpp"

#include "SDL3/SDL.h"

using namespace blunted;

GraphicsSystem *graphicsSystem;
AudioSystem *audioSystem;

boost::shared_ptr<Scene2D> scene2D;
boost::shared_ptr<Scene3D> scene3D;

boost::shared_ptr<GameTask> gameTask;
boost::shared_ptr<MenuTask> menuTask;

boost::intrusive_ptr<Geometry> greenPilon;
boost::intrusive_ptr<Geometry> bluePilon;
boost::intrusive_ptr<Geometry> yellowPilon;
boost::intrusive_ptr<Geometry> redPilon;

boost::intrusive_ptr<Geometry> smallDebugCircle1;
boost::intrusive_ptr<Geometry> smallDebugCircle2;
boost::intrusive_ptr<Geometry> largeDebugCircle;

Database *db;
Properties *config;

// Minimal JSON field reader for manifest.json: returns the raw value of `key`
// (quotes stripped for strings). Enough for the two fields the game reads; no
// full JSON parser is pulled in. Returns "" when absent or unparseable.
static std::string JsonStringField(const std::string &json, const std::string &key) {
  std::string token = "\"" + key + "\"";
  size_t pos = json.find(token);
  if (pos == std::string::npos) return "";
  pos = json.find(':', pos + token.size());
  if (pos == std::string::npos) return "";
  pos = json.find_first_not_of(" \t\r\n", pos + 1);
  if (pos == std::string::npos) return "";
  if (json[pos] == '"') {
    size_t end = json.find('"', pos + 1);
    return (end == std::string::npos) ? "" : json.substr(pos + 1, end - pos - 1);
  }
  size_t end = json.find_first_of(",\r\n}", pos);
  if (end == std::string::npos) end = json.size();
  std::string value = json.substr(pos, end - pos);
  while (!value.empty() && (value[value.size() - 1] == ' ' || value[value.size() - 1] == '\t')) {
    value.erase(value.size() - 1);
  }
  return value;
}

boost::intrusive_ptr<Image2D> debugImage;
boost::intrusive_ptr<Image2D> debugOverlay;

std::vector<IHIDevice*> controllers;

bool superDebug = false;
e_DebugMode debugMode = e_DebugMode_Off;

std::string activeSaveDirectory;

std::string configFile = "football.config";
std::string GetConfigFilename() {
  return configFile;
}

boost::shared_ptr<Scene2D> GetScene2D() {
  return scene2D;
}

boost::shared_ptr<Scene3D> GetScene3D() {
  return scene3D;
}

GraphicsSystem *GetGraphicsSystem() {
  return graphicsSystem;
}

boost::shared_ptr<GameTask> GetGameTask() {
  return gameTask;
}

boost::shared_ptr<MenuTask> GetMenuTask() {
  return menuTask;
}

Database *GetDB() {
  return db;
}

Properties *GetConfiguration() {
  return config;
}

std::string GetActiveSaveDirectory() {
  return activeSaveDirectory;
}

void SetActiveSaveDirectory(const std::string &dir) {
  activeSaveDirectory = dir;
}

bool IsReleaseVersion() {
  if (GetConfiguration()->GetBool("debug", false)) return false; else return true;
}

bool Verbose() {
  return !IsReleaseVersion();
}

bool UpdateNonImportableDB() {
  if (IsReleaseVersion()) return false;
  else return true;
}

bool SuperDebug() {
  return superDebug;
}

e_DebugMode GetDebugMode() {
  return debugMode;
}

boost::intrusive_ptr<Image2D> GetDebugImage() {
  return debugImage;
}

boost::intrusive_ptr<Image2D> GetDebugOverlay() {
  return debugOverlay;
}

void GetDebugOverlayCoord(Match *match, const Vector3 &worldPos, int &x, int &y) {
  Vector3 proj = GetProjectedCoord(worldPos, match->GetCamera());
  int dud1, dud2;
  GetMenuTask()->GetWindowManager()->GetCoordinates(proj.coords[0], proj.coords[1], 1, 1, x, y, dud1, dud2);

  int contextW, contextH, bpp;
  GetScene2D()->GetContextSize(contextW, contextH, bpp);
  x = clamp(x, 0, contextW - 1);
  y = clamp(y, 0, contextH - 1);
}

int PredictFrameTimeToGo_ms(int frameCount) {
  int averageFrameTime_ms = GetGraphicsSystem()->GetAverageFrameTime_ms(frameCount);
  int timeSinceLastSwap_ms = GetGraphicsSystem()->GetTimeSinceLastSwap_ms();
  int timeToNextSwapPrediction_ms = averageFrameTime_ms - timeSinceLastSwap_ms;
  timeToNextSwapPrediction_ms = clamp(timeToNextSwapPrediction_ms, 0, 1000);
  return timeToNextSwapPrediction_ms;
}

const std::vector<IHIDevice*> &GetControllers() {
  return controllers;
}

void SetGreenDebugPilon(const Vector3 &pos) { greenPilon->SetPosition(pos, false); }
void SetBlueDebugPilon(const Vector3 &pos) { bluePilon->SetPosition(pos, false); }
void SetYellowDebugPilon(const Vector3 &pos) { yellowPilon->SetPosition(pos, false); }
void SetRedDebugPilon(const Vector3 &pos) { redPilon->SetPosition(pos, false); }

void SetSmallDebugCircle1(const Vector3 &pos) { smallDebugCircle1->SetPosition(pos, false); }
void SetSmallDebugCircle2(const Vector3 &pos) { smallDebugCircle2->SetPosition(pos, false); }
void SetLargeDebugCircle(const Vector3 &pos) { largeDebugCircle->SetPosition(pos, false); }

boost::intrusive_ptr<Geometry> GetGreenDebugPilon() { return greenPilon; }
boost::intrusive_ptr<Geometry> GetBlueDebugPilon() { return bluePilon; }
boost::intrusive_ptr<Geometry> GetYellowDebugPilon() { return yellowPilon; }
boost::intrusive_ptr<Geometry> GetRedDebugPilon() { return redPilon; }

boost::intrusive_ptr<Geometry> GetSmallDebugCircle1() { return smallDebugCircle1; }
boost::intrusive_ptr<Geometry> GetSmallDebugCircle2() { return smallDebugCircle2; }
boost::intrusive_ptr<Geometry> GetLargeDebugCircle() { return largeDebugCircle; }

bool InitGameSystems(Properties &cfg) {
  config = &cfg;

  SystemManager *systemManager = SystemManager::GetInstancePtr();
  graphicsSystem = new GraphicsSystem();
  bool ok = systemManager->RegisterSystem("GraphicsSystem", graphicsSystem);
  if (!ok) { Log(e_FatalError, "gamecontext", "InitGameSystems", "Could not register GraphicsSystem"); return false; }
  audioSystem = new AudioSystem();
  ok = systemManager->RegisterSystem("AudioSystem", audioSystem);
  if (!ok) { Log(e_FatalError, "gamecontext", "InitGameSystems", "Could not register AudioSystem"); return false; }

  graphicsSystem->Initialize(*config);
  audioSystem->Initialize(*config);

  return true;
}

bool InitGameContext(Properties &cfg) {
  config = &cfg;

  db = new Database();
  bool dbSuccess = db->Load("databases/default/database.sqlite");
  if (!dbSuccess) { Log(e_FatalError, "gamecontext", "InitGameContext", "Could not open database"); return false; }

  // Schema version gate: reject an incompatible database with a clear error
  // instead of failing mid-match. user_version == 0 means legacy data without
  // any schema marker.
  {
    DatabaseResult *versionResult = db->Query("PRAGMA user_version");
    int dbSchemaVersion = 0;
    if (versionResult && !versionResult->data.empty()) {
      dbSchemaVersion = atoi(versionResult->data.at(0).at(0).c_str());
    }
    if (dbSchemaVersion != databaseSchemaVersion) {
      Log(e_FatalError, "gamecontext", "InitGameContext",
          "Database schema version mismatch: found " + int_to_str(dbSchemaVersion) +
          ", expected " + int_to_str(databaseSchemaVersion) +
          ". Re-run the data import (tm-gf-import) or update the game data.");
      return false;
    }
  }

  // Log the data version from manifest.json; its schema_version must agree
  // with the database's user_version (a mismatch means the DB file was swapped
  // or modified behind the manifest's back).
  {
    std::ifstream manifestFile("databases/default/manifest.json");
    if (manifestFile.is_open()) {
      std::stringstream manifestStream;
      manifestStream << manifestFile.rdbuf();
      manifestFile.close();
      std::string manifest = manifestStream.str();
      std::string schemaVersionStr = JsonStringField(manifest, "schema_version");
      std::string dataVersion = JsonStringField(manifest, "data_version");
      if (!schemaVersionStr.empty() && atoi(schemaVersionStr.c_str()) != databaseSchemaVersion) {
        Log(e_FatalError, "gamecontext", "InitGameContext",
            "manifest.json schema_version does not match the database: " + schemaVersionStr +
            " != " + int_to_str(databaseSchemaVersion));
        return false;
      }
      Log(e_Notice, "gamecontext", "InitGameContext",
          "Game data: schema v" + int_to_str(databaseSchemaVersion) +
          ", data version " + (dataVersion.empty() ? "unknown" : dataVersion));
    } else {
      Log(e_Notice, "gamecontext", "InitGameContext",
          "Game data: schema v" + int_to_str(databaseSchemaVersion) + ", no manifest.json found");
    }
  }

#ifndef __APPLE__
  // macOS creates the graphics/audio systems (and the GL window) on the main
  // thread before the scheduler thread starts; see src/main.cpp. All other
  // platforms create them here, inside the game context.
  if (!InitGameSystems(cfg)) return false;
#endif

  scene2D = boost::shared_ptr<Scene2D>(new Scene2D("scene2D", *config));
  SceneManager::GetInstance().RegisterScene(scene2D);

  scene3D = boost::shared_ptr<Scene3D>(new Scene3D("scene3D"));
  SceneManager::GetInstance().RegisterScene(scene3D);

  // debug pilons

  boost::intrusive_ptr<Resource<GeometryData> > geometry = ResourceManagerPool::GetInstance().GetManager<GeometryData>(e_ResourceType_GeometryData)->Fetch("media/objects/helpers/green.ase", true);
  greenPilon = static_pointer_cast<Geometry>(ObjectFactory::GetInstance().CreateObject("greenPilon", e_ObjectType_Geometry));
  scene3D->CreateSystemObjects(greenPilon);
  greenPilon->SetGeometryData(geometry);
  greenPilon->SetLocalMode(e_LocalMode_Absolute);
  greenPilon->SetPosition(Vector3(0, 0, -10));

  geometry = ResourceManagerPool::GetInstance().GetManager<GeometryData>(e_ResourceType_GeometryData)->Fetch("media/objects/helpers/blue.ase", true);
  bluePilon = static_pointer_cast<Geometry>(ObjectFactory::GetInstance().CreateObject("bluePilon", e_ObjectType_Geometry));
  scene3D->CreateSystemObjects(bluePilon);
  bluePilon->SetGeometryData(geometry);
  bluePilon->SetLocalMode(e_LocalMode_Absolute);
  bluePilon->SetPosition(Vector3(0, 0, -10));

  geometry = ResourceManagerPool::GetInstance().GetManager<GeometryData>(e_ResourceType_GeometryData)->Fetch("media/objects/helpers/yellow.ase", true);
  yellowPilon = static_pointer_cast<Geometry>(ObjectFactory::GetInstance().CreateObject("yellowPilon", e_ObjectType_Geometry));
  scene3D->CreateSystemObjects(yellowPilon);
  yellowPilon->SetGeometryData(geometry);
  yellowPilon->SetLocalMode(e_LocalMode_Absolute);
  yellowPilon->SetPosition(Vector3(0, 0, -10));

  geometry = ResourceManagerPool::GetInstance().GetManager<GeometryData>(e_ResourceType_GeometryData)->Fetch("media/objects/helpers/red.ase", true);
  redPilon = static_pointer_cast<Geometry>(ObjectFactory::GetInstance().CreateObject("redPilon", e_ObjectType_Geometry));
  scene3D->CreateSystemObjects(redPilon);
  redPilon->SetGeometryData(geometry);
  redPilon->SetLocalMode(e_LocalMode_Absolute);
  redPilon->SetPosition(Vector3(0, 0, -10));

  geometry = ResourceManagerPool::GetInstance().GetManager<GeometryData>(e_ResourceType_GeometryData)->Fetch("media/objects/helpers/smalldebugcircle.ase", true);
  smallDebugCircle1 = static_pointer_cast<Geometry>(ObjectFactory::GetInstance().CreateObject("smallDebugCircle1", e_ObjectType_Geometry));
  scene3D->CreateSystemObjects(smallDebugCircle1);
  smallDebugCircle1->SetGeometryData(geometry);
  smallDebugCircle1->SetLocalMode(e_LocalMode_Absolute);
  smallDebugCircle1->SetPosition(Vector3(0, 0, -10));

  geometry = ResourceManagerPool::GetInstance().GetManager<GeometryData>(e_ResourceType_GeometryData)->Fetch("media/objects/helpers/smalldebugcircle.ase", true);
  smallDebugCircle2 = static_pointer_cast<Geometry>(ObjectFactory::GetInstance().CreateObject("smallDebugCircle2", e_ObjectType_Geometry));
  scene3D->CreateSystemObjects(smallDebugCircle2);
  smallDebugCircle2->SetGeometryData(geometry);
  smallDebugCircle2->SetLocalMode(e_LocalMode_Absolute);
  smallDebugCircle2->SetPosition(Vector3(0, 0, -10));

  geometry = ResourceManagerPool::GetInstance().GetManager<GeometryData>(e_ResourceType_GeometryData)->Fetch("media/objects/helpers/largedebugcircle.ase", true);
  largeDebugCircle = static_pointer_cast<Geometry>(ObjectFactory::GetInstance().CreateObject("largeDebugCircle", e_ObjectType_Geometry));
  scene3D->CreateSystemObjects(largeDebugCircle);
  largeDebugCircle->SetGeometryData(geometry);
  largeDebugCircle->SetLocalMode(e_LocalMode_Absolute);
  largeDebugCircle->SetPosition(Vector3(0, 0, -10));

  geometry.reset();

  // controllers (keyboard always present, gamepads rescanned dynamically)
  controllers.clear();
  controllers.push_back(new HIDKeyboard());
  RefreshGamepads();

  return true;
}

bool RefreshGamepads() {
  // called from the game thread (GameTask::ProcessPhase)
  bool changed = false;
  int count = UserEventManager::GetInstance().GetJoystickCount();
  // remove gamepads that are gone
  for (int i = (int)controllers.size() - 1; i >= 1; i--) {
    HIDGamepad *pad = static_cast<HIDGamepad*>(controllers.at(i));
    bool stillThere = false;
    for (int j = 0; j < count; j++) {
      if (UserEventManager::GetInstance().GetJoystickID(j) == pad->GetJoystickID()) { stillThere = true; break; }
    }
    if (!stillThere) {
      delete controllers.at(i);
      controllers.erase(controllers.begin() + i);
      changed = true;
    }
  }
  // add newly connected gamepads (keep ordering by slot)
  int existing = (int)controllers.size() - 1;
  for (int j = existing; j < count; j++) {
    controllers.push_back(new HIDGamepad(j));
    changed = true;
  }
  // if an existing gamepad changed slot, re-map it (recreate to keep gamepadID == slot)
  // note: controllers[0] is the keyboard, so controllers[i] holds gamepad slot i-1
  for (unsigned int i = 1; i < controllers.size(); i++) {
    HIDGamepad *pad = static_cast<HIDGamepad*>(controllers.at(i));
    int slot = UserEventManager::GetInstance().GetSlotForJoystickID(pad->GetJoystickID());
    if (slot != (signed int)(i - 1)) {
      // slot changed: recreate so gamepadID matches slot
      delete controllers.at(i);
      controllers.at(i) = new HIDGamepad(slot);
      changed = true;
    }
  }
  return changed;
}

void ShutdownGameContext() {
  for (unsigned int i = 0; i < controllers.size(); i++) {
    delete controllers.at(i);
  }
  controllers.clear();

  if (config) delete config; config = NULL;
  if (db) delete db; db = NULL;
}

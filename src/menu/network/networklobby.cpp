#include "networklobby.hpp"

#include "network.hpp"

#include "../pagefactory.hpp"

#include "../../main.hpp"

#include "utils/gui2/events.hpp"

#include "net/netclient.hpp"
#include "net/netserver.hpp"

#include <SDL3/SDL.h>

NetworkLobbyPage::NetworkLobbyPage(Gui2WindowManager *windowManager, const Gui2PageData &pageData) : Gui2Page(windowManager, pageData) {

  titleCaption = new Gui2Caption(windowManager, "caption_network_lobby_title", 20, 8, 60, 3, "LAN Lobby");
  this->AddView(titleCaption);
  titleCaption->Show();

  phaseCaption = new Gui2Caption(windowManager, "caption_network_lobby_phase", 20, 14, 60, 3, "");
  this->AddView(phaseCaption);
  phaseCaption->Show();

  teamsCaption = new Gui2Caption(windowManager, "caption_network_lobby_teams", 20, 19, 60, 3, "");
  this->AddView(teamsCaption);
  teamsCaption->Show();

  cursorCaption = new Gui2Caption(windowManager, "caption_network_lobby_cursor", 20, 24, 60, 3, "");
  this->AddView(cursorCaption);
  cursorCaption->Show();

  for (int i = 0; i < net_maxPlayers; i++) {
    Gui2Caption *caption = new Gui2Caption(windowManager, "caption_network_lobby_player" + int_to_str(i), 20, 32 + i * 5, 60, 3, "");
    this->AddView(caption);
    caption->Hide();
    playerCaptions.push_back(caption);
  }

  helpCaption = new Gui2Caption(windowManager, "caption_network_lobby_help", 20, 60, 60, 3, "");
  this->AddView(helpCaption);
  helpCaption->Show();

  this->SetFocus();
  this->Show();
}

NetworkLobbyPage::~NetworkLobbyPage() {
}

bool NetworkLobbyPage::IsHost() {
  return GetMenuTask()->GetNetServer() != 0;
}

NetLobbyState NetworkLobbyPage::GetState() {
  if (IsHost()) {
    boost::shared_ptr<NetServer> server = GetMenuTask()->GetNetServer();
    if (server) return server->GetLobbyState();
  } else {
    boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
    if (client) return client->GetLobbyState();
  }
  return NetLobbyState();
}

std::vector<NetCatalogEntry> NetworkLobbyPage::GetCatalog() {
  if (IsHost()) {
    boost::shared_ptr<NetServer> server = GetMenuTask()->GetNetServer();
    if (server) return server->GetCatalog();
  } else {
    boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
    if (client) return client->GetCatalog();
  }
  return std::vector<NetCatalogEntry>();
}

uint32_t NetworkLobbyPage::GetLocalPlayerId() {
  if (IsHost()) return 0;
  boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
  if (client) return client->GetPlayerId();
  return 0;
}

int NetworkLobbyPage::GetChooserSide(uint32_t playerId) {
  NetLobbyState state = GetState();
  for (int side = 0; side < 2; side++) {
    if (state.chooser[side] == playerId) return side;
  }
  return -1;
}

void NetworkLobbyPage::SendAction(int type, int side, int value) {
  NetLobbyAction action;
  action.type = type;
  action.side = side;
  action.value = value;

  if (IsHost()) {
    boost::shared_ptr<NetServer> server = GetMenuTask()->GetNetServer();
    if (server) { action.playerId = 0; server->ApplyLobbyAction(action); }
  } else {
    boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
    if (client) client->SendLobbyAction(action);
  }
}

std::string NetworkLobbyPage::SideName(int side) {
  if (side == e_NetSide_Home) return "HOME";
  if (side == e_NetSide_Away) return "AWAY";
  return "SPECTATOR";
}

std::string NetworkLobbyPage::TeamName(int teamId) {
  if (teamId < 0) return "-";
  std::vector<NetCatalogEntry> catalog = GetCatalog();
  for (unsigned int i = 0; i < catalog.size(); i++) {
    if (catalog.at(i).id == teamId) return catalog.at(i).name;
  }
  return "?";
}

void NetworkLobbyPage::Process() {
  Gui2View::Process();

  NetLobbyState state = GetState();
  uint32_t localId = GetLocalPlayerId();

  phaseCaption->SetCaption(std::string("Phase: ") + (state.phase == e_NetLobbyPhase_Sides ? "choose sides" : "choose teams"));
  teamsCaption->SetCaption("Home: " + TeamName(state.teamId[0]) + "    Away: " + TeamName(state.teamId[1]));

  for (int i = 0; i < net_maxPlayers; i++) {
    if (i < (int)state.players.size()) {
      const NetLobbyPlayer &player = state.players.at(i);
      std::string line;
      if (player.id == localId) line += "> "; else line += "  ";
      line += player.name;
      line += "  [" + SideName(player.side) + "]";
      if (player.isHost) line += "  host";
      if (player.ready) line += "  READY";
      playerCaptions.at(i)->SetCaption(line);
      playerCaptions.at(i)->Show();
    } else {
      playerCaptions.at(i)->Hide();
    }
  }

  int chooserSide = GetChooserSide(localId);
  if (state.phase == e_NetLobbyPhase_Teams && chooserSide >= 0) {
    std::vector<NetCatalogEntry> catalog = GetCatalog();
    int cursor = state.teamCursor[chooserSide];
    if (cursor < 0) cursor = 0;
    if (cursor >= (int)catalog.size()) cursor = catalog.size() > 0 ? (int)catalog.size() - 1 : 0;
    std::string name = catalog.empty() ? "-" : catalog.at(cursor).name;
    cursorCaption->SetCaption("Choosing " + SideName(chooserSide) + ": " + name);
  } else {
    cursorCaption->SetCaption("");
  }

  if (state.phase == e_NetLobbyPhase_Sides) {
    helpCaption->SetCaption("Left/Right: side   Enter: ready   Esc: leave");
  } else {
    helpCaption->SetCaption("Left/Right: team   Enter: confirm   Esc: leave");
  }
}

void NetworkLobbyPage::ProcessKeyboardEvent(KeyboardEvent *event) {
  NetLobbyState state = GetState();
  uint32_t localId = GetLocalPlayerId();

  const NetLobbyPlayer *local = 0;
  for (unsigned int i = 0; i < state.players.size(); i++) {
    if (state.players.at(i).id == localId) { local = &state.players.at(i); break; }
  }

  if (event->GetKeyOnce(SDLK_ESCAPE)) {
    Leave();
    return;
  }

  int chooserSide = GetChooserSide(localId);
  std::vector<NetCatalogEntry> catalog = GetCatalog();

  if (state.phase == e_NetLobbyPhase_Sides) {
    if (event->GetKeyOnce(SDLK_LEFT)) {
      int side = local ? local->side : e_NetSide_Spectator;
      side -= 1;
      if (side < 0) side = 2;
      SendAction(e_NetLobbyAction_SetSide, side, 0);
    } else if (event->GetKeyOnce(SDLK_RIGHT)) {
      int side = local ? local->side : e_NetSide_Spectator;
      side += 1;
      if (side > 2) side = 0;
      SendAction(e_NetLobbyAction_SetSide, side, 0);
    } else if (event->GetKeyOnce(SDLK_RETURN)) {
      bool ready = local ? local->ready : false;
      SendAction(e_NetLobbyAction_SetReady, 0, ready ? 0 : 1);
    }
  } else {
    if (chooserSide >= 0) {
      int cursor = state.teamCursor[chooserSide];
      if (event->GetKeyOnce(SDLK_LEFT)) {
        if (cursor > 0) cursor--;
        SendAction(e_NetLobbyAction_MoveCursor, chooserSide, cursor);
      } else if (event->GetKeyOnce(SDLK_RIGHT)) {
        if (cursor < (int)catalog.size() - 1) cursor++;
        SendAction(e_NetLobbyAction_MoveCursor, chooserSide, cursor);
      } else if (event->GetKeyOnce(SDLK_RETURN)) {
        if (cursor >= 0 && cursor < (int)catalog.size()) {
          SendAction(e_NetLobbyAction_CommitTeam, chooserSide, catalog.at(cursor).id);
        }
      }
    }
  }
}

void NetworkLobbyPage::ProcessWindowingEvent(WindowingEvent *event) {
  event->Ignore();
}

void NetworkLobbyPage::Leave() {
  if (IsHost()) {
    boost::shared_ptr<NetServer> server = GetMenuTask()->GetNetServer();
    if (server) server->Stop();
    GetMenuTask()->SetNetServer(boost::shared_ptr<NetServer>());
  } else {
    boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
    if (client) client->Disconnect();
    GetMenuTask()->SetNetClient(boost::shared_ptr<NetClient>());
  }
  CreatePage(e_PageID_NetworkMenu);
}

#ifndef _HPP_NETMESSAGES
#define _HPP_NETMESSAGES

#include <cstdint>
#include <string>
#include <vector>

#include "netbuffer.hpp"

enum e_NetRejectReason {
  e_NetReject_None = 0,
  e_NetReject_ProtocolMismatch,
  e_NetReject_BuildMismatch,
  e_NetReject_DataVersionMismatch,
  e_NetReject_DataHashMismatch,
  e_NetReject_AnimationMismatch,
  e_NetReject_LobbyFull,
  e_NetReject_Unknown
};

struct NetClientHello {
  NetClientHello() : protocolVersion(0), sessionId(0) {}

  int protocolVersion;
  std::string buildHash;
  std::string dataVersion;
  std::string dataHash;
  std::string animationHash;
  std::string playerName;
  uint32_t sessionId;
};

struct NetServerHello {
  NetServerHello() : accepted(false), reason(e_NetReject_None), sessionId(0) {}

  bool accepted;
  e_NetRejectReason reason;
  std::string reasonText;
  uint32_t sessionId;
};

void WriteClientHello(NetBuffer &buffer, const NetClientHello &hello);
NetClientHello ReadClientHello(NetBuffer &buffer);
void WriteServerHello(NetBuffer &buffer, const NetServerHello &hello);
NetServerHello ReadServerHello(NetBuffer &buffer);

enum e_NetLobbyPhase {
  e_NetLobbyPhase_Sides = 0,
  e_NetLobbyPhase_Teams
};

enum e_NetSide {
  e_NetSide_Home = 0,
  e_NetSide_Away = 1,
  e_NetSide_Spectator = 2
};

enum e_NetLobbyActionType {
  e_NetLobbyAction_SetSide = 0,
  e_NetLobbyAction_SetReady,
  e_NetLobbyAction_MoveCursor,
  e_NetLobbyAction_CommitTeam
};

struct NetLobbyPlayer {
  NetLobbyPlayer() : id(0), side(e_NetSide_Spectator), ready(false), isHost(false), ping_ms(0) {}

  uint32_t id;
  std::string name;
  int side;
  bool ready;
  bool isHost;
  int ping_ms;
};

// Canonical, host-authoritative lobby state. Mirrored to every peer so that side
// changes and the team cursor are visible live on all devices.
struct NetLobbyState {
  NetLobbyState() : revision(0), phase(e_NetLobbyPhase_Sides) {
    teamId[0] = -1;
    teamId[1] = -1;
    chooser[0] = 0;
    chooser[1] = 0;
    teamCursor[0] = 0;
    teamCursor[1] = 0;
    listScroll[0] = 0;
    listScroll[1] = 0;
  }

  uint32_t revision;
  int phase;
  std::vector<NetLobbyPlayer> players;
  int teamId[2];
  uint32_t chooser[2];
  int teamCursor[2];
  int listScroll[2];
};

// Client intent. The server attributes it to the connection (playerId is filled in
// by the server) and applies it to the canonical state.
struct NetLobbyAction {
  NetLobbyAction() : type(0), playerId(0), side(0), value(0) {}

  int type;
  uint32_t playerId;
  int side;
  int value;
};

void WriteLobbyState(NetBuffer &buffer, const NetLobbyState &state);
NetLobbyState ReadLobbyState(NetBuffer &buffer);
void WriteLobbyAction(NetBuffer &buffer, const NetLobbyAction &action);
NetLobbyAction ReadLobbyAction(NetBuffer &buffer);

#endif


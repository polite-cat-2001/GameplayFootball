#ifndef _HPP_NETMESSAGES
#define _HPP_NETMESSAGES

#include <cstdint>
#include <string>
#include <vector>

#include "netbuffer.hpp"
#include "nettypes.hpp"

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
  e_NetLobbyAction_CommitTeam,
  e_NetLobbyAction_SetSelection,
  e_NetLobbyAction_SetTeamReady,
  e_NetLobbyAction_SetDevice,
  e_NetLobbyAction_DeviceLost,
  e_NetLobbyAction_RequestSideSelect,
  e_NetLobbyAction_SetResumeReady
};

struct NetLobbyPlayer {
  NetLobbyPlayer() : id(0), side(e_NetSide_Spectator), ready(false), isHost(false), ping_ms(0), device(0), resumeReady(false) {}

  uint32_t id;
  std::string name;
  int side;
  bool ready;
  bool isHost;
  int ping_ms;
  int device; // 0 = keyboard, 1 = gamepad (last device used by this peer)
  bool resumeReady; // voted to leave the in-match pause menu
};

// Canonical, host-authoritative lobby state. Mirrored to every peer so that side
// changes and the team cursor are visible live on all devices.
struct NetLobbyState {
  NetLobbyState() : revision(0), phase(e_NetLobbyPhase_Sides), sideSelect(false) {
    teamId[0] = -1;
    teamId[1] = -1;
    countryId[0] = -1;
    countryId[1] = -1;
    leagueId[0] = -1;
    leagueId[1] = -1;
    teamReady[0] = false;
    teamReady[1] = false;
    chooser[0] = 0;
    chooser[1] = 0;
    teamCursor[0] = 0;
    teamCursor[1] = 0;
    listScroll[0] = 0;
    listScroll[1] = 0;
  }

  uint32_t revision;
  int phase;
  // In-match side selection: while true the lobby stays in the Sides phase and
  // never advances to team selection; all-ready means "resume the match".
  bool sideSelect;
  std::vector<NetLobbyPlayer> players;
  int teamId[2];
  int countryId[2];
  int leagueId[2];
  bool teamReady[2];
  uint32_t chooser[2];
  int teamCursor[2];
  int listScroll[2];
};

// Client intent. The server attributes it to the connection (playerId is filled in
// by the server) and applies it to the canonical state.
struct NetLobbyAction {
  NetLobbyAction() : type(0), playerId(0), side(0), value(0), value2(0) {}

  int type;
  uint32_t playerId;
  int side;
  int value;
  int value2;
};

void WriteLobbyState(NetBuffer &buffer, const NetLobbyState &state);
NetLobbyState ReadLobbyState(NetBuffer &buffer);
void WriteLobbyAction(NetBuffer &buffer, const NetLobbyAction &action);
NetLobbyAction ReadLobbyAction(NetBuffer &buffer);

struct NetCatalogEntry {
  int id = 0;
  std::string name;
  std::string shortName;
};

void WriteCatalog(NetBuffer &buffer, const std::vector<NetCatalogEntry> &catalog);
std::vector<NetCatalogEntry> ReadCatalog(NetBuffer &buffer);

// Sent by the host when the lobby is done: the client builds the same Match from
// these team ids (v1 relies on the identical database enforced by the handshake).
struct NetMatchSetup {
  NetMatchSetup() { teamId[0] = -1; teamId[1] = -1; }

  int teamId[2];
};

void WriteMatchSetup(NetBuffer &buffer, const NetMatchSetup &setup);
NetMatchSetup ReadMatchSetup(NetBuffer &buffer);

// Host animation collection listing, in host index order. The client resolves
// each name to its own Animation* so snapshots can carry a compact animID.
void WriteAnimationTable(NetBuffer &buffer, const std::vector<std::string> &names);
std::vector<std::string> ReadAnimationTable(NetBuffer &buffer);

// Client -> host per-tick input: button bitmask (e_ButtonFunction) + direction.
void WriteInputFrame(NetBuffer &buffer, const NetInputFrame &frame);
NetInputFrame ReadInputFrame(NetBuffer &buffer);

// Visual match environment (sun light) so lighting matches on the thin client.
struct NetMatchEnvironment {
  NetMatchEnvironment() : homeKit(-1), awayKit(-1) {}
  blunted::Vector3 sunPosition;
  blunted::Vector3 sunColor;
  int homeKit; // current kit number per side; -1 = leave as-is
  int awayKit;
};

void WriteMatchEnvironment(NetBuffer &buffer, const NetMatchEnvironment &environment);
NetMatchEnvironment ReadMatchEnvironment(NetBuffer &buffer);

// Keepalive / ping. `echo` carries the last received sequence so the peer can
// measure round-trip time; all input-delay (fairness) timing is derived from it.
struct NetKeepalive {
  NetKeepalive() : seq(0), echo(0) {}

  uint32_t seq;
  uint32_t echo;
};

void WriteKeepalive(NetBuffer &buffer, const NetKeepalive &keepalive);
NetKeepalive ReadKeepalive(NetBuffer &buffer);

#endif



#ifndef _HPP_NETTYPES
#define _HPP_NETTYPES

#include <cstdint>
#include <string>

#include "base/math/vector3.hpp"

const int net_protocolVersion = 1;
const uint16_t net_defaultPort = 27015;
const int net_maxPlayers = 4;
const int net_maxHumansPerTeam = 2;
const int net_keepaliveInterval_ms = 500;
const int net_disconnectTimeout_ms = 5000;
const int net_snapshotRate_hz = 100;
const int net_inputRate_hz = 100;

struct NetAddress {
  NetAddress() : port(0) {}
  NetAddress(const std::string &ip, uint16_t port) : ip(ip), port(port) {}

  std::string ip;
  uint16_t port;
};

enum e_NetMessageType {
  e_NetMessage_ClientHello = 1,
  e_NetMessage_ServerHello,
  e_NetMessage_Catalog,
  e_NetMessage_LobbyAction,
  e_NetMessage_LobbyState,
  e_NetMessage_MatchSetup,
  e_NetMessage_AnimationTable,
  e_NetMessage_MatchEnvironment,
  e_NetMessage_SetupAck,
  e_NetMessage_InputFrame,
  e_NetMessage_Snapshot,
  e_NetMessage_ReliableEvent,
  e_NetMessage_PauseRequest,
  e_NetMessage_PauseState,
  e_NetMessage_ReplayStop,
  e_NetMessage_Keepalive
};

enum e_NetConnectionState {
  e_NetConnectionState_Disconnected,
  e_NetConnectionState_Connecting,
  e_NetConnectionState_Handshaking,
  e_NetConnectionState_Connected
};

struct NetInputFrame {
  NetInputFrame() : buttons(0) {}

  uint32_t buttons;
  blunted::Vector3 direction;
};

#endif

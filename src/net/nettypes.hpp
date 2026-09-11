#ifndef _HPP_NETTYPES
#define _HPP_NETTYPES

#include <cstdint>
#include <string>
#include <vector>

#include "base/math/vector3.hpp"

// Bump on any wire-format / message-semantics change. Both peers are already
// gated by the build hash, but this makes the protocol revision explicit.
// v2: LobbyState sideSelect/resumeReady, MatchEnvironment kits, Snapshot maxRtt,
//     NetKeepalive, NetMatchSession pause/side-select semantics.
// v3: UDP realtime channel (input/snapshot) + client snapshot interpolation.
// v4: per-player animation blend state (smooth/smoothFactor) in snapshots.
const int net_protocolVersion = 4;
const uint16_t net_defaultPort = 27015;
const int net_maxPlayers = 4;
const int net_maxHumansPerTeam = 2;
const int net_keepaliveInterval_ms = 500;
const int net_disconnectTimeout_ms = 5000;
const int net_snapshotRate_hz = 100;
const int net_inputRate_hz = 100;
// Interpolation buffer B: the client renders this far behind the newest
// snapshot so a late/lost datagram can be smoothed over. 0 = off: the client
// holds the newest snapshot ("hold last", like the old TCP path) and packet
// loss costs at most one tick. >0 blends two snapshots at (now - B); host then
// carries +B in its input delay (client does not, it already renders B behind).
const int net_interpolationBuffer_ms = 40;

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

// A snapshot datagram as received by the client: raw payload bytes plus the
// host's own snapshot time (the fixed-rate timeline used for interpolation) and
// the client-steady-clock arrival time (diagnostics / buffer warm-up).
struct NetRawSnapshot {
  NetRawSnapshot() : recvTime_ms(0), hostTime_ms(0) {}
  NetRawSnapshot(unsigned long recvTime_ms, unsigned long hostTime_ms, const std::vector<uint8_t> &bytes)
      : recvTime_ms(recvTime_ms), hostTime_ms(hostTime_ms), bytes(bytes) {}

  unsigned long recvTime_ms;
  unsigned long hostTime_ms;
  std::vector<uint8_t> bytes;
};

#endif

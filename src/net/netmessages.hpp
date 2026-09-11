#ifndef _HPP_NETMESSAGES
#define _HPP_NETMESSAGES

#include <cstdint>
#include <string>

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

#endif

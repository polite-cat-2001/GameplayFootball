#include "netmessages.hpp"

void WriteClientHello(NetBuffer &buffer, const NetClientHello &hello) {
  buffer.PutU32((uint32_t)hello.protocolVersion);
  buffer.PutString(hello.buildHash);
  buffer.PutString(hello.dataVersion);
  buffer.PutString(hello.dataHash);
  buffer.PutString(hello.animationHash);
  buffer.PutString(hello.playerName);
  buffer.PutU32(hello.sessionId);
}

NetClientHello ReadClientHello(NetBuffer &buffer) {
  NetClientHello hello;
  hello.protocolVersion = (int)buffer.GetU32();
  hello.buildHash = buffer.GetString();
  hello.dataVersion = buffer.GetString();
  hello.dataHash = buffer.GetString();
  hello.animationHash = buffer.GetString();
  hello.playerName = buffer.GetString();
  hello.sessionId = buffer.GetU32();
  return hello;
}

void WriteServerHello(NetBuffer &buffer, const NetServerHello &hello) {
  buffer.PutBool(hello.accepted);
  buffer.PutU8((uint8_t)hello.reason);
  buffer.PutString(hello.reasonText);
  buffer.PutU32(hello.sessionId);
}

NetServerHello ReadServerHello(NetBuffer &buffer) {
  NetServerHello hello;
  hello.accepted = buffer.GetBool();
  hello.reason = (e_NetRejectReason)buffer.GetU8();
  hello.reasonText = buffer.GetString();
  hello.sessionId = buffer.GetU32();
  return hello;
}

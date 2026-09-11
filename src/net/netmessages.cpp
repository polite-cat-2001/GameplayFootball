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

void WriteLobbyState(NetBuffer &buffer, const NetLobbyState &state) {
  buffer.PutU32(state.revision);
  buffer.PutU8((uint8_t)state.phase);
  buffer.PutU32((uint32_t)state.players.size());
  for (unsigned int i = 0; i < state.players.size(); i++) {
    const NetLobbyPlayer &player = state.players.at(i);
    buffer.PutU32(player.id);
    buffer.PutString(player.name);
    buffer.PutU8((uint8_t)player.side);
    buffer.PutBool(player.ready);
    buffer.PutBool(player.isHost);
    buffer.PutU32((uint32_t)player.ping_ms);
    buffer.PutU32((uint32_t)player.device);
  }
  for (int side = 0; side < 2; side++) {
    buffer.PutU32((uint32_t)state.teamId[side]);
    buffer.PutU32((uint32_t)state.countryId[side]);
    buffer.PutU32((uint32_t)state.leagueId[side]);
    buffer.PutBool(state.teamReady[side]);
    buffer.PutU32(state.chooser[side]);
    buffer.PutU32((uint32_t)state.teamCursor[side]);
    buffer.PutU32((uint32_t)state.listScroll[side]);
  }
}

NetLobbyState ReadLobbyState(NetBuffer &buffer) {
  NetLobbyState state;
  state.revision = buffer.GetU32();
  state.phase = (int)buffer.GetU8();
  uint32_t playerCount = buffer.GetU32();
  state.players.resize(playerCount);
  for (unsigned int i = 0; i < playerCount; i++) {
    NetLobbyPlayer &player = state.players.at(i);
    player.id = buffer.GetU32();
    player.name = buffer.GetString();
    player.side = (int)buffer.GetU8();
    player.ready = buffer.GetBool();
    player.isHost = buffer.GetBool();
    player.ping_ms = (int)buffer.GetU32();
    player.device = (int)buffer.GetU32();
  }
  for (int side = 0; side < 2; side++) {
    state.teamId[side] = (int)buffer.GetU32();
    state.countryId[side] = (int)buffer.GetU32();
    state.leagueId[side] = (int)buffer.GetU32();
    state.teamReady[side] = buffer.GetBool();
    state.chooser[side] = buffer.GetU32();
    state.teamCursor[side] = (int)buffer.GetU32();
    state.listScroll[side] = (int)buffer.GetU32();
  }
  return state;
}

void WriteLobbyAction(NetBuffer &buffer, const NetLobbyAction &action) {
  buffer.PutU8((uint8_t)action.type);
  buffer.PutU32(action.playerId);
  buffer.PutU32((uint32_t)action.side);
  buffer.PutU32((uint32_t)action.value);
  buffer.PutU32((uint32_t)action.value2);
}

NetLobbyAction ReadLobbyAction(NetBuffer &buffer) {
  NetLobbyAction action;
  action.type = (int)buffer.GetU8();
  action.playerId = buffer.GetU32();
  action.side = (int)buffer.GetU32();
  action.value = (int)buffer.GetU32();
  action.value2 = (int)buffer.GetU32();
  return action;
}

void WriteCatalog(NetBuffer &buffer, const std::vector<NetCatalogEntry> &catalog) {
  buffer.PutU32((uint32_t)catalog.size());
  for (unsigned int i = 0; i < catalog.size(); i++) {
    buffer.PutU32((uint32_t)catalog.at(i).id);
    buffer.PutString(catalog.at(i).name);
    buffer.PutString(catalog.at(i).shortName);
  }
}

std::vector<NetCatalogEntry> ReadCatalog(NetBuffer &buffer) {
  std::vector<NetCatalogEntry> catalog;
  uint32_t count = buffer.GetU32();
  catalog.resize(count);
  for (unsigned int i = 0; i < count; i++) {
    catalog.at(i).id = (int)buffer.GetU32();
    catalog.at(i).name = buffer.GetString();
    catalog.at(i).shortName = buffer.GetString();
  }
  return catalog;
}



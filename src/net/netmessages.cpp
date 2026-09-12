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
  buffer.PutBool(state.sideSelect);
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
    buffer.PutBool(player.resumeReady);
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
  buffer.PutFloat(state.matchDifficulty);
  buffer.PutFloat(state.matchDuration);
}

NetLobbyState ReadLobbyState(NetBuffer &buffer) {
  NetLobbyState state;
  state.revision = buffer.GetU32();
  state.phase = (int)buffer.GetU8();
  state.sideSelect = buffer.GetBool();
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
    player.resumeReady = buffer.GetBool();
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
  state.matchDifficulty = buffer.GetFloat();
  state.matchDuration = buffer.GetFloat();
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

void WriteMatchSetup(NetBuffer &buffer, const NetMatchSetup &setup) {
  buffer.PutU32((uint32_t)setup.teamId[0]);
  buffer.PutU32((uint32_t)setup.teamId[1]);
}

NetMatchSetup ReadMatchSetup(NetBuffer &buffer) {
  NetMatchSetup setup;
  setup.teamId[0] = (int)buffer.GetU32();
  setup.teamId[1] = (int)buffer.GetU32();
  return setup;
}

void WriteAnimationTable(NetBuffer &buffer, const std::vector<std::string> &names) {
  buffer.PutU32((uint32_t)names.size());
  for (unsigned int i = 0; i < names.size(); i++) {
    buffer.PutString(names.at(i));
  }
}

std::vector<std::string> ReadAnimationTable(NetBuffer &buffer) {
  std::vector<std::string> names;
  uint32_t count = buffer.GetU32();
  names.resize(count);
  for (unsigned int i = 0; i < count; i++) {
    names.at(i) = buffer.GetString();
  }
  return names;
}

void WriteInputFrame(NetBuffer &buffer, const NetInputFrame &frame) {
  buffer.PutU32(frame.buttons);
  buffer.PutVector3(frame.direction);
}

NetInputFrame ReadInputFrame(NetBuffer &buffer) {
  NetInputFrame frame;
  frame.buttons = buffer.GetU32();
  frame.direction = buffer.GetVector3();
  return frame;
}

void WriteMatchEnvironment(NetBuffer &buffer, const NetMatchEnvironment &environment) {
  buffer.PutVector3(environment.sunPosition);
  buffer.PutVector3(environment.sunColor);
  buffer.PutU32((uint32_t)environment.homeKit);
  buffer.PutU32((uint32_t)environment.awayKit);
}

NetMatchEnvironment ReadMatchEnvironment(NetBuffer &buffer) {
  NetMatchEnvironment environment;
  environment.sunPosition = buffer.GetVector3();
  environment.sunColor = buffer.GetVector3();
  environment.homeKit = (int)buffer.GetU32();
  environment.awayKit = (int)buffer.GetU32();
  return environment;
}

void WriteKeepalive(NetBuffer &buffer, const NetKeepalive &keepalive) {
  buffer.PutU32(keepalive.seq);
  buffer.PutU32(keepalive.echo);
}

NetKeepalive ReadKeepalive(NetBuffer &buffer) {
  NetKeepalive keepalive;
  keepalive.seq = buffer.GetU32();
  keepalive.echo = buffer.GetU32();
  return keepalive;
}



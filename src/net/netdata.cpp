#include "netdata.hpp"

void SerializePlayerDataRaw(NetBuffer &buffer, const PlayerDataRaw &player) {
  buffer.PutU32((uint32_t)player.databaseID);
  buffer.PutString(player.firstName);
  buffer.PutString(player.lastName);
  buffer.PutString(player.roleString);
  buffer.PutString(player.profileXml);
  buffer.PutFloat(player.baseStat);
  buffer.PutU32((uint32_t)player.age);
  buffer.PutU32((uint32_t)player.skinColor);
  buffer.PutString(player.hairStyle);
  buffer.PutString(player.hairColor);
  buffer.PutFloat(player.height);
}

PlayerDataRaw DeserializePlayerDataRaw(NetBuffer &buffer) {
  PlayerDataRaw player;
  player.databaseID = (int)buffer.GetU32();
  player.firstName = buffer.GetString();
  player.lastName = buffer.GetString();
  player.roleString = buffer.GetString();
  player.profileXml = buffer.GetString();
  player.baseStat = buffer.GetFloat();
  player.age = (int)buffer.GetU32();
  player.skinColor = (int)buffer.GetU32();
  player.hairStyle = buffer.GetString();
  player.hairColor = buffer.GetString();
  player.height = buffer.GetFloat();
  return player;
}

void SerializeTeamDataRaw(NetBuffer &buffer, const TeamDataRaw &team) {
  buffer.PutU32((uint32_t)team.databaseID);
  buffer.PutString(team.name);
  buffer.PutString(team.shortName);
  buffer.PutString(team.logoUrl);
  buffer.PutString(team.kitUrl);
  buffer.PutString(team.formationXml);
  buffer.PutString(team.formationFactoryXml);
  buffer.PutString(team.tacticsXml);
  buffer.PutString(team.tacticsFactoryXml);
  buffer.PutVector3(team.color1);
  buffer.PutVector3(team.color2);
  buffer.PutU32((uint32_t)team.players.size());
  for (unsigned int i = 0; i < team.players.size(); i++) {
    SerializePlayerDataRaw(buffer, team.players.at(i));
  }
}

TeamDataRaw DeserializeTeamDataRaw(NetBuffer &buffer) {
  TeamDataRaw team;
  team.databaseID = (int)buffer.GetU32();
  team.name = buffer.GetString();
  team.shortName = buffer.GetString();
  team.logoUrl = buffer.GetString();
  team.kitUrl = buffer.GetString();
  team.formationXml = buffer.GetString();
  team.formationFactoryXml = buffer.GetString();
  team.tacticsXml = buffer.GetString();
  team.tacticsFactoryXml = buffer.GetString();
  team.color1 = buffer.GetVector3();
  team.color2 = buffer.GetVector3();
  uint32_t playerCount = buffer.GetU32();
  team.players.resize(playerCount);
  for (unsigned int i = 0; i < playerCount; i++) {
    team.players.at(i) = DeserializePlayerDataRaw(buffer);
  }
  return team;
}

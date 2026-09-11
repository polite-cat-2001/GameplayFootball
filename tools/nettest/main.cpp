#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

#include "net/netclient.hpp"
#include "net/netdata.hpp"
#include "net/netserver.hpp"

static bool PlayerRawEqual(const PlayerDataRaw &a, const PlayerDataRaw &b) {
  return a.databaseID == b.databaseID &&
         a.firstName == b.firstName &&
         a.lastName == b.lastName &&
         a.roleString == b.roleString &&
         a.profileXml == b.profileXml &&
         a.baseStat == b.baseStat &&
         a.age == b.age &&
         a.skinColor == b.skinColor &&
         a.hairStyle == b.hairStyle &&
         a.hairColor == b.hairColor &&
         a.height == b.height;
}

static bool TeamRawEqual(const TeamDataRaw &a, const TeamDataRaw &b) {
  if (a.databaseID != b.databaseID ||
      a.name != b.name ||
      a.shortName != b.shortName ||
      a.logoUrl != b.logoUrl ||
      a.kitUrl != b.kitUrl ||
      a.formationXml != b.formationXml ||
      a.formationFactoryXml != b.formationFactoryXml ||
      a.tacticsXml != b.tacticsXml ||
      a.tacticsFactoryXml != b.tacticsFactoryXml ||
      a.color1.GetDistance(b.color1) != 0.0f ||
      a.color2.GetDistance(b.color2) != 0.0f ||
      a.players.size() != b.players.size()) {
    return false;
  }
  for (unsigned int i = 0; i < a.players.size(); i++) {
    if (!PlayerRawEqual(a.players.at(i), b.players.at(i))) return false;
  }
  return true;
}

static bool TestSerializationRoundTrip() {
  PlayerDataRaw p1;
  p1.databaseID = 123;
  p1.firstName = "First";
  p1.lastName = "Last";
  p1.roleString = "GK DM CF";
  p1.profileXml = "<physical_velocity>0.9</physical_velocity>";
  p1.baseStat = 0.8125f;
  p1.age = 27;
  p1.skinColor = 3;
  p1.hairStyle = "curly02";
  p1.hairColor = "brown";
  p1.height = 1.83f;

  PlayerDataRaw p2;
  p2.databaseID = 456;
  p2.firstName = "Second";

  TeamDataRaw team;
  team.databaseID = 42;
  team.name = "Test United";
  team.shortName = "TUN";
  team.logoUrl = "databases/default/images_teams/x/y_logo.png";
  team.kitUrl = "databases/default/images_teams/x/y_kit";
  team.formationXml = "<p1><position>0,0</position><role>GK</role></p1>";
  team.formationFactoryXml = "<factory/>";
  team.tacticsXml = "<position_offense_depth_factor>0.5</position_offense_depth_factor>";
  team.tacticsFactoryXml = "<factory/>";
  team.color1.Set(0.25f, 0.5f, 0.75f);
  team.color2.Set(1.0f, 0.0f, 0.125f);
  team.players.push_back(p1);
  team.players.push_back(p2);

  NetBuffer buffer;
  SerializeTeamDataRaw(buffer, team);
  buffer.ResetRead();
  TeamDataRaw restored = DeserializeTeamDataRaw(buffer);

  if (buffer.Failed()) {
    std::printf("FAIL: serialization buffer failed\n");
    return false;
  }
  if (!TeamRawEqual(team, restored)) {
    std::printf("FAIL: team round-trip mismatch\n");
    return false;
  }
  return true;
}

static bool TestHandshake(uint16_t port) {
  NetServer server(port);
  if (!server.Start()) {
    std::printf("FAIL: server could not bind port %u\n", (unsigned int)port);
    return false;
  }

  bool serverGotHello = false;
  bool serverAccepted = false;
  server.sig_OnHandshake.connect([&](const NetClientHello &hello, const NetServerHello &response) {
    serverGotHello = true;
    serverAccepted = response.accepted;
    std::printf("server: hello from '%s' (proto %d) -> accepted=%d reason='%s'\n",
                hello.playerName.c_str(), hello.protocolVersion, response.accepted, response.reasonText.c_str());
  });

  NetClient client;
  bool clientGotHello = false;
  bool clientAccepted = false;
  client.sig_OnHandshake.connect([&](const NetServerHello &response) {
    clientGotHello = true;
    clientAccepted = response.accepted;
    std::printf("client: server hello -> accepted=%d reason='%s'\n",
                response.accepted, response.reasonText.c_str());
  });

  client.Connect(NetAddress("127.0.0.1", port));

  for (int i = 0; i < 50 && !(serverGotHello && clientGotHello); i++) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  client.Disconnect();
  server.Stop();

  if (!(serverGotHello && clientGotHello && serverAccepted && clientAccepted)) {
    std::printf("FAIL: handshake incomplete (serverGotHello=%d clientGotHello=%d serverAccepted=%d clientAccepted=%d)\n",
                serverGotHello, clientGotHello, serverAccepted, clientAccepted);
    return false;
  }
  return true;
}

int main(int argc, char **argv) {
  uint16_t port = 27500;
  if (argc > 1) port = (uint16_t)std::atoi(argv[1]);

  bool ok = true;
  if (!TestHandshake(port)) ok = false;
  if (!TestSerializationRoundTrip()) ok = false;

  if (ok) {
    std::printf("PASS\n");
    return 0;
  }
  return 1;
}

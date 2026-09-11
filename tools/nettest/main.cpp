// Headless scenario tests for the LAN transport / protocol (netlib). Runs a
// NetServer and NetClient in one process and exercises the control-channel
// state machine: handshake, lobby, side selection, resume votes, disconnect,
// ping and serialization round-trips.
//
// Prints PASS/FAIL and exits 0/1.

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include "net/netclient.hpp"
#include "net/netdata.hpp"
#include "net/netmessages.hpp"
#include "net/netserver.hpp"

// ---------------------------------------------------------------------------
// tiny test framework
// ---------------------------------------------------------------------------

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond, msg) \
  do { \
    g_checks++; \
    if (!(cond)) { std::printf("FAIL: %s\n", msg); g_failures++; } \
  } while (0)

static void SleepMs(int ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

template <typename Predicate>
static bool WaitFor(Predicate predicate, int timeoutMs = 3000, int stepMs = 20) {
  int waited = 0;
  while (waited < timeoutMs) {
    if (predicate()) return true;
    SleepMs(stepMs);
    waited += stepMs;
  }
  return predicate();
}

// A connected server/client pair on a fixed port.
struct NetPair {
  explicit NetPair(uint16_t port) : server(port), connected(false) {}

  ~NetPair() {
    client.Disconnect();
    server.Stop();
  }

  bool Connect() {
    if (!server.Start()) return false;
    client.Connect(NetAddress("127.0.0.1", server.GetPort()));
    connected = WaitFor([this]() {
      return client.GetState() == e_NetConnectionState_Connected;
    });
    return connected;
  }

  NetServer server;
  NetClient client;
  bool connected;
};

static bool ServerPlayerHasSide(NetServer &server, uint32_t id, int side) {
  NetLobbyState state = server.GetLobbyState();
  for (unsigned int i = 0; i < state.players.size(); i++) {
    if (state.players.at(i).id == id) return state.players.at(i).side == side;
  }
  return false;
}

static bool ServerPlayerReady(NetServer &server, uint32_t id, bool ready) {
  NetLobbyState state = server.GetLobbyState();
  for (unsigned int i = 0; i < state.players.size(); i++) {
    if (state.players.at(i).id == id) return state.players.at(i).ready == ready;
  }
  return false;
}

// ---------------------------------------------------------------------------
// serialization round-trips
// ---------------------------------------------------------------------------

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

static void TestSerializationRoundTrips() {
  // Team / player data.
  {
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

    NetBuffer buffer;
    SerializeTeamDataRaw(buffer, team);
    buffer.ResetRead();
    TeamDataRaw restored = DeserializeTeamDataRaw(buffer);
    CHECK(!buffer.Failed(), "team raw buffer failed");
    CHECK(TeamRawEqual(team, restored), "team raw round-trip mismatch");
  }

  // Lobby state, including the newer sideSelect / resumeReady / device fields.
  {
    NetLobbyState state;
    state.revision = 7;
    state.phase = e_NetLobbyPhase_Sides;
    state.sideSelect = true;
    NetLobbyPlayer player;
    player.id = 5;
    player.name = "Tester";
    player.side = e_NetSide_Away;
    player.ready = true;
    player.isHost = false;
    player.ping_ms = 42;
    player.device = 1;
    player.resumeReady = true;
    state.players.push_back(player);
    state.teamId[0] = 3;
    state.teamId[1] = 8;
    state.countryId[0] = 10;
    state.countryId[1] = 11;
    state.leagueId[0] = 1;
    state.leagueId[1] = 2;
    state.teamReady[0] = true;
    state.teamReady[1] = false;
    state.chooser[0] = 0;
    state.chooser[1] = 5;
    state.teamCursor[0] = 4;
    state.teamCursor[1] = 9;
    state.listScroll[0] = 1;
    state.listScroll[1] = 2;

    NetBuffer buffer;
    WriteLobbyState(buffer, state);
    buffer.ResetRead();
    NetLobbyState restored = ReadLobbyState(buffer);
    CHECK(!buffer.Failed(), "lobby buffer failed");
    CHECK(restored.revision == state.revision, "lobby revision mismatch");
    CHECK(restored.phase == state.phase, "lobby phase mismatch");
    CHECK(restored.sideSelect == state.sideSelect, "lobby sideSelect mismatch");
    CHECK(restored.players.size() == 1, "lobby player count mismatch");
    CHECK(restored.players.at(0).id == player.id, "lobby player id mismatch");
    CHECK(restored.players.at(0).name == player.name, "lobby player name mismatch");
    CHECK(restored.players.at(0).side == player.side, "lobby player side mismatch");
    CHECK(restored.players.at(0).ready == player.ready, "lobby player ready mismatch");
    CHECK(restored.players.at(0).device == player.device, "lobby player device mismatch");
    CHECK(restored.players.at(0).resumeReady == player.resumeReady, "lobby player resumeReady mismatch");
    CHECK(restored.chooser[1] == state.chooser[1], "lobby chooser mismatch");
    CHECK(restored.teamCursor[0] == state.teamCursor[0], "lobby teamCursor mismatch");
    CHECK(restored.teamId[1] == state.teamId[1], "lobby teamId mismatch");
  }

  // Match environment, including kit numbers.
  {
    NetMatchEnvironment environment;
    environment.sunPosition.Set(1.0f, 2.0f, 3.0f);
    environment.sunColor.Set(0.5f, 0.25f, 0.125f);
    environment.homeKit = 2;
    environment.awayKit = 3;
    NetBuffer buffer;
    WriteMatchEnvironment(buffer, environment);
    buffer.ResetRead();
    NetMatchEnvironment restored = ReadMatchEnvironment(buffer);
    CHECK(!buffer.Failed(), "environment buffer failed");
    CHECK(restored.sunPosition.GetDistance(environment.sunPosition) == 0.0f, "environment sun pos mismatch");
    CHECK(restored.sunColor.GetDistance(environment.sunColor) == 0.0f, "environment sun color mismatch");
    CHECK(restored.homeKit == 2 && restored.awayKit == 3, "environment kit mismatch");
  }

  // Keepalive / ping.
  {
    NetKeepalive keepalive;
    keepalive.seq = 12345;
    keepalive.echo = 67890;
    NetBuffer buffer;
    WriteKeepalive(buffer, keepalive);
    buffer.ResetRead();
    NetKeepalive restored = ReadKeepalive(buffer);
    CHECK(!buffer.Failed(), "keepalive buffer failed");
    CHECK(restored.seq == keepalive.seq && restored.echo == keepalive.echo, "keepalive round-trip mismatch");
  }
}

// ---------------------------------------------------------------------------
// transport / protocol scenarios
// ---------------------------------------------------------------------------

static void TestHandshake(uint16_t port) {
  NetPair pair(port);
  CHECK(pair.Connect(), "handshake: client did not connect");
  CHECK(pair.server.GetLobbyState().players.size() == 2, "handshake: lobby should have host + client");
}

static void TestLobbyFlow(uint16_t port) {
  NetPair pair(port);
  if (!pair.Connect()) { CHECK(false, "lobby: connect failed"); return; }

  uint32_t clientId = pair.client.GetPlayerId();
  CHECK(clientId != 0, "lobby: client id should be nonzero");

  NetLobbyAction action;
  action.type = e_NetLobbyAction_SetSide;
  action.side = e_NetSide_Away;
  pair.client.SendLobbyAction(action);
  CHECK(WaitFor([&]() { return ServerPlayerHasSide(pair.server, clientId, e_NetSide_Away); }),
        "lobby: client side not applied");

  // All peers (host + client) ready -> Sides advances to Teams.
  action = NetLobbyAction();
  action.type = e_NetLobbyAction_SetReady;
  action.value = 1;
  pair.client.SendLobbyAction(action);
  CHECK(WaitFor([&]() { return ServerPlayerReady(pair.server, clientId, true); }),
        "lobby: client ready not applied");

  action.playerId = 0; // host
  pair.server.ApplyLobbyAction(action);
  CHECK(WaitFor([&]() { return pair.server.GetLobbyState().phase == e_NetLobbyPhase_Teams; }),
        "lobby: all-ready did not advance to Teams");
}

static void TestSideSelectOpenCancel(uint16_t port) {
  NetPair pair(port);
  if (!pair.Connect()) { CHECK(false, "sideSelect: connect failed"); return; }

  // Open (value != 0).
  NetLobbyAction action;
  action.type = e_NetLobbyAction_RequestSideSelect;
  action.value = 1;
  pair.client.SendLobbyAction(action);
  CHECK(WaitFor([&]() { return pair.server.GetLobbyState().sideSelect; }),
        "sideSelect: open not applied");
  CHECK(pair.server.GetLobbyState().phase == e_NetLobbyPhase_Sides, "sideSelect: phase should be Sides");
  CHECK(!pair.server.GetLobbyState().players.empty() &&
        !pair.server.GetLobbyState().players.at(0).ready, "sideSelect: ready should reset on open");
  CHECK(WaitFor([&]() { return pair.client.GetLobbyState().sideSelect; }),
        "sideSelect: client did not mirror open");

  // Cancel (value == 0).
  action.value = 0;
  pair.client.SendLobbyAction(action);
  CHECK(WaitFor([&]() { return!pair.server.GetLobbyState().sideSelect; }),
        "sideSelect: cancel not applied");
  CHECK(pair.server.ConsumeSideSelectCancel(), "sideSelect: host did not get cancel signal");
}

static void TestResumeVote(uint16_t port) {
  NetPair pair(port);
  if (!pair.Connect()) { CHECK(false, "resume: connect failed"); return; }

  NetLobbyAction vote;
  vote.type = e_NetLobbyAction_SetResumeReady;
  vote.value = 1;

  pair.client.SendLobbyAction(vote);
  CHECK(WaitFor([&]() {
    NetLobbyState s = pair.server.GetLobbyState();
    for (unsigned int i = 0; i < s.players.size(); i++) {
      if (!s.players.at(i).isHost) return s.players.at(i).resumeReady;
    }
    return false;
  }), "resume: client vote not applied");
  CHECK(!pair.server.ConsumeAllResumeReady(), "resume: should not be complete with one vote");

  vote.playerId = 0; // host
  pair.server.ApplyLobbyAction(vote);
  CHECK(pair.server.ConsumeAllResumeReady(), "resume: all votes should complete");

  pair.server.ResetResumeVotes();
  CHECK(!pair.server.ConsumeAllResumeReady(), "resume: reset should clear completion");
}

static void TestDisconnect(uint16_t port) {
  NetPair pair(port);
  if (!pair.Connect()) { CHECK(false, "disconnect: connect failed"); return; }

  uint32_t clientId = pair.client.GetPlayerId();
  pair.client.Disconnect();

  uint32_t gone = 0;
  CHECK(WaitFor([&]() { return pair.server.ConsumeDisconnectedPlayer(gone); }, 5000),
        "disconnect: host did not detect disconnect");
  CHECK(gone == clientId, "disconnect: wrong player id reported");
  CHECK(WaitFor([&]() { return pair.server.GetLobbyState().players.size() == 1; }),
        "disconnect: player not removed from lobby");
}

static void TestKeepaliveRtt(uint16_t port) {
  NetPair pair(port);
  if (!pair.Connect()) { CHECK(false, "rtt: connect failed"); return; }

  // The ping timer runs every 500 ms; after ~1.2 s both sides should have a
  // measured round-trip time.
  SleepMs(1200);
  CHECK(pair.client.GetRtt_ms() >= 0, "rtt: client has no measurement");
  CHECK(pair.server.GetMaxClientRtt_ms() >= 0, "rtt: host has no measurement");
}

int main(int argc, char **argv) {
  uint16_t basePort = 27500;
  if (argc > 1) basePort = (uint16_t)std::atoi(argv[1]);

  TestSerializationRoundTrips();
  TestHandshake(basePort + 0);
  TestLobbyFlow(basePort + 1);
  TestSideSelectOpenCancel(basePort + 2);
  TestResumeVote(basePort + 3);
  TestDisconnect(basePort + 4);
  TestKeepaliveRtt(basePort + 5);

  if (g_failures == 0) {
    std::printf("PASS (%d checks)\n", g_checks);
    return 0;
  }
  std::printf("FAIL (%d/%d checks failed)\n", g_failures, g_checks);
  return 1;
}

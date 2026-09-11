#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include "net/netclient.hpp"
#include "net/netserver.hpp"

int main(int argc, char **argv) {
  uint16_t port = 27500;
  if (argc > 1) port = (uint16_t)std::atoi(argv[1]);

  NetServer server(port);
  if (!server.Start()) {
    std::printf("FAIL: server could not bind port %u\n", (unsigned int)port);
    return 1;
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

  if (serverGotHello && clientGotHello && serverAccepted && clientAccepted) {
    std::printf("PASS\n");
    return 0;
  }

  std::printf("FAIL: handshake incomplete (serverGotHello=%d clientGotHello=%d serverAccepted=%d clientAccepted=%d)\n",
              serverGotHello, clientGotHello, serverAccepted, clientAccepted);
  return 1;
}

#include "netserver.hpp"

#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/make_shared.hpp>

#include <algorithm>
#include <chrono>
#include <deque>
#include <map>

#include "netassets.hpp"
#include "nethiddevice.hpp"
#include "netudp.hpp"

#include "base/utils.hpp"

namespace {
const size_t kHeaderSize = 4;

unsigned long SteadyNow_ms() {
  return (unsigned long)std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}
}

class NetServerConnection : public boost::enable_shared_from_this<NetServerConnection> {

  public:
    NetServerConnection(boost::asio::io_context &ioContext, NetServer *server)
        : socket(ioContext), server(server), sessionId(0) {}

    boost::asio::ip::tcp::socket &GetSocket() { return socket; }

    uint32_t GetSessionId() const { return sessionId; }
    void SetSessionId(uint32_t id) { sessionId = id; }

    boost::shared_ptr<NetHIDDevice> GetHIDevice() const { return hidDevice; }
    void SetHIDevice(boost::shared_ptr<NetHIDDevice> device) { hidDevice = device; }

    int GetRtt_ms() const { return rtt_ms.load(); }
    unsigned long GetLastPacketTime_ms() const { return lastPacketTime_ms.load(); }
    void Touch() { lastPacketTime_ms.store(SteadyNow_ms()); }

    void SendPing() {
      NetKeepalive keepalive;
      keepalive.seq = ++pingSeq;
      keepalive.echo = lastReceivedSeq;
      pingSent[keepalive.seq] = SteadyNow_ms();
      if (pingSent.size() > 64) pingSent.erase(pingSent.begin());

      NetBuffer buffer;
      WriteKeepalive(buffer, keepalive);
      SendMessage(e_NetMessage_Keepalive, buffer);
    }

    // Reply to a ping immediately (seq 0 marks it as a pong, so it never
    // triggers another reply). Waiting for the next periodic keepalive would
    // inflate the measured RTT by up to a whole keepalive interval.
    void SendPong(uint32_t echoSeq) {
      NetKeepalive keepalive;
      keepalive.seq = 0;
      keepalive.echo = echoSeq;
      NetBuffer buffer;
      WriteKeepalive(buffer, keepalive);
      SendMessage(e_NetMessage_Keepalive, buffer);
    }

    void HandleKeepalive(const NetKeepalive &keepalive) {
      Touch();
      if (keepalive.seq != 0) {
        lastReceivedSeq = keepalive.seq;
        SendPong(keepalive.seq);
      }
      if (keepalive.echo != 0) {
        std::map<uint32_t, unsigned long>::iterator iter = pingSent.find(keepalive.echo);
        if (iter != pingSent.end()) {
          rtt_ms.store((int)(SteadyNow_ms() - iter->second));
          pingSent.erase(iter);
        }
      }
    }

    void Start() {
      Touch();
      ReadHeader();
    }

    void SendMessage(e_NetMessageType type, NetBuffer &body) {
      boost::shared_ptr<std::vector<uint8_t> > payload = boost::make_shared<std::vector<uint8_t> >();
      payload->push_back((uint8_t)type);
      const std::vector<uint8_t> &bodyData = body.Data();
      payload->insert(payload->end(), bodyData.begin(), bodyData.end());

      boost::shared_ptr<std::vector<uint8_t> > packet = boost::make_shared<std::vector<uint8_t> >();
      packet->resize(kHeaderSize + payload->size());
      NetWriteU32LE(&packet->at(0), (uint32_t)payload->size());
      std::copy(payload->begin(), payload->end(), packet->begin() + kHeaderSize);

      boost::asio::post(socket.get_executor(),
          boost::bind(&NetServerConnection::Enqueue, shared_from_this(), packet));
    }

  private:
    void Enqueue(boost::shared_ptr<std::vector<uint8_t> > packet) {
      bool writeInProgress = !writeQueue.empty();
      writeQueue.push_back(packet);
      if (!writeInProgress) DoWrite();
    }

    void ReadHeader() {
      boost::asio::async_read(socket, boost::asio::buffer(header, kHeaderSize),
          boost::bind(&NetServerConnection::HandleHeader, shared_from_this(), boost::asio::placeholders::error));
    }

    void HandleHeader(const boost::system::error_code &error) {
      if (error) { server->RemoveConnection(shared_from_this()); return; }
      uint32_t length = NetReadU32LE(header);
      if (length == 0 || length > 1024 * 1024) { server->RemoveConnection(shared_from_this()); return; }
      body.resize(length);
      boost::asio::async_read(socket, boost::asio::buffer(body),
          boost::bind(&NetServerConnection::HandleBody, shared_from_this(), boost::asio::placeholders::error));
    }

    void HandleBody(const boost::system::error_code &error) {
      if (error) { server->RemoveConnection(shared_from_this()); return; }
      if (!body.empty()) {
        Touch();
        e_NetMessageType type = (e_NetMessageType)body[0];
        NetBuffer buffer;
        buffer.Data().assign(body.begin() + 1, body.end());
        buffer.ResetRead();
        Dispatch(type, buffer);
      }
      ReadHeader();
    }

    void Dispatch(e_NetMessageType type, NetBuffer &buffer) {
      if (type == e_NetMessage_ClientHello) {
        NetClientHello hello = ReadClientHello(buffer);
        server->HandleClientHello(shared_from_this(), hello);
      } else if (type == e_NetMessage_LobbyAction) {
        NetLobbyAction action = ReadLobbyAction(buffer);
        server->HandleLobbyAction(shared_from_this(), action);
      } else if (type == e_NetMessage_InputFrame) {
        NetInputFrame frame = ReadInputFrame(buffer);
        server->HandleInputFrame(shared_from_this(), frame);
      } else if (type == e_NetMessage_PauseRequest) {
        server->HandlePauseRequest(buffer.GetBool());
      } else if (type == e_NetMessage_ReplayStop) {
        server->HandleReplayStop();
      } else if (type == e_NetMessage_Keepalive) {
        HandleKeepalive(ReadKeepalive(buffer));
      }
    }

    void DoWrite() {
      if (writeQueue.empty()) return;
      boost::asio::async_write(socket, boost::asio::buffer(*writeQueue.front()),
          boost::bind(&NetServerConnection::HandleWrite, shared_from_this(), boost::asio::placeholders::error));
    }

    void HandleWrite(const boost::system::error_code &error) {
      if (error) { server->RemoveConnection(shared_from_this()); return; }
      writeQueue.pop_front();
      if (!writeQueue.empty()) DoWrite();
    }

    boost::asio::ip::tcp::socket socket;
    NetServer *server;
    uint32_t sessionId;
    boost::shared_ptr<NetHIDDevice> hidDevice;
    uint8_t header[kHeaderSize];
    std::vector<uint8_t> body;
    std::deque<boost::shared_ptr<std::vector<uint8_t> > > writeQueue;

    uint32_t pingSeq = 0;
    uint32_t lastReceivedSeq = 0;
    std::map<uint32_t, unsigned long> pingSent;
    std::atomic<int> rtt_ms{-1};
    std::atomic<unsigned long> lastPacketTime_ms{0};
};

NetServer::NetServer(uint16_t port) : port(port), running(false), nextSessionId(1), pauseRequestPending(false), pauseRequestState(false), replayStopPending(false), allResumeReadyPending(false), sideSelectCancelPending(false) {
}

NetServer::~NetServer() {
  Stop();
}

bool NetServer::Start() {
  if (running.load()) return true;

  boost::system::error_code error;
  acceptor = boost::make_shared<boost::asio::ip::tcp::acceptor>(ioContext);
  boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), port);
  acceptor->open(endpoint.protocol(), error);
  if (error) { acceptor.reset(); return false; }
  acceptor->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true), error);
  acceptor->bind(endpoint, error);
  if (error) { acceptor->close(); acceptor.reset(); return false; }
  acceptor->listen(boost::asio::socket_base::max_listen_connections, error);
  if (error) { acceptor->close(); acceptor.reset(); return false; }

  {
    boost::mutex::scoped_lock lock(lobbyMutex);
    lobbyState = NetLobbyState();
    NetLobbyPlayer host;
    host.id = 0;
    host.name = "Host";
    host.side = e_NetSide_Home;
    host.isHost = true;
    lobbyState.players.push_back(host);
    RecomputeChoppers();
  }

  workGuard = boost::shared_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type> >(
      new boost::asio::executor_work_guard<boost::asio::io_context::executor_type>(boost::asio::make_work_guard(ioContext)));
  running.store(true);
  StartUdp();
  DoAccept();
  StartPingTimer();
  ioThread = boost::thread(boost::bind(&NetServer::Run, this));
  return true;
}

void NetServer::Stop() {
  if (!running.load()) return;

  running.store(false);

  if (pingTimer) {
    pingTimer->cancel();
  }

  if (acceptor) {
    boost::system::error_code error;
    acceptor->close(error);
  }

  if (udpSocket) {
    // Close but keep the object alive: a posted DoSendUdp on the io thread may
    // still hold a copy. The socket is freed with the server.
    boost::system::error_code error;
    udpSocket->close(error);
  }
  {
    boost::mutex::scoped_lock lock(udpMutex);
    udpEndpoints.clear();
    udpLastInputSeq.clear();
  }

  {
    boost::mutex::scoped_lock lock(connectionsMutex);
    for (unsigned int i = 0; i < connections.size(); i++) {
      boost::system::error_code error;
      connections.at(i)->GetSocket().close(error);
    }
    connections.clear();
  }

  {
    boost::mutex::scoped_lock lock(retiredMutex);
    retiredDevices.clear();
  }

  if (workGuard) workGuard->reset();
  ioContext.stop();
  if (ioThread.joinable()) ioThread.join();
  ioContext.restart();
}

void NetServer::Run() {
  ioContext.run();
}

void NetServer::StartUdp() {
  boost::system::error_code error;
  udpSocket = boost::make_shared<boost::asio::ip::udp::socket>(ioContext);
  udpSocket->open(boost::asio::ip::udp::v4(), error);
  if (error) { udpSocket.reset(); return; }
  udpSocket->set_option(boost::asio::socket_base::reuse_address(true), error);
  udpSocket->bind(boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), port), error);
  if (error) { udpSocket->close(); udpSocket.reset(); return; }
  StartUdpReceive();
}

void NetServer::StartUdpReceive() {
  if (!udpSocket) return;
  udpRecvBuffer.resize(2048);
  udpSocket->async_receive_from(boost::asio::buffer(udpRecvBuffer), udpSenderEndpoint,
      boost::bind(&NetServer::HandleUdpReceive, this, boost::asio::placeholders::error,
                  boost::asio::placeholders::bytes_transferred));
}

void NetServer::HandleUdpReceive(const boost::system::error_code &error, std::size_t bytesTransferred) {
  if (!running.load()) return;
  if (error) { StartUdpReceive(); return; } // e.g. WSAECONNRESET from a peers' ICMP

  uint8_t type = 0;
  uint32_t sessionId = 0, seq = 0;
  if (bytesTransferred >= net_udpHeaderSize &&
      NetReadUdpHeader(udpRecvBuffer.data(), bytesTransferred, type, sessionId, seq)) {
    if (type == e_NetUdpType_Hello || type == e_NetUdpType_Input) {
      // Learn the client's UDP endpoint from the datagram itself (no NAT here:
      // direct LAN / Hamachi). Even a retransmitted Hello just refreshes it.
      {
        boost::mutex::scoped_lock lock(udpMutex);
        udpEndpoints[sessionId] = udpSenderEndpoint;
      }
      if (type == e_NetUdpType_Input) {
        bool fresh = false;
        {
          boost::mutex::scoped_lock lock(udpMutex);
          std::map<uint32_t, uint32_t>::iterator iter = udpLastInputSeq.find(sessionId);
          if (iter == udpLastInputSeq.end() || seq >= iter->second) {
            udpLastInputSeq[sessionId] = seq;
            fresh = true;
          }
        }
        if (fresh) {
          NetBuffer buffer;
          buffer.Data().assign(udpRecvBuffer.begin() + net_udpHeaderSize, udpRecvBuffer.begin() + bytesTransferred);
          buffer.ResetRead();
          NetInputFrame frame = ReadInputFrame(buffer);
          boost::shared_ptr<NetHIDDevice> device = GetHIDevice(sessionId);
          if (device) device->SetInput(frame);
        }
      }
    }
  }

  StartUdpReceive();
}

void NetServer::DoSendUdp(boost::shared_ptr<std::vector<uint8_t> > packet, const boost::asio::ip::udp::endpoint &endpoint) {
  if (!udpSocket) return;
  udpSocket->async_send_to(boost::asio::buffer(*packet), endpoint,
      [packet](const boost::system::error_code &, std::size_t) {
        (void)packet; // keeps the buffer alive until the async send completes
      });
}

bool NetServer::HasUdpEndpoint(uint32_t sessionId) {
  boost::mutex::scoped_lock lock(udpMutex);
  return udpEndpoints.find(sessionId) != udpEndpoints.end();
}

void NetServer::BroadcastSnapshot(NetBuffer &body) {
  std::vector<boost::shared_ptr<NetServerConnection> > current;
  {
    boost::mutex::scoped_lock lock(connectionsMutex);
    current = connections;
  }
  if (current.empty()) return;

  const uint32_t seq = ++snapshotSeq;
  const std::vector<uint8_t> &payload = body.Data();

  for (unsigned int i = 0; i < current.size(); i++) {
    uint32_t sessionId = current.at(i)->GetSessionId();
    boost::asio::ip::udp::endpoint endpoint;
    bool hasEndpoint = false;
    {
      boost::mutex::scoped_lock lock(udpMutex);
      std::map<uint32_t, boost::asio::ip::udp::endpoint>::iterator iter = udpEndpoints.find(sessionId);
      if (iter != udpEndpoints.end()) { endpoint = iter->second; hasEndpoint = true; }
    }
    if (hasEndpoint) {
      boost::shared_ptr<std::vector<uint8_t> > packet = boost::make_shared<std::vector<uint8_t> >();
      NetWriteUdpHeader(*packet, e_NetUdpType_Snapshot, sessionId, seq);
      packet->insert(packet->end(), payload.begin(), payload.end());
      boost::asio::post(ioContext, boost::bind(&NetServer::DoSendUdp, this, packet, endpoint));
    } else {
      // No UDP datagram from this peer yet: keep it fed over reliable TCP.
      current.at(i)->SendMessage(e_NetMessage_Snapshot, body);
    }
  }
}

void NetServer::StartPingTimer() {
  pingTimer = boost::make_shared<boost::asio::steady_timer>(ioContext);
  SchedulePingTimer();
}

void NetServer::SchedulePingTimer() {
  pingTimer->expires_after(boost::asio::chrono::milliseconds(net_keepaliveInterval_ms));
  pingTimer->async_wait(boost::bind(&NetServer::HandlePingTimer, this, boost::asio::placeholders::error));
}

void NetServer::HandlePingTimer(const boost::system::error_code &error) {
  if (error || !running.load()) return;

  std::vector<boost::shared_ptr<NetServerConnection> > current;
  {
    boost::mutex::scoped_lock lock(connectionsMutex);
    current = connections;
  }

  const unsigned long now_ms = SteadyNow_ms();
  for (unsigned int i = 0; i < current.size(); i++) {
    current.at(i)->SendPing();
    if (now_ms - current.at(i)->GetLastPacketTime_ms() > (unsigned long)net_disconnectTimeout_ms) {
      RemoveConnection(current.at(i));
    }
  }

  SchedulePingTimer();
}

int NetServer::GetMaxClientRtt_ms() {
  int maxRtt = 0;
  boost::mutex::scoped_lock lock(connectionsMutex);
  for (unsigned int i = 0; i < connections.size(); i++) {
    int rtt = connections.at(i)->GetRtt_ms();
    if (rtt > maxRtt) maxRtt = rtt;
  }
  return maxRtt;
}

bool NetServer::ConsumeDisconnectedPlayer(uint32_t &playerId) {
  boost::mutex::scoped_lock lock(disconnectMutex);
  if (disconnectedPlayers.empty()) return false;
  playerId = disconnectedPlayers.front();
  disconnectedPlayers.erase(disconnectedPlayers.begin());
  return true;
}

bool NetServer::ConsumeJoinedPlayer(uint32_t &playerId) {
  boost::mutex::scoped_lock lock(disconnectMutex);
  if (joinedPlayers.empty()) return false;
  playerId = joinedPlayers.front();
  joinedPlayers.erase(joinedPlayers.begin());
  return true;
}

void NetServer::ClearRosterEvents() {
  boost::mutex::scoped_lock lock(disconnectMutex);
  disconnectedPlayers.clear();
  joinedPlayers.clear();
}

void NetServer::ClearRetiredDevices() {
  boost::mutex::scoped_lock lock(retiredMutex);
  retiredDevices.clear();
}

void NetServer::SetSideSelectMode(bool on) {
  {
    boost::mutex::scoped_lock lock(lobbyMutex);
    if (lobbyState.sideSelect == on) return;
    lobbyState.sideSelect = on;
    lobbyState.phase = e_NetLobbyPhase_Sides;
    for (unsigned int i = 0; i < lobbyState.players.size(); i++) {
      lobbyState.players.at(i).ready = false;
      lobbyState.players.at(i).resumeReady = false;
    }
    lobbyState.teamReady[0] = false;
    lobbyState.teamReady[1] = false;
    lobbyState.revision++;
    RecomputeChoppers();
  }
  {
    boost::mutex::scoped_lock lock(resumeMutex);
    allResumeReadyPending = false;
    sideSelectCancelPending = false;
  }
  BroadcastLobbyState();
}

void NetServer::RecomputeResumeReady() {
  // caller holds lobbyMutex
  bool allReady = !lobbyState.players.empty() && !lobbyState.sideSelect;
  for (unsigned int i = 0; i < lobbyState.players.size(); i++) {
    if (!lobbyState.players.at(i).resumeReady) { allReady = false; break; }
  }
  boost::mutex::scoped_lock lock(resumeMutex);
  if (allReady) allResumeReadyPending = true;
}

void NetServer::ResetResumeVotes() {
  {
    boost::mutex::scoped_lock lock(lobbyMutex);
    for (unsigned int i = 0; i < lobbyState.players.size(); i++) lobbyState.players.at(i).resumeReady = false;
    lobbyState.revision++;
  }
  {
    boost::mutex::scoped_lock lock(resumeMutex);
    allResumeReadyPending = false;
    sideSelectCancelPending = false;
  }
  BroadcastLobbyState();
}

bool NetServer::ConsumeAllResumeReady() {
  boost::mutex::scoped_lock lock(resumeMutex);
  if (!allResumeReadyPending) return false;
  allResumeReadyPending = false;
  return true;
}

bool NetServer::ConsumeSideSelectCancel() {
  boost::mutex::scoped_lock lock(resumeMutex);
  if (!sideSelectCancelPending) return false;
  sideSelectCancelPending = false;
  return true;
}

void NetServer::SendToPlayer(uint32_t playerId, e_NetMessageType type, NetBuffer &body) {
  boost::shared_ptr<NetServerConnection> target;
  {
    boost::mutex::scoped_lock lock(connectionsMutex);
    for (unsigned int i = 0; i < connections.size(); i++) {
      if (connections.at(i)->GetSessionId() == playerId) { target = connections.at(i); break; }
    }
  }
  if (target) target->SendMessage(type, body);
}

void NetServer::DoAccept() {
  boost::shared_ptr<NetServerConnection> connection = boost::make_shared<NetServerConnection>(ioContext, this);
  acceptor->async_accept(connection->GetSocket(),
      boost::bind(&NetServer::HandleAccept, this, connection, boost::asio::placeholders::error));
}

void NetServer::HandleAccept(boost::shared_ptr<NetServerConnection> connection, const boost::system::error_code &error) {
  if (!error) {
    {
      boost::mutex::scoped_lock lock(connectionsMutex);
      connections.push_back(connection);
    }
    connection->Start();
  }

  if (running.load()) DoAccept();
}

void NetServer::HandleClientHello(boost::shared_ptr<NetServerConnection> connection, const NetClientHello &hello) {
  NetServerHello response;
  response.sessionId = nextSessionId++;

  const std::string localBuild = NetGetBuildHash();
  const std::string localDataVersion = NetGetDataVersion();
  const std::string localDataHash = NetGetDataHash();
  const std::string localAnimationHash = NetGetAnimationHash();

  if (hello.protocolVersion != net_protocolVersion) {
    response.reason = e_NetReject_ProtocolMismatch;
    response.reasonText = "protocol version mismatch";
  } else if (hello.buildHash != localBuild) {
    response.reason = e_NetReject_BuildMismatch;
    response.reasonText = "build mismatch";
  } else if (hello.dataVersion != localDataVersion) {
    response.reason = e_NetReject_DataVersionMismatch;
    response.reasonText = "data version mismatch";
  } else if (hello.dataHash != localDataHash) {
    response.reason = e_NetReject_DataHashMismatch;
    response.reasonText = "data hash mismatch";
  } else if (hello.animationHash != localAnimationHash) {
    response.reason = e_NetReject_AnimationMismatch;
    response.reasonText = "animation set mismatch";
  } else {
    boost::mutex::scoped_lock lock(lobbyMutex);
    if (lobbyState.players.size() >= (unsigned int)net_maxPlayers) {
      response.reason = e_NetReject_LobbyFull;
      response.reasonText = "lobby is full";
    }
  }

  response.accepted = (response.reason == e_NetReject_None);

  if (response.accepted) {
    connection->SetSessionId(response.sessionId);
    connection->SetHIDevice(boost::make_shared<NetHIDDevice>("net_client_" + blunted::int_to_str(response.sessionId), e_HIDeviceType_Keyboard, response.sessionId));

    boost::mutex::scoped_lock lock(lobbyMutex);
    NetLobbyPlayer player;
    player.id = response.sessionId;
    player.name = hello.playerName.empty() ? ("Player " + blunted::int_to_str(response.sessionId)) : hello.playerName;
    player.side = e_NetSide_Spectator;
    player.isHost = false;
    lobbyState.players.push_back(player);
    // A new peer must pick a side: send everyone back to the side phase.
    lobbyState.phase = e_NetLobbyPhase_Sides;
    for (unsigned int i = 0; i < lobbyState.players.size(); i++) lobbyState.players.at(i).ready = false;
    lobbyState.revision++;
    RecomputeChoppers();
    {
      boost::mutex::scoped_lock lock(disconnectMutex);
      joinedPlayers.push_back(response.sessionId);
    }
  }

  NetBuffer buffer;
  WriteServerHello(buffer, response);
  connection->SendMessage(e_NetMessage_ServerHello, buffer);

  if (response.accepted) {
    NetBuffer catalogBuffer;
    {
      boost::mutex::scoped_lock lock(lobbyMutex);
      WriteCatalog(catalogBuffer, catalog);
    }
    connection->SendMessage(e_NetMessage_Catalog, catalogBuffer);
    BroadcastLobbyState();
  }
}

void NetServer::HandleLobbyAction(boost::shared_ptr<NetServerConnection> connection, const NetLobbyAction &action) {
  NetLobbyAction attributed = action;
  attributed.playerId = connection->GetSessionId();
  ApplyLobbyAction(attributed);
}

void NetServer::HandleInputFrame(boost::shared_ptr<NetServerConnection> connection, const NetInputFrame &frame) {
  boost::shared_ptr<NetHIDDevice> device = connection->GetHIDevice();
  if (device) device->SetInput(frame);
}

boost::shared_ptr<NetHIDDevice> NetServer::GetHIDevice(uint32_t sessionId) {
  boost::mutex::scoped_lock lock(connectionsMutex);
  for (unsigned int i = 0; i < connections.size(); i++) {
    if (connections.at(i)->GetSessionId() == sessionId) return connections.at(i)->GetHIDevice();
  }
  return boost::shared_ptr<NetHIDDevice>();
}

void NetServer::HandlePauseRequest(bool paused) {
  boost::mutex::scoped_lock lock(pauseMutex);
  pauseRequestPending = true;
  pauseRequestState = paused;
}

bool NetServer::ConsumePauseRequest(bool &paused) {
  boost::mutex::scoped_lock lock(pauseMutex);
  if (!pauseRequestPending) return false;
  paused = pauseRequestState;
  pauseRequestPending = false;
  return true;
}

void NetServer::BroadcastPause(bool paused) {
  NetBuffer buffer;
  buffer.PutBool(paused);
  BroadcastMessage(e_NetMessage_PauseState, buffer);
}

void NetServer::HandleReplayStop() {
  {
    boost::mutex::scoped_lock lock(replayMutex);
    replayStopPending = true;
  }
  BroadcastReplayStop();
}

void NetServer::BroadcastReplayStop() {
  NetBuffer buffer;
  BroadcastMessage(e_NetMessage_ReplayStop, buffer);
}

bool NetServer::ConsumeReplayStop() {
  boost::mutex::scoped_lock lock(replayMutex);
  if (!replayStopPending) return false;
  replayStopPending = false;
  return true;
}

std::vector<boost::shared_ptr<NetHIDDevice> > NetServer::GetHIDevices() {
  std::vector<boost::shared_ptr<NetHIDDevice> > devices;
  boost::mutex::scoped_lock lock(connectionsMutex);
  for (unsigned int i = 0; i < connections.size(); i++) {
    boost::shared_ptr<NetHIDDevice> device = connections.at(i)->GetHIDevice();
    if (device) devices.push_back(device);
  }
  return devices;
}

void NetServer::ApplyLobbyAction(const NetLobbyAction &action) {
  bool changed = false;

  {
    boost::mutex::scoped_lock lock(lobbyMutex);

    NetLobbyPlayer *player = 0;
    for (unsigned int i = 0; i < lobbyState.players.size(); i++) {
      if (lobbyState.players.at(i).id == action.playerId) { player = &lobbyState.players.at(i); break; }
    }

    if (player) {
      if (action.type == e_NetLobbyAction_SetSide && lobbyState.phase == e_NetLobbyPhase_Sides) {
        int side = action.side;
        if (side < 0) side = 0;
        if (side > 2) side = 2;
        player->side = side;
        player->ready = false;
        changed = true;
      } else if (action.type == e_NetLobbyAction_SetReady) {
        player->ready = (action.value != 0);
        changed = true;
      } else if (action.type == e_NetLobbyAction_MoveCursor) {
        if (action.side >= 0 && action.side < 2 && lobbyState.chooser[action.side] == player->id) {
          lobbyState.teamCursor[action.side] = action.value;
          changed = true;
        }
      } else if (action.type == e_NetLobbyAction_CommitTeam) {
        if (action.side >= 0 && action.side < 2 && lobbyState.chooser[action.side] == player->id) {
          lobbyState.teamId[action.side] = action.value;
          changed = true;
        }
      } else if (action.type == e_NetLobbyAction_SetSelection) {
        if (action.side >= 0 && action.side < 2 && lobbyState.chooser[action.side] == player->id) {
          if (action.value == 0) {
            lobbyState.countryId[action.side] = action.value2;
            lobbyState.leagueId[action.side] = -1;
            lobbyState.teamId[action.side] = -1;
            lobbyState.teamReady[action.side] = false;
            changed = true;
          } else if (action.value == 1) {
            lobbyState.leagueId[action.side] = action.value2;
            lobbyState.teamId[action.side] = -1;
            lobbyState.teamReady[action.side] = false;
            changed = true;
          } else if (action.value == 2) {
            lobbyState.teamId[action.side] = action.value2;
            lobbyState.teamReady[action.side] = false;
            changed = true;
          }
        }
      } else if (action.type == e_NetLobbyAction_SetTeamReady) {
        if (action.side >= 0 && action.side < 2 && lobbyState.chooser[action.side] == player->id) {
          lobbyState.teamReady[action.side] = (action.value != 0);
          changed = true;
        }
      } else if (action.type == e_NetLobbyAction_SetDevice) {
        player->device = action.value;
        changed = true;
      } else if (action.type == e_NetLobbyAction_DeviceLost) {
        lobbyState.phase = e_NetLobbyPhase_Sides;
        for (unsigned int i = 0; i < lobbyState.players.size(); i++) lobbyState.players.at(i).ready = false;
        lobbyState.teamReady[0] = false;
        lobbyState.teamReady[1] = false;
        RecomputeChoppers();
        changed = true;
      } else if (action.type == e_NetLobbyAction_SetResumeReady) {
        player->resumeReady = (action.value != 0);
        changed = true;
      } else if (action.type == e_NetLobbyAction_SetMatchOptions) {
        // Host-only kickoff options; value selects the field, value2 is *1000.
        if (player->isHost) {
          float value = (float)action.value2 / 1000.0f;
          if (value < 0.0f) value = 0.0f;
          if (value > 1.0f) value = 1.0f;
          if (action.value == 0) lobbyState.matchDifficulty = value;
          else if (action.value == 1) lobbyState.matchDuration = value;
          changed = true;
        }
      } else if (action.type == e_NetLobbyAction_BackToTeams) {
        // Any peer may back out of the options screen: return to team selection
        // and clear both confirmations so it doesn't bounce straight back.
        if (lobbyState.phase == e_NetLobbyPhase_Options) {
          lobbyState.phase = e_NetLobbyPhase_Teams;
          lobbyState.teamReady[0] = false;
          lobbyState.teamReady[1] = false;
          changed = true;
        }
      } else if (action.type == e_NetLobbyAction_RequestSideSelect) {
        // Any peer may ask for (value != 0) or cancel (value == 0) in-match side
        // selection; the host mirrors it. Cancel resumes the match as-is.
        if (action.value != 0) {
          if (!lobbyState.sideSelect) {
            lobbyState.sideSelect = true;
            lobbyState.phase = e_NetLobbyPhase_Sides;
            for (unsigned int i = 0; i < lobbyState.players.size(); i++) {
              lobbyState.players.at(i).ready = false;
              lobbyState.players.at(i).resumeReady = false;
            }
            lobbyState.teamReady[0] = false;
            lobbyState.teamReady[1] = false;
            RecomputeChoppers();
            changed = true;
          }
        } else {
          if (lobbyState.sideSelect) {
            lobbyState.sideSelect = false;
            lobbyState.phase = e_NetLobbyPhase_Sides;
            for (unsigned int i = 0; i < lobbyState.players.size(); i++) {
              lobbyState.players.at(i).ready = false;
              lobbyState.players.at(i).resumeReady = false;
            }
            lobbyState.teamReady[0] = false;
            lobbyState.teamReady[1] = false;
            RecomputeChoppers();
            changed = true;
            boost::mutex::scoped_lock lock(resumeMutex);
            sideSelectCancelPending = true;
          }
        }
      }

      if (lobbyState.phase == e_NetLobbyPhase_Sides) {
        bool allReady = true;
        for (unsigned int i = 0; i < lobbyState.players.size(); i++) {
          if (!lobbyState.players.at(i).ready) { allReady = false; break; }
        }
        if (allReady && !lobbyState.sideSelect) {
          lobbyState.phase = e_NetLobbyPhase_Teams;
          RecomputeChoppers();
          changed = true;
        }
      }

      // Both sides picked and confirmed their teams: move to the kickoff
      // options screen (host sets AI difficulty / match duration).
      if (lobbyState.phase == e_NetLobbyPhase_Teams) {
        if (lobbyState.teamReady[0] && lobbyState.teamReady[1] &&
            lobbyState.teamId[0] > 0 && lobbyState.teamId[1] > 0) {
          lobbyState.phase = e_NetLobbyPhase_Options;
          changed = true;
        }
      }
    }

    if (player) RecomputeResumeReady();
    if (changed) lobbyState.revision++;
  }

  if (changed) {
    BroadcastLobbyState();
  }
}

void NetServer::RemoveConnection(boost::shared_ptr<NetServerConnection> connection) {
  boost::system::error_code error;
  connection->GetSocket().close(error);

  uint32_t playerId = connection->GetSessionId();

  // The team may still hold a raw pointer to this device and the game thread
  // keeps running until it detects the disconnect. Keep the device alive (with
  // all buttons released) until the host rebinds controllers, otherwise
  // Match::Process dereferences freed memory.
  boost::shared_ptr<NetHIDDevice> device = connection->GetHIDevice();
  if (device) device->Clear();

  {
    boost::mutex::scoped_lock lock(connectionsMutex);
    for (unsigned int i = 0; i < connections.size(); i++) {
      if (connections.at(i) == connection) {
        connections.erase(connections.begin() + i);
        break;
      }
    }
  }

  if (device) {
    boost::mutex::scoped_lock lock(retiredMutex);
    retiredDevices.push_back(device);
  }

  {
    boost::mutex::scoped_lock lock(udpMutex);
    udpEndpoints.erase(playerId);
    udpLastInputSeq.erase(playerId);
  }

  if (running.load() && playerId != 0) {
    {
      boost::mutex::scoped_lock lock(disconnectMutex);
      disconnectedPlayers.push_back(playerId);
    }
    RemovePlayer(playerId);
  }
}

void NetServer::RemovePlayer(uint32_t playerId) {
  {
    boost::mutex::scoped_lock lock(lobbyMutex);
    for (unsigned int i = 0; i < lobbyState.players.size(); i++) {
      if (lobbyState.players.at(i).id == playerId) {
        lobbyState.players.erase(lobbyState.players.begin() + i);
        break;
      }
    }
    // Sides/choosers may have shifted; everyone re-confirms.
    lobbyState.phase = e_NetLobbyPhase_Sides;
    for (unsigned int i = 0; i < lobbyState.players.size(); i++) lobbyState.players.at(i).ready = false;
    lobbyState.revision++;
    RecomputeChoppers();
    // If the player who left was the only one still refusing to resume, the
    // rest can continue now.
    RecomputeResumeReady();
  }

  BroadcastLobbyState();
}

void NetServer::BroadcastLobbyState() {
  NetBuffer buffer;
  {
    boost::mutex::scoped_lock lock(lobbyMutex);
    WriteLobbyState(buffer, lobbyState);
  }

  std::vector<boost::shared_ptr<NetServerConnection> > current;
  {
    boost::mutex::scoped_lock lock(connectionsMutex);
    current = connections;
  }

  for (unsigned int i = 0; i < current.size(); i++) {
    current.at(i)->SendMessage(e_NetMessage_LobbyState, buffer);
  }
}

void NetServer::RecomputeChoppers() {
  int hostSide = e_NetSide_Home;
  uint32_t hostId = 0;
  for (unsigned int i = 0; i < lobbyState.players.size(); i++) {
    const NetLobbyPlayer &player = lobbyState.players.at(i);
    if (player.isHost) {
      hostId = player.id;
      if (player.side == e_NetSide_Home || player.side == e_NetSide_Away) hostSide = player.side;
    }
  }

  lobbyState.chooser[hostSide] = hostId;

  int otherSide = 1 - hostSide;
  uint32_t otherChooser = hostId;
  for (unsigned int i = 0; i < lobbyState.players.size(); i++) {
    const NetLobbyPlayer &player = lobbyState.players.at(i);
    if (!player.isHost && player.side == otherSide) { otherChooser = player.id; break; }
  }
  lobbyState.chooser[otherSide] = otherChooser;
}

NetLobbyState NetServer::GetLobbyState() {
  boost::mutex::scoped_lock lock(lobbyMutex);
  return lobbyState;
}

void NetServer::SetCatalog(const std::vector<NetCatalogEntry> &catalog) {
  boost::mutex::scoped_lock lock(lobbyMutex);
  this->catalog = catalog;
}

std::vector<NetCatalogEntry> NetServer::GetCatalog() {
  boost::mutex::scoped_lock lock(lobbyMutex);
  return catalog;
}

void NetServer::BroadcastMessage(e_NetMessageType type, NetBuffer &body) {
  std::vector<boost::shared_ptr<NetServerConnection> > current;
  {
    boost::mutex::scoped_lock lock(connectionsMutex);
    current = connections;
  }

  for (unsigned int i = 0; i < current.size(); i++) {
    current.at(i)->SendMessage(type, body);
  }
}

void NetServer::SetHostName(const std::string &name) {
  {
    boost::mutex::scoped_lock lock(lobbyMutex);
    for (unsigned int i = 0; i < lobbyState.players.size(); i++) {
      if (lobbyState.players.at(i).isHost) { lobbyState.players.at(i).name = name; break; }
    }
    lobbyState.revision++;
  }
  BroadcastLobbyState();
}

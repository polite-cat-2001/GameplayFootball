#include "netclient.hpp"

#include <boost/bind.hpp>
#include <boost/make_shared.hpp>

#include <algorithm>
#include <chrono>

#include "netassets.hpp"
#include "netudp.hpp"

namespace {
const size_t kHeaderSize = 4;

unsigned long SteadyNow_ms() {
  return (unsigned long)std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}
}

NetClient::NetClient()
    : running(false),
      state(e_NetConnectionState_Disconnected),
      socket(ioContext),
      resolver(ioContext),
      playerId(0),
      matchStartPending(false),
      animationTablePending(false),
      environmentPending(false),
      pauseStatePending(false),
      pauseState(false),
      replayStopPending(false) {
}

NetClient::~NetClient() {
  Disconnect();
}

bool NetClient::Connect(const NetAddress &address) {
  if (running.load()) return true;

  this->address = address;
  running.store(true);
  state.store(e_NetConnectionState_Connecting);

  workGuard = boost::shared_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type> >(
      new boost::asio::executor_work_guard<boost::asio::io_context::executor_type>(boost::asio::make_work_guard(ioContext)));
  ioThread = boost::thread(boost::bind(&NetClient::Run, this));
  boost::asio::post(ioContext, boost::bind(&NetClient::DoConnect, this, address.ip, address.port));
  return true;
}

void NetClient::Disconnect() {
  if (!running.load()) return;

  running.store(false);
  if (pingTimer) {
    pingTimer->cancel();
  }
  boost::system::error_code error;
  socket.close(error);
  if (udpSocket) {
    // Close but keep the object alive: a posted DoSendUdp may still hold a copy.
    udpSocket->close(error);
  }
  hostUdpResolved = false;
  udpReady.store(false);
  if (workGuard) workGuard->reset();
  ioContext.stop();
  if (ioThread.joinable()) ioThread.join();
  ioContext.restart();
  state.store(e_NetConnectionState_Disconnected);
}

void NetClient::Run() {
  ioContext.run();
}

void NetClient::DoConnect(const std::string &ip, uint16_t port) {
  boost::system::error_code error;
  boost::asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(ip, std::to_string(port), error);
  if (error) { Fail(e_NetReject_Unknown, "could not resolve host"); return; }
  boost::asio::async_connect(socket, endpoints,
      boost::bind(&NetClient::HandleConnect, this, boost::asio::placeholders::error));
}

void NetClient::HandleConnect(const boost::system::error_code &error) {
  if (error) { Fail(e_NetReject_Unknown, "connection failed"); return; }
  lastPacketTime_ms.store(SteadyNow_ms());
  state.store(e_NetConnectionState_Handshaking);
  StartUdp();
  SendClientHello();
  StartPingTimer();
  ReadHeader();
}

void NetClient::StartUdp() {
  boost::system::error_code error;
  boost::asio::ip::tcp::endpoint remote = socket.remote_endpoint(error);
  if (error) return; // still handshaking; snapshots fall back to TCP

  udpSocket = boost::make_shared<boost::asio::ip::udp::socket>(ioContext);
  udpSocket->open(boost::asio::ip::udp::v4(), error);
  if (error) { udpSocket.reset(); return; }
  // Bind explicitly to an ephemeral port: async_receive_from on an unbound
  // socket is unreliable, and binding first also makes the endpoint stable
  // (learned by the host from our Hello).
  udpSocket->bind(boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0), error);
  if (error) { udpSocket->close(); udpSocket.reset(); return; }
  hostUdpEndpoint = boost::asio::ip::udp::endpoint(remote.address(), remote.port());
  hostUdpResolved = true;
  StartUdpReceive();
  SendUdpHello();
}

void NetClient::StartUdpReceive() {
  if (!udpSocket) return;
  udpRecvBuffer.resize(2048);
  udpSocket->async_receive_from(boost::asio::buffer(udpRecvBuffer), udpSenderEndpoint,
      boost::bind(&NetClient::HandleUdpReceive, this, boost::asio::placeholders::error,
                  boost::asio::placeholders::bytes_transferred));
}

void NetClient::HandleUdpReceive(const boost::system::error_code &error, std::size_t bytesTransferred) {
  if (!running.load()) return;
  if (error) { StartUdpReceive(); return; }

  uint8_t type = 0;
  uint32_t sessionId = 0, seq = 0;
  if (bytesTransferred > net_udpHeaderSize &&
      NetReadUdpHeader(udpRecvBuffer.data(), bytesTransferred, type, sessionId, seq) &&
      type == e_NetUdpType_Snapshot) {
    std::vector<uint8_t> payload(udpRecvBuffer.begin() + net_udpHeaderSize, udpRecvBuffer.begin() + bytesTransferred);
    // Snapshot payload starts with matchTime_ms then actualTime_ms (host clock).
    unsigned long hostTime = payload.size() >= 8 ? (unsigned long)NetReadU32LE(payload.data() + 4) : 0;
    boost::mutex::scoped_lock lock(pendingMutex);
    snapshotBuffer.push_back(NetRawSnapshot(SteadyNow_ms(), hostTime, payload));
    while (snapshotBuffer.size() > 64) snapshotBuffer.pop_front();
    udpReady.store(true);
  }

  StartUdpReceive();
}

void NetClient::SendUdpHello() {
  if (!udpSocket || !hostUdpResolved) return;
  boost::shared_ptr<std::vector<uint8_t> > packet = boost::make_shared<std::vector<uint8_t> >();
  NetWriteUdpHeader(*packet, e_NetUdpType_Hello, playerId.load(), 0);
  boost::asio::post(socket.get_executor(), boost::bind(&NetClient::DoSendUdp, this, packet));
}

void NetClient::DoSendUdp(boost::shared_ptr<std::vector<uint8_t> > packet) {
  if (!udpSocket || !hostUdpResolved) return;
  udpSocket->async_send_to(boost::asio::buffer(*packet), hostUdpEndpoint,
      [packet](const boost::system::error_code &, std::size_t) {
        (void)packet; // keeps the buffer alive until the async send completes
      });
}

void NetClient::StartPingTimer() {
  pingTimer = boost::make_shared<boost::asio::steady_timer>(ioContext);
  SchedulePingTimer();
}

void NetClient::SchedulePingTimer() {
  pingTimer->expires_after(boost::asio::chrono::milliseconds(net_keepaliveInterval_ms));
  pingTimer->async_wait(boost::bind(&NetClient::HandlePingTimer, this, boost::asio::placeholders::error));
}

void NetClient::HandlePingTimer(const boost::system::error_code &error) {
  if (error || !running.load()) return;

  SendPing();
  if (!udpReady.load()) SendUdpHello(); // keep trying until a UDP snapshot arrives

  if (SteadyNow_ms() - lastPacketTime_ms.load() > (unsigned long)net_disconnectTimeout_ms) {
    Fail(e_NetReject_Unknown, "host timed out");
    return;
  }

  SchedulePingTimer();
}

void NetClient::SendPing() {
  NetKeepalive keepalive;
  keepalive.seq = ++pingSeq;
  keepalive.echo = lastReceivedSeq;
  pingSent[keepalive.seq] = SteadyNow_ms();
  if (pingSent.size() > 64) pingSent.erase(pingSent.begin());

  NetBuffer body;
  WriteKeepalive(body, keepalive);
  SendMessage(e_NetMessage_Keepalive, body);
}

// Reply immediately; seq 0 marks a pong and never triggers another reply.
void NetClient::SendPong(uint32_t echoSeq) {
  NetKeepalive keepalive;
  keepalive.seq = 0;
  keepalive.echo = echoSeq;
  NetBuffer body;
  WriteKeepalive(body, keepalive);
  SendMessage(e_NetMessage_Keepalive, body);
}

void NetClient::HandleKeepalive(const NetKeepalive &keepalive) {
  lastPacketTime_ms.store(SteadyNow_ms());
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

void NetClient::SendClientHello() {
  NetClientHello hello;
  hello.protocolVersion = net_protocolVersion;
  hello.buildHash = NetGetBuildHash();
  hello.dataVersion = NetGetDataVersion();
  hello.dataHash = NetGetDataHash();
  hello.animationHash = NetGetAnimationHash();
  hello.playerName = playerName.empty() ? "player" : playerName;
  hello.sessionId = 0;

  NetBuffer body;
  WriteClientHello(body, hello);
  SendMessage(e_NetMessage_ClientHello, body);
}

void NetClient::SendMessage(e_NetMessageType type, NetBuffer &body) {
  boost::shared_ptr<std::vector<uint8_t> > payload = boost::make_shared<std::vector<uint8_t> >();
  payload->push_back((uint8_t)type);
  payload->insert(payload->end(), body.Data().begin(), body.Data().end());

  boost::shared_ptr<std::vector<uint8_t> > packet = boost::make_shared<std::vector<uint8_t> >();
  packet->resize(kHeaderSize + payload->size());
  NetWriteU32LE(&packet->at(0), (uint32_t)payload->size());
  std::copy(payload->begin(), payload->end(), packet->begin() + kHeaderSize);

  boost::asio::post(socket.get_executor(), boost::bind(&NetClient::Enqueue, this, packet));
}

void NetClient::Enqueue(boost::shared_ptr<std::vector<uint8_t> > packet) {
  bool writeInProgress = !writeQueue.empty();
  writeQueue.push_back(packet);
  if (!writeInProgress) DoWrite();
}

void NetClient::SendLobbyAction(const NetLobbyAction &action) {
  NetBuffer body;
  WriteLobbyAction(body, action);
  SendMessage(e_NetMessage_LobbyAction, body);
}

void NetClient::SendInputFrame(const NetInputFrame &frame) {
  NetBuffer body;
  WriteInputFrame(body, frame);

  if (udpSocket && hostUdpResolved) {
    boost::shared_ptr<std::vector<uint8_t> > packet = boost::make_shared<std::vector<uint8_t> >();
    NetWriteUdpHeader(*packet, e_NetUdpType_Input, playerId.load(), ++inputSeq);
    packet->insert(packet->end(), body.Data().begin(), body.Data().end());
    boost::asio::post(socket.get_executor(), boost::bind(&NetClient::DoSendUdp, this, packet));
  }
  // Until the first UDP snapshot confirms the channel, also feed the reliable
  // TCP path so input is never lost to a firewall dropping UDP.
  if (!udpReady.load()) SendMessage(e_NetMessage_InputFrame, body);
}

void NetClient::SendPauseRequest(bool paused) {
  NetBuffer body;
  body.PutBool(paused);
  SendMessage(e_NetMessage_PauseRequest, body);
}

void NetClient::SendReplayStop() {
  NetBuffer body;
  SendMessage(e_NetMessage_ReplayStop, body);
}

bool NetClient::ConsumeReplayStop() {
  boost::mutex::scoped_lock lock(pendingMutex);
  if (!replayStopPending) return false;
  replayStopPending = false;
  return true;
}

void NetClient::ReadHeader() {
  boost::asio::async_read(socket, boost::asio::buffer(header, kHeaderSize),
      boost::bind(&NetClient::HandleHeader, this, boost::asio::placeholders::error));
}

void NetClient::HandleHeader(const boost::system::error_code &error) {
  if (error) { Fail(e_NetReject_Unknown, "connection lost"); return; }
  uint32_t length = NetReadU32LE(header);
  if (length == 0 || length > 1024 * 1024) { Fail(e_NetReject_Unknown, "invalid message"); return; }
  body.resize(length);
  boost::asio::async_read(socket, boost::asio::buffer(body),
      boost::bind(&NetClient::HandleBody, this, boost::asio::placeholders::error));
}

void NetClient::HandleBody(const boost::system::error_code &error) {
  if (error) { Fail(e_NetReject_Unknown, "connection lost"); return; }
  lastPacketTime_ms.store(SteadyNow_ms());
  if (!body.empty()) {
    e_NetMessageType type = (e_NetMessageType)body[0];
    NetBuffer buffer;
    buffer.Data().assign(body.begin() + 1, body.end());
    buffer.ResetRead();
    Dispatch(type, buffer);
  }
  if (state.load() != e_NetConnectionState_Disconnected) ReadHeader();
}

void NetClient::Dispatch(e_NetMessageType type, NetBuffer &buffer) {
  if (type == e_NetMessage_ServerHello) {
    NetServerHello hello = ReadServerHello(buffer);
    {
      boost::mutex::scoped_lock lock(stateMutex);
      serverHello = hello;
    }
    playerId.store(hello.sessionId);
    state.store(hello.accepted ? e_NetConnectionState_Connected : e_NetConnectionState_Disconnected);
    if (!hello.accepted) {
      boost::system::error_code error;
      socket.close(error);
    }
  } else if (type == e_NetMessage_LobbyState) {
    boost::mutex::scoped_lock lock(stateMutex);
    lobbyState = ReadLobbyState(buffer);
  } else if (type == e_NetMessage_Catalog) {
    boost::mutex::scoped_lock lock(stateMutex);
    catalog = ReadCatalog(buffer);
  } else if (type == e_NetMessage_MatchSetup) {
    boost::mutex::scoped_lock lock(pendingMutex);
    matchSetup = ReadMatchSetup(buffer);
    matchStartPending = true;
  } else if (type == e_NetMessage_AnimationTable) {
    boost::mutex::scoped_lock lock(pendingMutex);
    animationTable = ReadAnimationTable(buffer);
    animationTablePending = true;
  } else if (type == e_NetMessage_Snapshot) {
    // TCP-fallback snapshots (before the host learned our UDP endpoint) share
    // the same queue as the UDP ones.
    const std::vector<uint8_t> &data = buffer.Data();
    unsigned long hostTime = data.size() >= 8 ? (unsigned long)NetReadU32LE(data.data() + 4) : 0;
    boost::mutex::scoped_lock lock(pendingMutex);
    snapshotBuffer.push_back(NetRawSnapshot(SteadyNow_ms(), hostTime, std::vector<uint8_t>(data.begin(), data.end())));
    while (snapshotBuffer.size() > 64) snapshotBuffer.pop_front();
  } else if (type == e_NetMessage_MatchEnvironment) {
    boost::mutex::scoped_lock lock(pendingMutex);
    environment = ReadMatchEnvironment(buffer);
    environmentPending = true;
  } else if (type == e_NetMessage_PauseState) {
    boost::mutex::scoped_lock lock(pendingMutex);
    pauseState = buffer.GetBool();
    pauseStatePending = true;
  } else if (type == e_NetMessage_ReplayStop) {
    boost::mutex::scoped_lock lock(pendingMutex);
    replayStopPending = true;
  } else if (type == e_NetMessage_Keepalive) {
    HandleKeepalive(ReadKeepalive(buffer));
  }
}

bool NetClient::ConsumeMatchSetup(NetMatchSetup &setup) {
  boost::mutex::scoped_lock lock(pendingMutex);
  if (!matchStartPending) return false;
  setup = matchSetup;
  matchStartPending = false;
  return true;
}

bool NetClient::ConsumeAnimationTable(std::vector<std::string> &names) {
  boost::mutex::scoped_lock lock(pendingMutex);
  if (!animationTablePending) return false;
  names = animationTable;
  animationTablePending = false;
  return true;
}

void NetClient::DrainSnapshots(std::deque<NetRawSnapshot> &out) {
  boost::mutex::scoped_lock lock(pendingMutex);
  out.insert(out.end(), snapshotBuffer.begin(), snapshotBuffer.end());
  snapshotBuffer.clear();
}

bool NetClient::ConsumeEnvironment(NetMatchEnvironment &out) {
  boost::mutex::scoped_lock lock(pendingMutex);
  if (!environmentPending) return false;
  out = environment;
  environmentPending = false;
  return true;
}

bool NetClient::ConsumePauseState(bool &out) {
  boost::mutex::scoped_lock lock(pendingMutex);
  if (!pauseStatePending) return false;
  out = pauseState;
  pauseStatePending = false;
  return true;
}

void NetClient::Fail(const e_NetRejectReason reason, const std::string &reasonText) {
  NetServerHello hello;
  hello.accepted = false;
  hello.reason = reason;
  hello.reasonText = reasonText;
  {
    boost::mutex::scoped_lock lock(stateMutex);
    serverHello = hello;
  }
  state.store(e_NetConnectionState_Disconnected);
}

NetServerHello NetClient::GetServerHello() const {
  boost::mutex::scoped_lock lock(stateMutex);
  return serverHello;
}

NetLobbyState NetClient::GetLobbyState() const {
  boost::mutex::scoped_lock lock(stateMutex);
  return lobbyState;
}

std::vector<NetCatalogEntry> NetClient::GetCatalog() const {
  boost::mutex::scoped_lock lock(stateMutex);
  return catalog;
}

void NetClient::DoWrite() {
  if (writeQueue.empty()) return;
  boost::asio::async_write(socket, boost::asio::buffer(*writeQueue.front()),
      boost::bind(&NetClient::HandleWrite, this, boost::asio::placeholders::error));
}

void NetClient::HandleWrite(const boost::system::error_code &error) {
  if (error) { Fail(e_NetReject_Unknown, "connection lost"); return; }
  writeQueue.pop_front();
  if (!writeQueue.empty()) DoWrite();
}

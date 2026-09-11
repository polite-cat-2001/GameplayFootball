#include "netclient.hpp"

#include <boost/bind.hpp>
#include <boost/make_shared.hpp>

#include <algorithm>
#include <chrono>

#include "netassets.hpp"

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
      snapshotPending(false),
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
  SendClientHello();
  StartPingTimer();
  ReadHeader();
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
  SendMessage(e_NetMessage_InputFrame, body);
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
    serverHello = ReadServerHello(buffer);
    playerId = serverHello.sessionId;
    state.store(serverHello.accepted ? e_NetConnectionState_Connected : e_NetConnectionState_Disconnected);
    if (!serverHello.accepted) {
      boost::system::error_code error;
      socket.close(error);
    }
    sig_OnHandshake(serverHello);
  } else if (type == e_NetMessage_LobbyState) {
    lobbyState = ReadLobbyState(buffer);
    sig_OnLobbyState(lobbyState);
  } else if (type == e_NetMessage_Catalog) {
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
    boost::mutex::scoped_lock lock(pendingMutex);
    snapshot.assign(buffer.Data().begin(), buffer.Data().end());
    snapshotPending = true;
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

bool NetClient::ConsumeSnapshot(std::vector<uint8_t> &bytes) {
  boost::mutex::scoped_lock lock(pendingMutex);
  if (!snapshotPending) return false;
  bytes = snapshot;
  snapshotPending = false;
  return true;
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
  serverHello = NetServerHello();
  serverHello.accepted = false;
  serverHello.reason = reason;
  serverHello.reasonText = reasonText;
  state.store(e_NetConnectionState_Disconnected);
  sig_OnHandshake(serverHello);
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

#include "netclient.hpp"

#include <boost/bind.hpp>
#include <boost/make_shared.hpp>

#include <algorithm>

#include "netassets.hpp"

namespace {
const size_t kHeaderSize = 4;
}

NetClient::NetClient()
    : running(false),
      state(e_NetConnectionState_Disconnected),
      socket(ioContext),
      resolver(ioContext) {
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
  state.store(e_NetConnectionState_Handshaking);
  SendClientHello();
  ReadHeader();
}

void NetClient::SendClientHello() {
  NetClientHello hello;
  hello.protocolVersion = net_protocolVersion;
  hello.buildHash = NetGetBuildHash();
  hello.dataVersion = NetGetDataVersion();
  hello.dataHash = NetGetDataHash();
  hello.animationHash = NetGetAnimationHash();
  hello.playerName = "player";
  hello.sessionId = 0;

  NetBuffer body;
  WriteClientHello(body, hello);

  boost::shared_ptr<std::vector<uint8_t> > payload = boost::make_shared<std::vector<uint8_t> >();
  payload->push_back((uint8_t)e_NetMessage_ClientHello);
  payload->insert(payload->end(), body.Data().begin(), body.Data().end());

  boost::shared_ptr<std::vector<uint8_t> > packet = boost::make_shared<std::vector<uint8_t> >();
  packet->resize(kHeaderSize + payload->size());
  NetWriteU32LE(&packet->at(0), (uint32_t)payload->size());
  std::copy(payload->begin(), payload->end(), packet->begin() + kHeaderSize);

  bool writeInProgress = !writeQueue.empty();
  writeQueue.push_back(packet);
  if (!writeInProgress) DoWrite();
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
  if (!body.empty()) {
    e_NetMessageType type = (e_NetMessageType)body[0];
    NetBuffer buffer;
    buffer.Data().assign(body.begin() + 1, body.end());
    buffer.ResetRead();
    Dispatch(type, buffer);
  }
  if (state.load() == e_NetConnectionState_Handshaking) ReadHeader();
}

void NetClient::Dispatch(e_NetMessageType type, NetBuffer &buffer) {
  if (type == e_NetMessage_ServerHello) {
    serverHello = ReadServerHello(buffer);
    state.store(serverHello.accepted ? e_NetConnectionState_Connected : e_NetConnectionState_Disconnected);
    if (!serverHello.accepted) {
      boost::system::error_code error;
      socket.close(error);
    }
    sig_OnHandshake(serverHello);
  }
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

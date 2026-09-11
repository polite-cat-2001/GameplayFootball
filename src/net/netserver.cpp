#include "netserver.hpp"

#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/make_shared.hpp>

#include <algorithm>
#include <deque>

#include "netassets.hpp"

namespace {
const size_t kHeaderSize = 4;
}

class NetServerConnection : public boost::enable_shared_from_this<NetServerConnection> {

  public:
    NetServerConnection(boost::asio::io_context &ioContext, NetServer *server)
        : socket(ioContext), server(server) {}

    boost::asio::ip::tcp::socket &GetSocket() { return socket; }

    void Start() { ReadHeader(); }

    void SendMessage(e_NetMessageType type, NetBuffer &body) {
      boost::shared_ptr<std::vector<uint8_t> > payload = boost::make_shared<std::vector<uint8_t> >();
      payload->push_back((uint8_t)type);
      const std::vector<uint8_t> &bodyData = body.Data();
      payload->insert(payload->end(), bodyData.begin(), bodyData.end());

      boost::shared_ptr<std::vector<uint8_t> > packet = boost::make_shared<std::vector<uint8_t> >();
      packet->resize(kHeaderSize + payload->size());
      NetWriteU32LE(&packet->at(0), (uint32_t)payload->size());
      std::copy(payload->begin(), payload->end(), packet->begin() + kHeaderSize);

      bool writeInProgress = !writeQueue.empty();
      writeQueue.push_back(packet);
      if (!writeInProgress) DoWrite();
    }

  private:
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
    uint8_t header[kHeaderSize];
    std::vector<uint8_t> body;
    std::deque<boost::shared_ptr<std::vector<uint8_t> > > writeQueue;
};

NetServer::NetServer(uint16_t port) : port(port), running(false), nextSessionId(1) {
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

  workGuard = boost::shared_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type> >(
      new boost::asio::executor_work_guard<boost::asio::io_context::executor_type>(boost::asio::make_work_guard(ioContext)));
  running.store(true);
  DoAccept();
  ioThread = boost::thread(boost::bind(&NetServer::Run, this));
  return true;
}

void NetServer::Stop() {
  if (!running.load()) return;

  running.store(false);

  if (acceptor) {
    boost::system::error_code error;
    acceptor->close(error);
  }

  {
    boost::mutex::scoped_lock lock(connectionsMutex);
    for (unsigned int i = 0; i < connections.size(); i++) {
      boost::system::error_code error;
      connections.at(i)->GetSocket().close(error);
    }
    connections.clear();
  }

  if (workGuard) workGuard->reset();
  ioContext.stop();
  if (ioThread.joinable()) ioThread.join();
  ioContext.restart();
}

void NetServer::Run() {
  ioContext.run();
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
  }

  response.accepted = (response.reason == e_NetReject_None);

  NetBuffer buffer;
  WriteServerHello(buffer, response);
  connection->SendMessage(e_NetMessage_ServerHello, buffer);

  sig_OnHandshake(hello, response);
}

void NetServer::RemoveConnection(boost::shared_ptr<NetServerConnection> connection) {
  boost::system::error_code error;
  connection->GetSocket().close(error);

  boost::mutex::scoped_lock lock(connectionsMutex);
  for (unsigned int i = 0; i < connections.size(); i++) {
    if (connections.at(i) == connection) {
      connections.erase(connections.begin() + i);
      break;
    }
  }
}

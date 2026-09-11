#include "netserver.hpp"

#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/make_shared.hpp>

#include <algorithm>
#include <deque>

#include "netassets.hpp"

#include "base/utils.hpp"

namespace {
const size_t kHeaderSize = 4;
}

class NetServerConnection : public boost::enable_shared_from_this<NetServerConnection> {

  public:
    NetServerConnection(boost::asio::io_context &ioContext, NetServer *server)
        : socket(ioContext), server(server), sessionId(0) {}

    boost::asio::ip::tcp::socket &GetSocket() { return socket; }

    uint32_t GetSessionId() const { return sessionId; }
    void SetSessionId(uint32_t id) { sessionId = id; }

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

  sig_OnHandshake(hello, response);
}

void NetServer::HandleLobbyAction(boost::shared_ptr<NetServerConnection> connection, const NetLobbyAction &action) {
  NetLobbyAction attributed = action;
  attributed.playerId = connection->GetSessionId();
  ApplyLobbyAction(attributed);
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
      }

      if (lobbyState.phase == e_NetLobbyPhase_Sides) {
        bool allReady = true;
        for (unsigned int i = 0; i < lobbyState.players.size(); i++) {
          if (!lobbyState.players.at(i).ready) { allReady = false; break; }
        }
        if (allReady) {
          lobbyState.phase = e_NetLobbyPhase_Teams;
          RecomputeChoppers();
          changed = true;
        }
      }
    }

    if (changed) lobbyState.revision++;
  }

  if (changed) {
    BroadcastLobbyState();
    sig_OnLobbyState(GetLobbyState());
  }
}

void NetServer::RemoveConnection(boost::shared_ptr<NetServerConnection> connection) {
  boost::system::error_code error;
  connection->GetSocket().close(error);

  uint32_t playerId = connection->GetSessionId();

  {
    boost::mutex::scoped_lock lock(connectionsMutex);
    for (unsigned int i = 0; i < connections.size(); i++) {
      if (connections.at(i) == connection) {
        connections.erase(connections.begin() + i);
        break;
      }
    }
  }

  if (running.load() && playerId != 0) {
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
  }

  BroadcastLobbyState();
  sig_OnLobbyState(GetLobbyState());
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

void NetServer::SetHostName(const std::string &name) {
  {
    boost::mutex::scoped_lock lock(lobbyMutex);
    for (unsigned int i = 0; i < lobbyState.players.size(); i++) {
      if (lobbyState.players.at(i).isHost) { lobbyState.players.at(i).name = name; break; }
    }
    lobbyState.revision++;
  }
  BroadcastLobbyState();
  sig_OnLobbyState(GetLobbyState());
}

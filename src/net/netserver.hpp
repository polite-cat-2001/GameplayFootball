#ifndef _HPP_NETSERVER
#define _HPP_NETSERVER

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/signals2.hpp>
#include <boost/thread.hpp>

#include <atomic>
#include <string>
#include <vector>

#include "netmessages.hpp"
#include "nettypes.hpp"

class NetServerConnection;
class NetHIDDevice;

class NetServer {

  public:
    explicit NetServer(uint16_t port);
    ~NetServer();

    bool Start();
    void Stop();
    bool IsRunning() const { return running.load(); }
    uint16_t GetPort() const { return port; }

    NetLobbyState GetLobbyState();
    void ApplyLobbyAction(const NetLobbyAction &action);

    void SetCatalog(const std::vector<NetCatalogEntry> &catalog);
    std::vector<NetCatalogEntry> GetCatalog();
    void SetHostName(const std::string &name);

    // Reliable broadcast on the control channel. Safe to call from any thread.
    void BroadcastMessage(e_NetMessageType type, NetBuffer &body);

    // One virtual input device per connected client, created on handshake.
    boost::shared_ptr<NetHIDDevice> GetHIDevice(uint32_t sessionId);
    std::vector<boost::shared_ptr<NetHIDDevice> > GetHIDevices();

    // Any peer may request pause; the host applies and broadcasts the state.
    void BroadcastPause(bool paused);
    bool ConsumePauseRequest(bool &paused);

    // Any peer skipping a replay tells the host, which broadcasts it to all.
    void BroadcastReplayStop();
    bool ConsumeReplayStop();

    boost::signals2::signal<void(const NetClientHello &, const NetServerHello &)> sig_OnHandshake;
    boost::signals2::signal<void(const NetLobbyState &)> sig_OnLobbyState;

  private:
    friend class NetServerConnection;

    void Run();
    void DoAccept();
    void HandleAccept(boost::shared_ptr<NetServerConnection> connection, const boost::system::error_code &error);
    void HandleClientHello(boost::shared_ptr<NetServerConnection> connection, const NetClientHello &hello);
    void HandleLobbyAction(boost::shared_ptr<NetServerConnection> connection, const NetLobbyAction &action);
    void HandleInputFrame(boost::shared_ptr<NetServerConnection> connection, const NetInputFrame &frame);
    void HandlePauseRequest(bool paused);
    void HandleReplayStop();
    void RemoveConnection(boost::shared_ptr<NetServerConnection> connection);
    void RemovePlayer(uint32_t playerId);
    void BroadcastLobbyState();
    void RecomputeChoppers();

    uint16_t port;
    std::atomic<bool> running;
    boost::asio::io_context ioContext;
    boost::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor;
    boost::shared_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type> > workGuard;
    boost::thread ioThread;

    boost::mutex connectionsMutex;
    std::vector<boost::shared_ptr<NetServerConnection> > connections;
    uint32_t nextSessionId;

    boost::mutex lobbyMutex;
    NetLobbyState lobbyState;
    std::vector<NetCatalogEntry> catalog;

    boost::mutex pauseMutex;
    bool pauseRequestPending;
    bool pauseRequestState;

    boost::mutex replayMutex;
    bool replayStopPending;
};

#endif

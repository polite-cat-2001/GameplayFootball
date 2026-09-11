#ifndef _HPP_NETSERVER
#define _HPP_NETSERVER

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
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

    // Fairness: max one-way delay estimate (RTT/2) over all connected clients,
    // used to delay the host's own input so no peer gains a ping advantage.
    int GetMaxClientRtt_ms();

    // A client vanished (socket error or keepalive timeout). Consumed by the
    // game task so a match in progress can pause and re-open side selection.
    bool ConsumeDisconnectedPlayer(uint32_t &playerId);

    // A client finished the handshake. Consumed by the game task so a match in
    // progress can pause and hand the newcomer the current match state.
    bool ConsumeJoinedPlayer(uint32_t &playerId);

    // Drop pending join/disconnect events (called when a match starts, so lobby
    // churn never triggers a spurious mid-match pause).
    void ClearRosterEvents();

    // A disconnected client's HID device must outlive the connection until the
    // host has re-bound controllers (the team holds a raw IHIDevice*). These
    // kept-alive devices are dropped once rebinding is done.
    void ClearRetiredDevices();

    // In-match side selection mode: keeps the lobby in the Sides phase.
    void SetSideSelectMode(bool on);

    // Resume vote: while paused every peer votes to continue; the match resumes
    // once all of them have. Reset when a new pause begins.
    void ResetResumeVotes();
    bool ConsumeAllResumeReady();

    // A peer left side selection; the host applies sides and stays paused.
    bool ConsumeSideSelectCancel();

    // Send one control message to a single peer (host -> one client).
    void SendToPlayer(uint32_t playerId, e_NetMessageType type, NetBuffer &body);

  private:
    friend class NetServerConnection;

    void Run();
    void StartPingTimer();
    void SchedulePingTimer();
    void HandlePingTimer(const boost::system::error_code &error);
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
    void RecomputeResumeReady(); // caller holds lobbyMutex

    uint16_t port;
    std::atomic<bool> running;
    boost::asio::io_context ioContext;
    boost::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor;
    boost::shared_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type> > workGuard;
    boost::thread ioThread;
    boost::shared_ptr<boost::asio::steady_timer> pingTimer;

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

    boost::mutex resumeMutex;
    bool allResumeReadyPending;
    bool sideSelectCancelPending;

    boost::mutex disconnectMutex;
    std::vector<uint32_t> disconnectedPlayers;
    std::vector<uint32_t> joinedPlayers;

    boost::mutex retiredMutex;
    std::vector<boost::shared_ptr<NetHIDDevice> > retiredDevices;
};

#endif

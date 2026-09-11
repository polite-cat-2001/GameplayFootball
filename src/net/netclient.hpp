#ifndef _HPP_NETCLIENT
#define _HPP_NETCLIENT

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/signals2.hpp>
#include <boost/thread.hpp>

#include <atomic>
#include <deque>
#include <map>
#include <vector>

#include "netmessages.hpp"
#include "nettypes.hpp"

class NetClient {

  public:
    NetClient();
    ~NetClient();

    bool Connect(const NetAddress &address);
    void Disconnect();
    bool IsRunning() const { return running.load(); }
    e_NetConnectionState GetState() const { return state.load(); }
    const NetServerHello &GetServerHello() const { return serverHello; }
    const NetLobbyState &GetLobbyState() const { return lobbyState; }
    const std::vector<NetCatalogEntry> &GetCatalog() const { return catalog; }
    uint32_t GetPlayerId() const { return playerId; }

    // Round-trip time to the host (-1 until the first ping is echoed).
    int GetRtt_ms() const { return rtt_ms.load(); }

    void SetPlayerName(const std::string &name) { playerName = name; }

    void SendLobbyAction(const NetLobbyAction &action);
    void SendInputFrame(const NetInputFrame &frame);
    void SendPauseRequest(bool paused);
    void SendReplayStop();

    // Match startup and realtime snapshot intake. All of these are one-shot
    // "consume" reads so the game thread can never lose an event by polling twice.
    bool ConsumeMatchSetup(NetMatchSetup &setup);
    bool ConsumeAnimationTable(std::vector<std::string> &names);
    bool ConsumeSnapshot(std::vector<uint8_t> &bytes);
    bool ConsumeEnvironment(NetMatchEnvironment &environment);
    bool ConsumePauseState(bool &paused);
    bool ConsumeReplayStop();

    boost::signals2::signal<void(const NetServerHello &)> sig_OnHandshake;
    boost::signals2::signal<void(const NetLobbyState &)> sig_OnLobbyState;

  private:
    void Run();
    void StartPingTimer();
    void SchedulePingTimer();
    void HandlePingTimer(const boost::system::error_code &error);
    void SendPing();
    void SendPong(uint32_t echoSeq);
    void HandleKeepalive(const NetKeepalive &keepalive);
    void DoConnect(const std::string &ip, uint16_t port);
    void HandleConnect(const boost::system::error_code &error);
    void SendClientHello();
    void SendMessage(e_NetMessageType type, NetBuffer &body);
    void Enqueue(boost::shared_ptr<std::vector<uint8_t> > packet);
    void ReadHeader();
    void HandleHeader(const boost::system::error_code &error);
    void HandleBody(const boost::system::error_code &error);
    void Dispatch(e_NetMessageType type, NetBuffer &buffer);
    void Fail(const e_NetRejectReason reason, const std::string &reasonText);
    void DoWrite();
    void HandleWrite(const boost::system::error_code &error);

    NetAddress address;
    std::atomic<bool> running;
    std::atomic<e_NetConnectionState> state;
    boost::asio::io_context ioContext;
    boost::asio::ip::tcp::socket socket;
    boost::asio::ip::tcp::resolver resolver;
    boost::shared_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type> > workGuard;
    boost::thread ioThread;
    boost::shared_ptr<boost::asio::steady_timer> pingTimer;

    std::atomic<int> rtt_ms{-1};
    std::atomic<unsigned long> lastPacketTime_ms{0};
    uint32_t pingSeq = 0;
    uint32_t lastReceivedSeq = 0;
    std::map<uint32_t, unsigned long> pingSent;

    uint8_t header[4];
    std::vector<uint8_t> body;
    std::deque<boost::shared_ptr<std::vector<uint8_t> > > writeQueue;

    NetServerHello serverHello;
    NetLobbyState lobbyState;
    std::vector<NetCatalogEntry> catalog;
    std::string playerName;
    uint32_t playerId;

    boost::mutex pendingMutex;
    bool matchStartPending;
    NetMatchSetup matchSetup;
    bool animationTablePending;
    std::vector<std::string> animationTable;
    bool snapshotPending;
    std::vector<uint8_t> snapshot;
    bool environmentPending;
    NetMatchEnvironment environment;
    bool pauseStatePending;
    bool pauseState;
    bool replayStopPending;
};

#endif

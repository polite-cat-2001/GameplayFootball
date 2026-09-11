#ifndef _HPP_NETCLIENT
#define _HPP_NETCLIENT

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/signals2.hpp>
#include <boost/thread.hpp>

#include <atomic>
#include <deque>
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

    boost::signals2::signal<void(const NetServerHello &)> sig_OnHandshake;

  private:
    void Run();
    void DoConnect(const std::string &ip, uint16_t port);
    void HandleConnect(const boost::system::error_code &error);
    void SendClientHello();
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

    uint8_t header[4];
    std::vector<uint8_t> body;
    std::deque<boost::shared_ptr<std::vector<uint8_t> > > writeQueue;

    NetServerHello serverHello;
};

#endif

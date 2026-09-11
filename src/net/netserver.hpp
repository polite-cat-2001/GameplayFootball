#ifndef _HPP_NETSERVER
#define _HPP_NETSERVER

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/signals2.hpp>
#include <boost/thread.hpp>

#include <atomic>
#include <vector>

#include "netmessages.hpp"
#include "nettypes.hpp"

class NetServerConnection;

class NetServer {

  public:
    explicit NetServer(uint16_t port);
    ~NetServer();

    bool Start();
    void Stop();
    bool IsRunning() const { return running.load(); }
    uint16_t GetPort() const { return port; }

    boost::signals2::signal<void(const NetClientHello &, const NetServerHello &)> sig_OnHandshake;

  private:
    friend class NetServerConnection;

    void Run();
    void DoAccept();
    void HandleAccept(boost::shared_ptr<NetServerConnection> connection, const boost::system::error_code &error);
    void HandleClientHello(boost::shared_ptr<NetServerConnection> connection, const NetClientHello &hello);
    void RemoveConnection(boost::shared_ptr<NetServerConnection> connection);

    uint16_t port;
    std::atomic<bool> running;
    boost::asio::io_context ioContext;
    boost::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor;
    boost::shared_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type> > workGuard;
    boost::thread ioThread;

    boost::mutex connectionsMutex;
    std::vector<boost::shared_ptr<NetServerConnection> > connections;
    uint32_t nextSessionId;
};

#endif

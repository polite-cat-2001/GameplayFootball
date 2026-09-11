#ifndef _HPP_NETSERVER
#define _HPP_NETSERVER

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

#include <atomic>

#include "nettypes.hpp"

class NetServer {

  public:
    explicit NetServer(uint16_t port);
    ~NetServer();

    bool Start();
    void Stop();
    bool IsRunning() const { return running.load(); }
    uint16_t GetPort() const { return port; }

  private:
    void Run();

    uint16_t port;
    std::atomic<bool> running;
    boost::asio::io_context ioContext;
    boost::shared_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type> > workGuard;
    boost::thread ioThread;
};

#endif

#ifndef _HPP_NETCLIENT
#define _HPP_NETCLIENT

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

#include <atomic>

#include "nettypes.hpp"

class NetClient {

  public:
    NetClient();
    ~NetClient();

    bool Connect(const NetAddress &address);
    void Disconnect();
    bool IsRunning() const { return running.load(); }

  private:
    void Run();

    NetAddress address;
    std::atomic<bool> running;
    boost::asio::io_context ioContext;
    boost::shared_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type> > workGuard;
    boost::thread ioThread;
};

#endif

#include "netserver.hpp"

#include <boost/bind.hpp>

NetServer::NetServer(uint16_t port) : port(port), running(false) {
}

NetServer::~NetServer() {
  Stop();
}

bool NetServer::Start() {
  if (running.load()) return true;

  workGuard = boost::shared_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type> >(
      new boost::asio::executor_work_guard<boost::asio::io_context::executor_type>(boost::asio::make_work_guard(ioContext)));
  running.store(true);
  ioThread = boost::thread(boost::bind(&NetServer::Run, this));
  return true;
}

void NetServer::Stop() {
  if (!running.load()) return;

  running.store(false);
  if (workGuard) workGuard->reset();
  ioContext.stop();
  if (ioThread.joinable()) ioThread.join();
  ioContext.restart();
}

void NetServer::Run() {
  ioContext.run();
}

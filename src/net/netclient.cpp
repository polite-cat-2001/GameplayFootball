#include "netclient.hpp"

#include <boost/bind.hpp>

NetClient::NetClient() : running(false) {
}

NetClient::~NetClient() {
  Disconnect();
}

bool NetClient::Connect(const NetAddress &address) {
  if (running.load()) return true;

  this->address = address;
  workGuard = boost::shared_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type> >(
      new boost::asio::executor_work_guard<boost::asio::io_context::executor_type>(boost::asio::make_work_guard(ioContext)));
  running.store(true);
  ioThread = boost::thread(boost::bind(&NetClient::Run, this));
  return true;
}

void NetClient::Disconnect() {
  if (!running.load()) return;

  running.store(false);
  if (workGuard) workGuard->reset();
  ioContext.stop();
  if (ioThread.joinable()) ioThread.join();
  ioContext.restart();
}

void NetClient::Run() {
  ioContext.run();
}

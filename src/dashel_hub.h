#ifndef PYASEBA_DASHEL_HUB_H_GUARD
#define PYASEBA_DASHEL_HUB_H_GUARD

#include "aseba/common/msg/msg.h"
#include "aseba/transport/dashel_plugins/dashel-plugins.h"
#include "dashel/dashel.h"

#include <memory>
#include <thread>

struct PyNodesManager;

struct DashelHub : public Dashel::Hub {
  std::unique_ptr<std::thread> thread;
  std::unique_ptr<std::thread> ping_thread;
  std::atomic_bool stop_ping_thread;
  PyNodesManager *nm;
  DashelHub(PyNodesManager *manager) : Dashel::Hub(), thread(), ping_thread(), stop_ping_thread(false), nm(manager) {
    Dashel::initPlugins();
  }
  void sendMessage(const Aseba::Message *message,
                   Dashel::Stream *sourceStream = nullptr) {
    for (auto stream : dataStreams) {
      message->serialize(stream);
      stream->flush();
    }
  }

  void ping();

  void start() {
    thread = std::make_unique<std::thread>(&Dashel::Hub::run, this);
    ping_thread = std::make_unique<std::thread>(&DashelHub::ping, this);
  }

  void stop() {
    Dashel::Hub::stop();
    thread->join();
    stop_ping_thread = true;
    ping_thread->join();
    stop_ping_thread = false;
    thread = nullptr;
    ping_thread = nullptr;
  }

  void connectionCreated(Dashel::Stream *stream) override;

  void incomingData(Dashel::Stream *stream) override;

  void connectionClosed(Dashel::Stream *stream, bool abnormal) override;
};

#endif

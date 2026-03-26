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
  PyNodesManager *nm;
  DashelHub(PyNodesManager *manager) : Dashel::Hub(), nm(manager) {
    Dashel::initPlugins();
  }
  void sendMessage(const Aseba::Message *message,
                   Dashel::Stream *sourceStream = nullptr) {
    for (auto stream : dataStreams) {
      message->serialize(stream);
      stream->flush();
    }
  }

  void run_and_ping();

  void start() {
    thread = std::make_unique<std::thread>(&DashelHub::run_and_ping, this);
  }

  void stop() {
    Dashel::Hub::stop();
    thread->join();
    thread = nullptr;
  }

  void connectionCreated(Dashel::Stream *stream) override;

  void incomingData(Dashel::Stream *stream) override;

  void connectionClosed(Dashel::Stream *stream, bool abnormal) override;
};

#endif

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
  explicit DashelHub(PyNodesManager *manager)
      : Dashel::Hub(), thread(nullptr), nm(manager) {
    Dashel::initPlugins();
  }
  void sendMessage(const Aseba::Message *message,
                   Dashel::Stream *sourceStream = nullptr) {
    bool need_lock =
        (!thread || std::this_thread::get_id() != thread->get_id());

    if (need_lock) {
      lock();
    }
    for (auto stream : dataStreams) {
      message->serialize(stream);
      stream->flush();
    }
    if (need_lock) {
      unlock();
    }
  }

  void start() {
    thread = std::make_unique<std::thread>(&Dashel::Hub::run, this);
  }

  void stop() {
    Dashel::Hub::stop();
    if (thread) {
      thread->join();
    }
    thread = nullptr;
  }

  void connectionCreated(Dashel::Stream *stream) override;

  void incomingData(Dashel::Stream *stream) override;

  void connectionClosed(Dashel::Stream *stream, bool abnormal) override;
};

#endif

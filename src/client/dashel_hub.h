#ifndef PYASEBA_DASHEL_HUB_H_GUARD
#define PYASEBA_DASHEL_HUB_H_GUARD

#include "aseba/common/msg/msg.h"
#include "aseba/transport/dashel_plugins/dashel-plugins.h"
#include "dashel/dashel.h"

#include <iostream>
#include <memory>
#include <thread>

struct PyNodesManager;

struct DashelHub : public Dashel::Hub {
  std::unique_ptr<std::thread> thread;
  PyNodesManager *nm;
  std::map<Dashel::Stream *, unsigned> stream_indices;
  Dashel::Stream *in_stream;
  unsigned stream_index;

  void add_stream(Dashel::Stream *stream) {
    if (stream_indices.count(stream) == 0) {
      stream_indices.emplace(stream, stream_index++);
    }
  }

  void remove_stream(Dashel::Stream *stream) {
    if (stream_indices.count(stream)) {
      stream_indices.erase(stream);
    }
  }

  explicit DashelHub(PyNodesManager *manager, int port = -1)
      : Dashel::Hub(true), thread(nullptr), nm(manager), stream_indices(),
        in_stream(nullptr), stream_index(0) {
    Dashel::initPlugins();
    if (port > 0) {
      in_stream = connect("tcpin:port=" + std::to_string(port));
      add_stream(in_stream);
    }
  }
  void sendMessage(const Aseba::Message *message, int stream_index = -1,
                   const std::set<unsigned> &exclude_stream_indices = {}) {
    bool need_lock =
        (!thread || std::this_thread::get_id() != thread->get_id());

    if (need_lock) {
      lock();
    }
    for (auto stream : dataStreams) {
      if (!stream_indices.count(stream)) {
        std::cerr << "Unindexed stream " << stream->getTargetName()
                  << std::endl;
        continue;
      }
      const auto i = stream_indices.at(stream);
      if (stream_index >= 0 && (i != stream_index)) {
        continue;
      }
      if (exclude_stream_indices.count(i)) {
        continue;
      }
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

  void close() {
    auto ss = dataStreams;
    for (auto stream : ss) {
      remove_stream(stream);
      closeStream(stream);
    }
  }

  Dashel::Stream *get_stream(unsigned index) const {
    for (auto [stream, i] : stream_indices) {
      if (index == i)
        return stream;
    }
    return nullptr;
  }

  std::string get_target_name(int index) const {
    auto stream = get_stream(index);
    if (stream) {
      return stream->getTargetName();
    }
    return "";
  }

  bool is_connected_to(int index) const {
    if (index < 0) {
      return dataStreams.size() > 0;
    }
    return get_stream(index);
  }

  bool is_connected() const { return dataStreams.size() > 0; }

  std::map<unsigned, std::string> get_targets() const {
    std::map<unsigned, std::string> targets;
    for (const auto [stream, index] : stream_indices) {
      targets[index] = stream->getTargetName();
    }
    return targets;
  }

  void connectionCreated(Dashel::Stream *stream) override;

  void incomingData(Dashel::Stream *stream) override;

  void connectionClosed(Dashel::Stream *stream, bool abnormal) override;
};

#endif

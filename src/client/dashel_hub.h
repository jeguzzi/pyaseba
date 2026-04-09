#ifndef PYASEBA_DASHEL_HUB_H_GUARD
#define PYASEBA_DASHEL_HUB_H_GUARD

#include "aseba/common/msg/msg.h"
#include "aseba/transport/dashel_plugins/dashel-plugins.h"
#include "dashel/dashel.h"

#ifdef ZEROCONF
#include "aseba/common/zeroconf/zeroconf-dashelhub.h"
#endif

#include <iostream>
#include <memory>
#include <thread>

// TODO: use common utils instead

inline std::string narrow(const wchar_t *src) {
  const size_t destSize(wcstombs(0, src, 0) + 1);
  std::vector<char> buffer(destSize, 0);
  wcstombs(&buffer[0], src, destSize);
  return std::string(buffer.begin(), buffer.end() - 1);
}

inline std::string narrow(const std::wstring &src) {
  return narrow(src.c_str());
}

using OutQueuedMessage =
    std::tuple<std::unique_ptr<Aseba::Message>, std::set<Dashel::Stream *>>;

struct PyNodesManager;

struct DashelHub : public Dashel::Hub {
  std::unique_ptr<std::thread> thread;
  PyNodesManager *nm;
  std::map<Dashel::Stream *, unsigned> stream_indices;
  Dashel::Stream *in_stream;
  unsigned stream_index;
  std::queue<OutQueuedMessage> out_msgs;
  std::mutex out_msgs_mutex;
  std::condition_variable out_msgs_cv;
  bool stopped;
  std::unique_ptr<std::thread> send_thread;

#ifdef ZEROCONF
  Aseba::DashelhubZeroconf zeroconf;
#endif

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
        in_stream(nullptr), stream_index(0), out_msgs(), out_msgs_mutex(),
        out_msgs_cv(), stopped(false), send_thread(nullptr)
#ifdef ZEROCONF
        ,
        zeroconf(*this)
#endif
  {
    Dashel::initPlugins();
    if (port > 0) {
      in_stream = connect("tcpin:port=" + std::to_string(port));
      add_stream(in_stream);
    }
  }

  void run_out_msgs() {
    while (!stopped) {
      std::unique_ptr<Aseba::Message> message;
      std::set<Dashel::Stream *> out_streams;
      {
        std::unique_lock<std::mutex> lck(out_msgs_mutex);
        out_msgs_cv.wait(lck, []() { return true; });
        if (!out_msgs.empty()) {
          // std::tie(message, streams) = out_msgs.front();
          auto &t = out_msgs.front();
          message = std::move(std::get<0>(t));
          out_streams = std::get<1>(t);
          out_msgs.pop();
        }
      }
      if (message) {
        lock();
        for (auto stream : out_streams) {
          message->serialize(stream);
          stream->flush();
        }
        unlock();
      }
    }
  }

  std::set<Dashel::Stream *>
  get_streams(int stream_index = -1,
              const std::set<unsigned> &exclude_stream_indices = {}) {
    std::set<Dashel::Stream *> out_streams;
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
      out_streams.insert(stream);
    }
    return out_streams;
  }

  void sendMessage(const Aseba::Message *message, int stream_index = -1,
                   const std::set<unsigned> &exclude_stream_indices = {}) {
    std::set<Dashel::Stream *> out_streams =
        get_streams(stream_index, exclude_stream_indices);
    if (out_streams.size()) {
      std::unique_ptr<Aseba::Message> msg;
      msg.reset(message->clone());
      std::unique_lock<std::mutex> lck(out_msgs_mutex);
      out_msgs.push({std::move(msg), out_streams});
      out_msgs_cv.notify_all();
    }
  }

  void sendMessage_(const Aseba::Message *message, int stream_index = -1,
                    const std::set<unsigned> &exclude_stream_indices = {}) {
    bool need_lock =
        (!thread || std::this_thread::get_id() != thread->get_id());
    if (need_lock) {
      lock();
    }
    for (auto stream : get_streams(stream_index, exclude_stream_indices)) {
      message->serialize(stream);
      stream->flush();
    }
    if (need_lock) {
      unlock();
    }
  }

#ifdef ZEROCONF
  void run_zeroconf() {
    std::cout << "run_zeroconf\n";
    while (zeroconf.dashelStep(-1))
      ;
  }
#endif

  void start() {
#ifdef ZEROCONF
    thread = std::make_unique<std::thread>(&DashelHub::run_zeroconf, this);
#else
    thread = std::make_unique<std::thread>(&Dashel::Hub::run, this);
#endif
    stopped = false;
    send_thread = std::make_unique<std::thread>(&DashelHub::run_out_msgs, this);
  }

  void stop() {
    Dashel::Hub::stop();
    if (thread) {
      thread->join();
    }
    thread = nullptr;
    stopped = true;
    if (send_thread) {
      send_thread->join();
    }
    send_thread = nullptr;
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

#ifdef ZEROCONF
  void advertise(const std::string &name,
                 const std::map<unsigned, Aseba::TargetDescription> &nodes) {
    if (!in_stream) {
      return;
    }
    std::vector<unsigned int> ids;
    std::vector<unsigned int> pids;
    std::string nname = "";
    unsigned protocolVersion{ASEBA_PROTOCOL_VERSION};
    for (auto const &[id, node] : nodes) {
      // std::string n = narrow(node.name);
      std::string n = "Thymio II";
      if (nname.empty()) {
        nname = n;
      } else if (nname != n) {
        nname = "Group";
      }
      std::cout << "ID " << id << std::endl;
      ids.push_back(static_cast<unsigned int>(id));
      // TODO
      pids.push_back(8); // product id
    }
    if (name.empty()) {
      nname = "Empty Group";
    }
    Aseba::Zeroconf::TxtRecord txt{protocolVersion, nname, false, ids, pids};
    try {
      zeroconf.advertise(name, in_stream, txt);
      std::cout << "Advertised " << name << " with" << txt.record()
                << std::endl;
    } catch (const std::runtime_error &e) {
      std::cerr << e.what() << std::endl;
    }
  }

  void deadvertise(const std::string &name) {
    if (!in_stream)
      return;
    zeroconf.forget(name, in_stream);
  }
#endif
};

#endif

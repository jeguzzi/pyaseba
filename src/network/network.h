#ifndef NETWORK_H
#define NETWORK_H

#include "advertise.h"
#include "aseba/vm/vm.h"
#include "dashel/dashel.h"
#include "node.h"
#include "utils.h"
#ifdef ZEROCONF
#include "zeroconf/zeroconf-dashelhub.h"
#endif

#include <atomic>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <pybind11/warnings.h>
#include <set>
#include <sstream>
#include <thread>
#include <valarray>
#include <vector>

class Network : public Dashel::Hub {

public:
  // vm -> stream
  inline static std::map<const AsebaVMState *, std::pair<Network *, Node *>>
      endpoints = {};

  static Network *network_for_vm(const AsebaVMState *vm) {
    if (endpoints.count(vm)) {
      return endpoints[vm].first;
    }
    return nullptr;
  }

  static Node *node_for_vm(const AsebaVMState *vm) {
    if (endpoints.count(vm)) {
      return endpoints[vm].second;
    }
    return nullptr;
  }

public:
  // void set_address(const std::string &a) { address = a; }
  const std::string &get_address() const { return address; }
  int get_port() const { return port; }
  void set_advertise_enabled(bool enabled) { advertise_enabled = enabled; }
  bool get_advertise_enabled() const { return advertise_enabled; }

private:
  // stream for listening to incoming connections
  Dashel::Stream *listenStream;
  std::string address;
  int port;
  std::string advertise_name;
  bool advertise_enabled;
  std::set<Dashel::Stream *> toDisconnect;
  std::atomic_bool spinning;
  std::unique_ptr<std::thread> thread;

public:
  std::map<int, std::shared_ptr<Node>> nodes;
  // this must be public because of bindings to C functions
  Dashel::Stream *stream;
#ifdef ZEROCONF
  Aseba::DashelhubZeroconf zeroconf;
#endif
  // all streams that must be disconnected at next step
  explicit Network(const std::string &address = "0.0.0.0",
                   const int port = ASEBA_DEFAULT_PORT,
                   const std::string &advertise_name = "")
      : Dashel::Hub(true), address(address), port(port),
        advertise_name(advertise_name),
        advertise_enabled(advertise_name.size()), spinning(false),
        thread(nullptr), stream(NULL) //, next_id(0)
#ifdef ZEROCONF
        ,
        zeroconf(*this)
#endif
  {
    if (listen())
      LOG_INFO("Created Aseba network listening on tcp:port={0}",
               listenStream->getTargetParameter("port"));
  }

  ~Network() {
    LOG_INFO("Deleted network on tcp:port={0}", port);
    stop();
  }

  void add_node(const std::shared_ptr<Node> &node) {
    node->init();
    endpoints[&(node->vm)] = std::make_pair(this, node.get());
    nodes[node->vm.nodeId] = node;
    // #ifdef ZEROCONF
    //     advertise();
    // #endif
  }

  void remove_node(const std::shared_ptr<Node> &node) {
    endpoints.erase(&(node->vm));
    nodes.erase(node->vm.nodeId);
    // #ifdef ZEROCONF
    //     // if (nodes.size() == 0) {
    //     //   deadvertise();
    //     // }
    //     advertise();
    // #endif
  }

  std::vector<AdvertisedNode> make_record_nodes() const {
    std::vector<AdvertisedNode> rs;
    for (const auto &[id, node] : nodes) {
      const auto vpid = node->get_variable(ASEBA_PID_VAR_NAME);
      const unsigned int pid = vpid.size() ? vpid[0] : 0;
      rs.emplace_back(static_cast<unsigned int>(id), pid,
                      node->get_advertized_name());
    }
    return rs;
  }

  void advertise() {
    if (!advertise_enabled || !listenStream) {
      return;
    }
#ifdef ZEROCONF
    ::advertise(zeroconf, listenStream, advertise_name, make_record_nodes());
#else
    pybind11::warnings::warn(
        "Pyaseba was built without Zeroconf support: skip advertise!");
#endif
  }

  void deadvertise() {
    if (!advertise_enabled || !listenStream) {
      return;
    }
#ifdef ZEROCONF
    ::deadvertise(zeroconf, listenStream, advertise_name);
#else
    pybind11::warnings::warn(
        "Pyaseba was built without Zeroconf support: skip de-advertise!");
#endif
  }

  Dashel::Stream *listen() {
    // connect client
    try {
      std::ostringstream oss;
      oss << "tcpin:port=" << port << ";address=" << address;
      listenStream = Dashel::Hub::connect(oss.str());
    } catch (Dashel::DashelException e) {
      LOG_WARN("Cannot create listening port {0}: {1}", port, e.what());
      listenStream = nullptr;
    }
    return listenStream;
  }

  virtual void connectionCreated(Dashel::Stream *stream) {
    std::string targetName = stream->getTargetName();
    LOG_INFO("New connection from {0}", targetName);
    if (targetName.substr(0, targetName.find_first_of(':')) == "tcp") {
      // schedule current stream for disconnection
      if (!this->stream) {
        this->stream = stream;
        LOG_INFO("Connection accepted");
      } else {
        LOG_INFO(
            "Connection refused: we are already connected to a client stream");
        // ??? Dashel say not to call closeStream in connectionCreated ???
        // closeStream(stream);
        // but this (proper way) makes us crash (why?)
        toDisconnect.insert(stream);
      }
      // toDisconnect.push_back(this->stream);
      // set new stream as current stream
      // this->stream = stream;
      // printf("New client connected.\n");
    }
  }

  virtual void connectionClosed(Dashel::Stream *stream, bool abnormal) {
    LOG_INFO("Connection to {0} closed ({1})", stream->getTargetName(),
             abnormal);
#ifdef ZEROCONF
    zeroconf.dashelConnectionClosed(stream);
#endif // ZEROCONF
    if (stream == this->stream) {
      this->stream = nullptr;
      // clear breakpoints
      for (auto &[id, node] : nodes) {
        node->vm.breakpointsCount = 0;
      }
    }
    toDisconnect.erase(stream);
    if (abnormal)
      LOG_WARN("Client has disconnected unexpectedly.");
    // else
    // printf("Client has disconnected properly.\n");
  }

  virtual void incomingData(Dashel::Stream *stream) {
#ifdef ZEROCONF
    if (zeroconf.isStreamHandled(stream)) {
      LOG_DEBUG("Incoming data for zeroconf");
      try {
        zeroconf.dashelIncomingData(stream);
      } catch (const std::exception &e) {
        LOG_ERROR("Advertise: {0}", e.what());
      }
      return;
    }
#endif // ZEROCONF
    LOG_DEBUG("Incoming data from {}", stream->getTargetName());
    // only process data for the current stream
    if (stream != this->stream) {
      return;
    }
    uint16_t temp;
    uint16_t len;
    stream->read(&temp, 2);
    len = bswap16(temp);
    stream->read(&temp, 2);
    uint16_t lastMessageSource;
    std::valarray<uint8_t> lastMessageData;
    lastMessageSource = bswap16(temp);
    lastMessageData.resize(len + 2);
    stream->read(&lastMessageData[0], lastMessageData.size());
    // uint16_t type = bswap16(lastMessageData[0]);
    uint16_t type;
    memcpy(&type, &lastMessageData[0], 2);
    type = bswap16(type);
    LOG_DEBUG("Incoming data ({0} bytes) of type 0x{1:X} from {2}", len, type,
              lastMessageSource);
    /* from IDE to a specific node */
    if (type >= ASEBA_MESSAGE_SET_BYTECODE &&
        type <= ASEBA_MESSAGE_GET_NODE_DESCRIPTION) {
      uint16_t dest;
      memcpy(&dest, &lastMessageData[2], 2);
      dest = bswap16(dest);
      LOG_DEBUG(
          "Got cmd message of type type 0x{0:X} from IDE ({1}) for node {2}",
          type, lastMessageSource, dest);
      if (nodes.count(dest)) {
        auto node = nodes.at(dest);
        if (type == ASEBA_MESSAGE_GET_EXECUTION_STATE) {
          node->send_device_info((void *)stream);
        }

        node->lastMessageSource = lastMessageSource;
        node->lastMessageData = lastMessageData;
        AsebaProcessIncomingEvents(&(node->vm));
        AsebaVMRun(&(node->vm), 1000);
      }
      return;
    }
    LOG_DEBUG("Got cmd message of type 0x{0:X} from node {1}", type,
              lastMessageSource);
    for (auto &[id, node] : nodes) {
      node->lastMessageSource = lastMessageSource;
      node->lastMessageData = lastMessageData;
      AsebaProcessIncomingEvents(&(node->vm));
      AsebaVMRun(&(node->vm), 1000);
    }
  }

  bool spin_once(unsigned timeout_ms) {
#ifdef ZEROCONF
    if (!zeroconf.dashelStep(timeout_ms))
#else
    if (!step(timeout_ms))
#endif // ZEROCONF
      return false;
    // disconnect old streams
    // No need to lock
    // lock();
    for (auto &stream : toDisconnect) {
      closeStream(stream);
      LOG_INFO("Connection {0} closed", stream->getTargetName());
    }
    toDisconnect.clear();
    // unlock();
    return true;
  }

  void tick(double dt) {
    for (const auto &[_, node] : nodes) {
      node->step(dt);
    }
  }

  void start(double dt, double duration = -1) {
    thread = std::make_unique<std::thread>(&Network::spin, this, dt, duration);
  }

  void stop() {
    spinning = false;
    if (thread) {
      thread->join();
    }
    thread = nullptr;
  }

  void spin(double dt, double duration = -1) {
    if (spinning)
      return;
#ifdef ZEROCONF
    advertise();
#endif
    spinning = true;
    Aseba::UnifiedTime deadline;
    const Aseba::UnifiedTime until =
        Aseba::UnifiedTime(deadline) + Aseba::UnifiedTime(duration * 1e3);
    const bool terminated = duration > 0;
    const Aseba::UnifiedTime delta = (static_cast<unsigned>(dt * 1e3));
    while (spinning) {
      if (dt > 0) {
        const auto now = Aseba::UnifiedTime();
        if (terminated && now > until)
          break;
        while (deadline <= now) {
          tick(dt);
          deadline += delta;
        }
        const auto d = deadline - now;
        if (!spin_once(d.value))
          break;
      } else {
        if (!spin_once(10))
          break;
      }
    }
    spinning = false;
#ifdef ZEROCONF
    deadvertise();
#endif
  }
};

#endif // NETWORK_H

#ifndef NETWORK_H
#define NETWORK_H

#ifndef ASEBA_ASSERT
#define ASEBA_ASSERT
#endif

#include "aseba/vm/vm.h"
#include "dashel/dashel.h"

#ifdef ZEROCONF
#include "aseba/common/zeroconf/zeroconf-dashelhub.h"
#endif

#include "node.h"
#include "utils.h"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <stack>

class Network : public Dashel::Hub {

public:
  // vm -> stream
  inline static std::map<const AsebaVMState *, std::pair<Network *, Node *>>
      endpoints = {};

  static Network *client_for_vm(const AsebaVMState *vm) {
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
  void set_advertise_enabled(bool enabled) {
    advertise_enabled = enabled;
  }
  bool get_advertise_enabled() const {
    return advertise_enabled;
  }

private:
  // stream for listening to incoming connections
  Dashel::Stream *listenStream;
  std::string address;
  int timeout;
  int port;
  std::string advertise_name;
  bool advertise_enabled;
  std::set<Dashel::Stream *> toDisconnect;

public:
  std::map<int, std::shared_ptr<Node>> nodes;
  // this must be public because of bindings to C functions
  Dashel::Stream *stream;
#ifdef ZEROCONF
  Aseba::DashelhubZeroconf zeroconf;
#endif
  // all streams that must be disconnected at next step
  explicit Network(const std::string &address = "0.0.0.0",
                   const int port = ASEBA_DEFAULT_PORT, int timeout = 0,
                   const std::string &advertise_name = "")
      : Dashel::Hub(true), address(address), timeout(timeout), port(port),
        advertise_name(advertise_name),
        advertise_enabled(advertise_name.size()), stream(NULL) //, next_id(0)
#ifdef ZEROCONF
        ,
        zeroconf(*this)
#endif
  {
    if (listen())
      log_info("Created Aseba client listening on tcp:port=%s",
               listenStream->getTargetParameter("port").c_str());
  }

  ~Network() {
    // for (auto kv : nodes) {
    //   delete kv.second;
    // }
    log_info("Deleted client on tcp:port=%d", port);
  }

  void add_node(const std::shared_ptr<Node> &node) {
    // log_info("Add node");
    node->finalize();
    endpoints[&(node->vm)] = std::make_pair(this, node.get());
    nodes[node->vm.nodeId] = node;
#ifdef ZEROCONF
    advertise();
#endif
  }

  void remove_node(const std::shared_ptr<Node> &node) {
    // log_info("Remove node");
    endpoints.erase(&(node->vm));
    nodes.erase(node->vm.nodeId);
#ifdef ZEROCONF
    // if (nodes.size() == 0) {
    //   deadvertise();
    // }
    advertise();
#endif
  }

#ifdef ZEROCONF
  void advertise() {
    if (!advertise_enabled) {
      return;
    }
    if (!listenStream) {
      return;
    }
    std::vector<unsigned int> ids;
    std::vector<unsigned int> pids;
    std::string name = "";
    // unsigned protocolVersion{ASEBA_PROTOCOL_VERSION};
    unsigned protocolVersion{9};
    for (auto const &[id, node] : nodes) {
      std::string n_name = node->advertized_name();
      if (name.empty()) {
        name = n_name;
      } else if (name != n_name) {
        name = "Group";
      }
      ids.push_back(static_cast<unsigned int>(id));
      // names += node->name + " ";
      auto pid = node->get_variable(ASEBA_PID_VAR_NAME);
      pids.push_back(pid[0]); // product id
    }
    if (name.empty()) {
      name = "Empty Group";
    }
    // Aseba::Zeroconf::TxtRecord txt{protocolVersion, names, false, ids, pids};
    Aseba::Zeroconf::TxtRecord txt{protocolVersion, name, false, ids, pids};
    try {
      zeroconf.advertise(advertise_name, listenStream, txt);
      log_debug("Advertised %s with %s", advertise_name.c_str(),
                txt.record().c_str());
    } catch (const std::runtime_error &e) {
      log_warn("Could not advertise: %s", e.what());
    }
  }

  void deadvertise() {
    if (!listenStream)
      return;
    log_debug("Deadvertise Aseba Network");
    zeroconf.forget(advertise_name, listenStream);
  }
#endif

  Dashel::Stream *listen() {
    // connect client
    try {
      std::ostringstream oss;
      oss << "tcpin:port=" << port << ";address=" << address;
      listenStream = Dashel::Hub::connect(oss.str());
    } catch (Dashel::DashelException e) {
      log_warn("Cannot create listening port %d: %s", port, e.what());
      listenStream = nullptr;
    }
    return listenStream;
  }

  virtual void connectionCreated(Dashel::Stream *stream) {
    std::string targetName = stream->getTargetName();
    log_info("Incoming Dashel connection from %s", targetName.c_str());
    if (targetName.substr(0, targetName.find_first_of(':')) == "tcp") {
      // schedule current stream for disconnection
      if (!this->stream) {
        this->stream = stream;
        log_info("Connection accepted");
      } else {
        log_info(
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
    log_info("Dashel connection closed");
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
      log_warn("Client has disconnected unexpectedly.");
    // else
    // printf("Client has disconnected properly.\n");
  }

  virtual void incomingData(Dashel::Stream *stream) {
#ifdef ZEROCONF
    if (zeroconf.isStreamHandled(stream)) {
      log_debug("Incoming data for zeroconf");
      try {
        zeroconf.dashelIncomingData(stream);
      } catch (const std::exception &e) {
        log_error("Advertise: %s", e.what());
      }
      return;
    }
#endif // ZEROCONF
    std::cout << "incomingData from " << stream->getTargetName() << " (" << this->stream->getTargetName() << ")" << std::endl;
    // only process data for the current stream
    if (stream != this->stream) {
      // printf("[DASHEL] incomingData from %p (%p) -> ignore\n", stream,
      // this->stream);
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
    std::cout << std::hex << type;
    type = bswap16(type);
    std::cout << " => " << std::hex << type << std::dec << std::endl;
    // memcpy(data, &node->lastMessageData[0], node->lastMessageData.size());

    // printf("[DASHEL] incomingData %d %d => %d\n", lastMessageData[0],
    // lastMessageData[1], type);
    log_debug("Incoming data (%d bytes) of type 0x%X %d from %d", len, (unsigned)type, type,
              lastMessageSource);
    // std::vector<std::shared_ptr<Node>> dest_nodes;
    /* from IDE to a specific node */
    if (type >= ASEBA_MESSAGE_SET_BYTECODE &&
        type <= ASEBA_MESSAGE_GET_NODE_DESCRIPTION) {
      uint16_t dest;
      memcpy(&dest, &lastMessageData[2], 2);
      dest = bswap16(dest);
      log_debug("Got cmd message of type %d from IDE (%d) for node %d\n",
              type, lastMessageSource, dest);
      if (nodes.count(dest)) {
        auto node = nodes.at(dest);
        if (node->finalized) {
          if (type == ASEBA_MESSAGE_GET_EXECUTION_STATE) {
            node->send_device_info((void *)stream);
          }

          node->lastMessageSource = lastMessageSource;
          node->lastMessageData = lastMessageData;
          AsebaProcessIncomingEvents(&(node->vm));
          AsebaVMRun(&(node->vm), 1000);
        }
      }
      return;
    }
    printf("Got cmd message of type %d from node %d\n", type, lastMessageSource);
    for (auto &[id, node] : nodes) {
      if (node->finalized) {
        node->lastMessageSource = lastMessageSource;
        node->lastMessageData = lastMessageData;
        AsebaProcessIncomingEvents(&(node->vm));
        AsebaVMRun(&(node->vm), 1000);
      }
    }
  }

  bool spin(float dt) {
#ifdef ZEROCONF
    if (!zeroconf.dashelStep(timeout))
#else
    if (!step(timeout))
#endif // ZEROCONF
      return false;

    for (const auto &[id, node] : nodes) {
      node->finalized = true;
      node->step(dt);
    }
    // disconnect old streams
    lock();
    for (auto &stream : toDisconnect) {
      closeStream(stream);
      log_info("Stream %s closed in spin", stream->getTargetName().c_str());
    }
    toDisconnect.clear();
    unlock();
    return true;
  }
  //
  // void run() {
  //   while (spin()) {}
  // }
};

#endif // NETWORK_H

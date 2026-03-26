#ifndef PYASEBA_NODE_MANAGER_H_GUARD
#define PYASEBA_NODE_MANAGER_H_GUARD

#include "aseba/common/msg/NodesManager.h"
#include "aseba/common/msg/msg.h"
#include "aseba/compiler/compiler.h"
#include "dashel_hub.h"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

#include <chrono>
#include <future>
#include <map>
#include <memory>
#include <shared_mutex>
#include <string>
#include <thread>
#include <tuple>

// UTF8 to wstring
inline std::wstring widen(const char *src) {
  const size_t destSize(mbstowcs(0, src, 0) + 1);
  std::vector<wchar_t> buffer(destSize, 0);
  mbstowcs(&buffer[0], src, destSize);
  return std::wstring(buffer.begin(), buffer.end() - 1);
}

using EventMaps = std::map<unsigned, std::map<std::wstring, unsigned>>;

struct Event {
  unsigned source;
  std::wstring name;
  Aseba::VariablesDataVector data;

  Event(unsigned source, const std::wstring &name,
        const Aseba::VariablesDataVector &data)
      : source(source), name(name), data(data) {}

  static std::shared_ptr<Event>
  from_msg(const std::shared_ptr<Aseba::Message> &msg,
           const EventMaps &event_maps) {
    if (const auto u_msg = std::dynamic_pointer_cast<Aseba::UserMessage>(msg)) {
      std::wstring name;
      if (event_maps.count(u_msg->source)) {
        for (const auto &[k, v] : event_maps.at(u_msg->source)) {
          if (v == u_msg->type) {
            name = k;
            break;
          }
        }
      }
      if (name.size()) {
        return std::make_shared<Event>(u_msg->source, name, u_msg->data);
      }
    }
    return nullptr;
  }
};

using EventCallback = std::function<void(const std::shared_ptr<Event> &)>;

template <typename T> struct AWaited {
  using Callback = std::function<void(const T &)>;

  Callback cb;
  std::unique_ptr<std::promise<T>> value;
  AWaited(bool awaited, const Callback &cb = nullptr) : cb(cb), value(nullptr) {

    if (awaited) {
      value = std::make_unique<std::promise<T>>();
    }
  }
};

using AWaitedDisconnection = AWaited<bool>;
using AWaitedNode = AWaited<uint16_t>;
using AWaitedMessage = AWaited<std::shared_ptr<Aseba::Message>>;

struct PyNodesManager : public Aseba::NodesManager {
  using VariablesCallback =
      std::function<void(const Aseba::VariablesDataVector &)>;
  using MessageCallback =
      std::function<void(const std::shared_ptr<Aseba::Message> &)>;
  using NodeCallback = std::function<void(unsigned)>;
  using MessageVector = std::vector<std::unique_ptr<Aseba::Message>>;
  using AWaitedMessageKey = std::tuple<int, int>;
  using WriteLock = std::unique_lock<std::shared_mutex>;
  using ReadLock = std::shared_lock<std::shared_mutex>;
  DashelHub hub;
  Dashel::Stream *stream;
  bool running;
  std::shared_mutex nodes_mutex;
  std::map<unsigned, Aseba::VariablesMap> variable_maps;
  EventMaps event_maps;
  std::vector<MessageCallback> message_callbacks;
  std::vector<NodeCallback> connection_callbacks;
  std::vector<NodeCallback> disconnection_callbacks;
  std::vector<AWaitedDisconnection> awaited_disconnections;
  std::map<int, std::vector<AWaitedNode>> awaited_node_connections;
  std::map<int, std::vector<AWaitedNode>> awaited_node_disconnections;
  std::map<AWaitedMessageKey, std::vector<AWaitedMessage>> awaited_messages;

  PyNodesManager()
      : hub(this), stream(nullptr), running(false), nodes_mutex(),
        variable_maps(), message_callbacks(), connection_callbacks(),
        disconnection_callbacks(), awaited_disconnections(),
        awaited_node_connections(), awaited_node_disconnections(),
        awaited_messages() {}

  bool has_node(unsigned nodeId) const {
    ReadLock lock;
    return nodes.count(nodeId) > 0;
  }

  std::vector<unsigned> get_nodes() const {
    ReadLock lock;
    std::vector<unsigned> ns;
    for (auto &[k, v] : nodes) {
      ns.push_back(k);
    }
    return ns;
  }

  void connectionClosed(const Dashel::Stream *) {
    WriteLock rlock;
    for (auto &[k, v] : nodes) {
      v.connected = false;
      nodeDisconnected(k);
    }
    nodes.clear();
    stream = nullptr;
  }

  void connectionCreated(const Dashel::Stream *) {}

  std::map<unsigned, Aseba::TargetDescription> get_nodes_() const {
    ReadLock lock;
    std::map<unsigned, Aseba::TargetDescription> rs;
    for (auto &[k, v] : nodes) {
      rs[k] = v;
    }
    return rs;
  }

  void add_message_callback(const MessageCallback &cb) {
    message_callbacks.push_back(cb);
  }

  void add_connection_callback(const NodeCallback &cb) {
    connection_callbacks.push_back(cb);
  }

  void add_disconnection_callback(const NodeCallback &cb) {
    disconnection_callbacks.push_back(cb);
  }

  void add_event_callback(const EventCallback &cb) {
    if (cb) {
      MessageCallback mcb = [cb,
                             this](const std::shared_ptr<Aseba::Message> &msg) {
        auto e = Event::from_msg(msg, event_maps);
        if (e) {
          cb(e);
        }
      };
      message_callbacks.push_back(mcb);
    }
  }

  void processMessage(const Aseba::Message *msg) {
    {
      WriteLock lock;
      Aseba::NodesManager::processMessage(msg);
    }
    std::shared_ptr<Aseba::Message> smsg;
    smsg.reset(std::move(msg->clone()));
    for (const auto &cb : message_callbacks) {
      py::gil_scoped_acquire acquire;
      cb(smsg);
    }
    // if (const auto e_msg = dynamic_cast<const Aseba::UserMessage *>(msg)) {
    //   for (const auto &cb : event_callbacks) {
    //     py::gil_scoped_acquire acquire;
    //     cb(e_msg->source, e_msg->type, e_msg->data);
    //   }
    // }
    check_awaited_messages(msg->source, msg->type, smsg);
    check_awaited_messages(-1, msg->type, smsg);
    check_awaited_messages(msg->source, -1, smsg);
    check_awaited_messages(-1, -1, smsg);
  }

  void check_awaited_messages(int source, int type,
                              const std::shared_ptr<Aseba::Message> &msg) {
    const std::tuple<int, int> key = std::make_tuple(source, type);
    if (awaited_messages.count(key)) {
      for (auto &wm : awaited_messages.at(key)) {
        if (wm.cb) {
          py::gil_scoped_acquire acquire;
          wm.cb(msg);
        }
        if (wm.value) {
          wm.value->set_value(msg);
        }
      }
      awaited_messages.erase(key);
    }
  }

  bool connect_and_start(const std::string &target, unsigned wait_ms,
                         int max_retries) {
    bool failed = false;
    bool connected = false;
    max_retries = std::max(max_retries, 0);
    while (max_retries >= 0) {
      connected = connect(target);
      if (connected) {
        break;
      }
      max_retries--;
      failed = true;
      std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
    }
    if (connected) {
      if (failed) {
        // HACK: else coppelia-sim aseba not connected if started after python
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      }
      start();
    }
    return connected;
  }

  void start() {
    if (!running) {
      hub.start();
      pingNetwork();
      running = true;
    }
  }

  bool connect(const std::string &target) {
    if (stream) {
      std::cerr << "Already connected to " << stream->getTargetName()
                << std::endl;
      return false;
    }
    try {
      stream = hub.connect(target);
      return true;
    } catch (const Dashel::DashelException &e) {
      std::cerr << e.what() << std::endl;
      return false;
    }
  }

  bool is_connected() const { return stream != nullptr; }

  void stop() {
    hub.stop();
    running = false;
  }

  void close() {
    if (stream) {
      hub.closeStream(stream);
    }
    stream = nullptr;
  }

  void stop_and_close() {
    stop();
    close();
  }

  void send_event(unsigned node_id, unsigned index,
                  const Aseba::VariablesDataVector &data) {
    sendUserMessage(index, data);
  }

  void emit_event(unsigned node_id, const std::wstring &name,
                  const Aseba::VariablesDataVector &data) {
    if (event_maps[node_id].count(name)) {
      const auto index = event_maps[node_id].at(name);
      send_event(node_id, index, data);
    }
  }

  void sendMessage(const Aseba::Message &message) override {
    hub.sendMessage(&message);
  }

  void sendUserMessage(unsigned type, const Aseba::VariablesDataVector &data) {
    Aseba::UserMessage message(type, data);
    hub.sendMessage(&message);
  }

  void set_variable_at_index(unsigned nodeId, unsigned index,
                             const Aseba::VariablesDataVector &value) {
    Aseba::SetVariables message(nodeId, index, value);
    hub.sendMessage(&message);
  }

  void set_variable(unsigned nodeId, const std::wstring &name,
                    const Aseba::VariablesDataVector &value) {

    const auto &m = getVariablesMap(nodeId);
    if (!m.count(name))
      return;
    auto [index, size] = m.at(name);
    // bool ok;
    // const auto index = getVariablePos(nodeId, name, &ok);
    // if (!ok)
    //   return;
    // const auto size = getVariableSize(nodeId, name, &ok);
    // if (!ok || size != value.size())
    //   return;
    set_variable_at_index(nodeId, index, value);
  }

  std::optional<Aseba::VariablesDataVector>
  get_variable_at_index(unsigned nodeId, unsigned index, unsigned size,
                        unsigned wait_ms,
                        const VariablesCallback &cb = nullptr) {
    Aseba::GetVariables message(nodeId, index, size);
    hub.sendMessage(&message);
    AWaitedMessage::Callback mcb = nullptr;
    if (cb) {
      mcb = [cb](const std::shared_ptr<Aseba::Message> &msg) {
        if (const auto v_msg =
                std::dynamic_pointer_cast<Aseba::Variables>(msg)) {
          cb(v_msg->variables);
        }
      };
    }
    auto msg = get_message(nodeId, static_cast<int>(ASEBA_MESSAGE_VARIABLES),
                           wait_ms, mcb);
    if (const auto v_msg = std::dynamic_pointer_cast<Aseba::Variables>(msg)) {
      return v_msg->variables;
    }
    return std::nullopt;
  }

  std::optional<Aseba::VariablesDataVector>
  get_variable(unsigned nodeId, const std::wstring &name, unsigned wait_ms,
               const VariablesCallback &cb = nullptr) {
    // bool ok;
    // const auto index = getVariablePos(nodeId, name, &ok);
    // if (!ok)
    //   return std::nullopt;
    // const auto size = getVariableSize(nodeId, name, &ok);
    // if (!ok)
    //   return std::nullopt;
    const auto &m = getVariablesMap(nodeId);
    if (!m.count(name))
      return std::nullopt;
    auto [index, size] = m.at(name);
    return get_variable_at_index(nodeId, index, size, wait_ms, cb);
  }

  void load_script(unsigned nodeId, const std::string &code,
                   const std::vector<Aseba::NamedValue> &events = {},
                   const std::vector<Aseba::NamedValue> &constants = {}) {

    WriteLock lock;
    std::wistringstream aesl_code(widen(code.c_str()));
    unsigned allocatedVariablesCount;
    Aseba::Compiler compiler;
    Aseba::Error error;
    Aseba::BytecodeVector bytecode;
    auto description = getDescription(nodeId);
    compiler.setTargetDescription(description);
    Aseba::CommonDefinitions common_definitions;
    common_definitions.events.assign(events.begin(), events.end());
    common_definitions.constants.assign(constants.begin(), constants.end());
    compiler.setCommonDefinitions(&common_definitions);
    bool result =
        compiler.compile(aesl_code, bytecode, allocatedVariablesCount, error);
    if (!result) {
      std::cerr << "Failed to compile script for node";
      std::wcerr << error.toWString() << std::endl;
      return;
    }
    variable_maps[nodeId] = *(compiler.getVariablesMap());
    unsigned i = 0;
    for (const auto &e : events) {
      event_maps[nodeId][e.name] = i++;
    }
    // py::print("VM", *vs);

    MessageVector messages;
    auto bytes = std::vector<uint16_t>(bytecode.begin(), bytecode.end());
    Aseba::sendBytecode(messages, nodeId, bytes);
    for (auto &msg : messages) {
      hub.sendMessage(msg.get());
    }
  }

  void run(unsigned nodeId) {
    Aseba::Run msg(nodeId);
    hub.sendMessage(&msg);
  }

  void pause(unsigned nodeId) {
    Aseba::Pause msg(nodeId);
    hub.sendMessage(&msg);
  }

  void stop_node(unsigned nodeId) {
    Aseba::Stop msg(nodeId);
    hub.sendMessage(&msg);
  }

  void reset(unsigned nodeId) {
    Aseba::Reset msg(nodeId);
    hub.sendMessage(&msg);
  }

  void sleep(unsigned nodeId) {
    Aseba::Sleep msg(nodeId);
    hub.sendMessage(&msg);
  }

  const Aseba::TargetDescription *getDescription(unsigned nodeId) const {
    ReadLock lock;
    bool ok;
    auto r = Aseba::NodesManager::getDescription(nodeId, &ok);
    if (!ok) {
      return nullptr;
    }
    return r;
  }

  inline static Aseba::VariablesMap empty_variable_map = {};

  const Aseba::VariablesMap &getVariablesMap(unsigned nodeId) const {
    ReadLock lock;
    if (variable_maps.count(nodeId)) {
      return variable_maps.at(nodeId);
    }
    return empty_variable_map;
  }

  std::vector<std::wstring> getVariables(unsigned nodeId) const {
    const auto &m = getVariablesMap(nodeId);
    std::vector<std::wstring> vs(m.size());
    size_t i = 0;
    for (const auto &[name, _] : m) {
      vs[i++] = name;
    }
    return vs;
  }

  std::vector<std::wstring> getEvents(unsigned nodeId) const {
    ReadLock lock;
    if (event_maps.count(nodeId)) {
      const auto &m = event_maps.at(nodeId);
      std::vector<std::wstring> rs(m.size());
      size_t i = 0;
      for (const auto &[name, _] : m) {
        rs[i++] = name;
      }
      return rs;
    }
    return {};
  }

  void nodeProtocolVersionMismatch(unsigned nodeId,
                                   const std::wstring &nodeName,
                                   uint16_t protocolVersion) override {
    std::cerr << "nodeProtocolVersionMismatch" << std::endl;
  }

  void nodeDescriptionReceived(unsigned nodeId) override {
    unsigned i;
    variable_maps[nodeId] = getDescription(nodeId)->getVariablesMap(i);
  }

  void
  check_awaited_node(unsigned targetId, unsigned nodeId,
                     std::map<int, std::vector<AWaitedNode>> &awaited_nodes) {
    if (awaited_nodes.count(targetId)) {
      for (auto &awaited_node : awaited_nodes.at(targetId)) {
        if (awaited_node.cb) {
          py::gil_scoped_acquire acquire;
          awaited_node.cb(nodeId);
        }
        if (awaited_node.value) {
          awaited_node.value->set_value(nodeId);
        }
      }
      awaited_nodes.erase(targetId);
    }
  }

  void nodeConnected(unsigned nodeId) override {
    for (const auto &cb : connection_callbacks) {
      py::gil_scoped_acquire acquire;
      cb(nodeId);
    }
    check_awaited_node(nodeId, nodeId, awaited_node_connections);
    check_awaited_node(-1, nodeId, awaited_node_connections);
  }

  void nodeDisconnected(unsigned nodeId) override {
    for (const auto &cb : disconnection_callbacks) {
      py::gil_scoped_acquire acquire;
      cb(nodeId);
    }
    for (auto &awaited : awaited_disconnections) {
      if (awaited.cb) {
        py::gil_scoped_acquire acquire;
        awaited.cb(true);
      }
      if (awaited.value) {
        awaited.value->set_value(true);
      }
    }
    awaited_disconnections.clear();
    check_awaited_node(nodeId, nodeId, awaited_node_disconnections);
    check_awaited_node(-1, nodeId, awaited_node_disconnections);
    variable_maps.erase(nodeId);
    event_maps.erase(nodeId);
  }

  std::shared_ptr<Event> get_event(unsigned nodeId, const std::wstring &name,
                                   int wait_ms = 0,
                                   const EventCallback &cb = nullptr) {
    if (!event_maps[nodeId].count(name)) {
      return nullptr;
    }
    const int type = event_maps[nodeId].at(name);
    AWaitedMessage::Callback mcb = nullptr;
    if (cb) {
      mcb = [cb, this](const std::shared_ptr<Aseba::Message> &msg) {
        auto e = Event::from_msg(msg, event_maps);
        if (e) {
          cb(e);
        }
      };
    }
    const auto msg = get_message(nodeId, type, wait_ms, mcb);
    return Event::from_msg(msg, event_maps);
  }

  std::shared_ptr<Aseba::Message>
  get_message(int nodeId = -1, int type = -1, int wait_ms = 0,
              const AWaitedMessage::Callback &cb = nullptr) {
    py::gil_scoped_release release;
    if (!wait_ms && !cb) {
      return nullptr;
    }
    if (nodeId < 0) {
      nodeId = -1;
    }
    if (type < 0) {
      type = -1;
    }
    const auto key = std::make_tuple(nodeId, type);
    const auto &[it, _] = awaited_messages.try_emplace(key);
    auto &ls = it->second;
    auto &awaited_msg = it->second.emplace_back(wait_ms, cb);
    if (!wait_ms) {
      return nullptr;
    }
    auto future = awaited_msg.value->get_future();
    std::shared_ptr<Aseba::Message> r = nullptr;
    const auto s = future.wait_for(std::chrono::milliseconds(wait_ms));
    if (s == std::future_status::ready) {
      r = future.get();
    }
    return r;
  }

  std::optional<unsigned>
  wait_node_connection(int nodeId = -1, unsigned wait_ms = 1000,
                       const AWaitedNode::Callback &cb = nullptr) {
    std::optional<unsigned> r = std::nullopt;
    {
      ReadLock lock;
      for (auto &[k, n] : nodes) {
        if (n.isComplete() && n.connected && (nodeId < 0 || nodeId == k)) {
          if (cb) {
            cb(k);
          }
          return k;
        }
      }
    }
    if (!wait_ms && !cb) {
      return std::nullopt;
    }
    if (nodeId < 0) {
      nodeId = -1;
    }
    const auto &[it, _] = awaited_node_connections.try_emplace(nodeId);
    auto &ls = it->second;
    auto &awaited_node = it->second.emplace_back(wait_ms, cb);
    if (!wait_ms) {
      return r;
    }
    auto future = awaited_node.value->get_future();
    const auto s = future.wait_for(std::chrono::milliseconds(wait_ms));
    if (s == std::future_status::ready) {
      r = future.get();
    }
    return r;
  }

  std::optional<bool>
  wait_disconnection(unsigned wait_ms = 1000,
                     const AWaitedDisconnection::Callback &cb = nullptr) {
    std::optional<bool> r = std::nullopt;
    if (!stream)
      return true;
    if (!wait_ms && !cb) {
      return r;
    }
    auto &awaited = awaited_disconnections.emplace_back(wait_ms, cb);
    if (!wait_ms) {
      return r;
    }
    auto future = awaited.value->get_future();
    const auto s = future.wait_for(std::chrono::milliseconds(wait_ms));
    if (s == std::future_status::ready) {
      r = future.get();
    }
    return r;
  }

  std::optional<unsigned>
  wait_node_disconnection(int nodeId = -1, unsigned wait_ms = 1000,
                          const AWaitedNode::Callback &cb = nullptr) {
    std::optional<unsigned> r = std::nullopt;
    {
      ReadLock lock;
      for (auto &[k, n] : nodes) {
        if (n.isComplete() && !n.connected && (nodeId < 0 || nodeId == k)) {
          if (cb) {
            cb(k);
          }
          return k;
        }
      }
    }
    if (!wait_ms && !cb) {
      return std::nullopt;
    }
    if (nodeId < 0) {
      nodeId = -1;
    }
    const auto &[it, _] = awaited_node_disconnections.try_emplace(nodeId);
    auto &ls = it->second;
    auto &awaited_node = it->second.emplace_back(wait_ms, cb);
    if (!wait_ms) {
      return r;
    }
    auto future = awaited_node.value->get_future();
    const auto s = future.wait_for(std::chrono::milliseconds(wait_ms));
    if (s == std::future_status::ready) {
      r = future.get();
    }
    return r;
  }
};

#endif
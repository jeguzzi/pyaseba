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
#include <queue>
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

template <bool P, typename T> struct AWaitedCallback {
  using type = typename std::conditional<P, std::function<void(T, bool)>,
                                         std::function<void(T)>>::type;
  static void apply(const type &cb, const T &arg, bool complete) {
    if constexpr (P) {
      cb(arg, complete);
    } else {
      cb(arg);
    }
  }
};

template <bool P, typename... T> struct AWaitedCallback<P, std::tuple<T...>> {
  using type = typename std::conditional<P, std::function<void(T..., bool)>,
                                         std::function<void(T...)>>::type;
  static void apply(const type &cb, const std::tuple<T...> &arg,
                    bool complete) {
    if constexpr (P) {
      std::apply(cb, std::tuple_cat(arg, std::tuple<bool>{complete}));
    } else {
      std::apply(cb, arg);
    }
  }
};

template <typename C, bool P, typename V, typename... T> struct AWaited {
  using Value = V;
  using Promise = std::promise<Value>;
  using TValue = Value;
  using AC = AWaitedCallback<P, V>;
  using Callback = typename AC::type;

  Callback cb;
  std::unique_ptr<Promise> value;
  bool complete;
  explicit AWaited(bool awaited, const Callback &cb = nullptr)
      : cb(cb), value(nullptr), complete(false) {
    if (awaited) {
      value = std::make_unique<Promise>();
    }
  }

  void update(T... args) {
    complete = static_cast<C *>(this)->is_complete(args...);
    Value v;
    if (complete || P) {
      v = static_cast<C *>(this)->get(args...);
    }
    if (complete && value) {
      value->set_value(v);
    }
    if (cb) {
      py::gil_scoped_acquire acquire;
      AC::apply(cb, v, complete);
    }
  }

  static void check(T... args, std::vector<C> &queue) {
    for (auto &awaited : queue) {
      awaited.update(args...);
    }
    queue.erase(std::remove_if(queue.begin(), queue.end(),
                               [](const auto &a) { return a.complete; }),
                queue.end());
  }
};

struct AWaitedNode : AWaited<AWaitedNode, false, uint16_t, uint16_t> {
  int target;
  using A = AWaited<AWaitedNode, false, uint16_t, uint16_t>;
  using A::Callback;
  using A::Value;

  explicit AWaitedNode(int node, bool awaited, const Callback &cb = nullptr)
      : A(awaited, cb), target(node) {}

  bool is_complete(uint16_t node) { return (target < 0 || node == target); }

  Value get(uint16_t node) { return node; }
};

struct AWaitedTarget
    : AWaited<AWaitedTarget, false, std::tuple<unsigned, std::string>, unsigned,
              std::string> {
  int target;
  using A = AWaited<AWaitedTarget, false, std::tuple<unsigned, std::string>,
                    unsigned, std::string>;
  using A::Callback;
  using A::Value;

  AWaitedTarget(int index, bool awaited, const Callback &cb = nullptr)
      : A(awaited, cb), target(index) {}

  bool is_complete(unsigned index, const std::string &name) {
    return (target < 0 || target == index);
  }

  Value get(unsigned index, const std::string &name) { return {index, name}; }
};

using QueuedMessage = std::tuple<std::shared_ptr<Aseba::Message>, unsigned>;

struct AWaitedMessage
    : AWaited<AWaitedMessage, false,
              std::tuple<std::shared_ptr<Aseba::Message>, unsigned>,
              const std::shared_ptr<Aseba::Message> &, unsigned> {
  using A = AWaited<AWaitedMessage, false,
                    std::tuple<std::shared_ptr<Aseba::Message>, unsigned>,
                    const std::shared_ptr<Aseba::Message> &, unsigned>;
  using A::Callback;
  using A::Value;

  int target_source;
  int target_type;

  AWaitedMessage(int source, int type, bool awaited,
                 const Callback &cb = nullptr)
      : A(awaited, cb), target_source(source), target_type(type) {}

  bool is_complete(const std::shared_ptr<Aseba::Message> &msg,
                   unsigned target_index) {
    return (target_source < 0 || target_source == msg->source) &&
           (target_type < 0 || target_type == msg->type);
  }

  Value get(const std::shared_ptr<Aseba::Message> &msg, unsigned target_index) {
    return {msg, target_index};
  }
};

struct AWaitedNodes
    : AWaited<AWaitedNodes, true, std::set<unsigned>, unsigned> {

  using A = AWaited<AWaitedNodes, true, std::set<unsigned>, unsigned>;
  using A::Callback;
  using A::Value;

  std::set<unsigned> nodes;
  std::set<unsigned> candidates;
  int number;

  AWaitedNodes(std::set<unsigned> nodes, const std::set<unsigned> &candidates,
               int number, bool awaited, const Callback &cb = nullptr)
      : A(awaited, cb), nodes(nodes), candidates(candidates), number(number) {}

  bool is_complete(unsigned n) {
    if (candidates.size() && !candidates.count(n))
      return false;
    nodes.insert(n);
    return (number >= 0 && nodes.size() >= number);
  }

  Value get(unsigned n) { return {nodes}; }
};

inline unsigned compute_variables_size(const Aseba::VariablesMap &m) {
  unsigned c = 0;
  for (const auto &[k, v] : m) {
    const auto &[_, size] = v;
    c += size;
  }
  return c;
}

using VariablesMap = std::map<std::wstring, Aseba::VariablesDataVector>;

struct AWaitedVariables : public AWaited<AWaitedVariables, false, VariablesMap,
                                         const Aseba::Variables *> {

  using A =
      AWaited<AWaitedVariables, false, VariablesMap, const Aseba::Variables *>;
  using A::Callback;
  using A::Value;

  Aseba::VariablesMap d;
  std::vector<bool> rs;
  Aseba::VariablesDataVector vs;
  int target_node;

  AWaitedVariables(int node, const Aseba::VariablesMap &d, bool awaited,
                   const Callback &cb = nullptr)
      : A(awaited, cb), d(d), rs(compute_variables_size(d), false),
        vs(d.size()), target_node(node) {}

  bool is_complete(const Aseba::Variables *msg) {
    if (target_node >= 0 && target_node != msg->source)
      return false;
    const auto start = msg->start;
    const auto &values = msg->variables;
    std::copy(values.begin(), values.end(), vs.begin() + start);
    for (size_t i = start; i < start + values.size(); i++) {
      rs[i] = true;
    }
    return (std::find(rs.begin(), rs.end(), false) == rs.end());
  }

  Value get(const Aseba::Variables *msg) {
    VariablesMap m;
    for (const auto &[k, v] : d) {
      const auto [index, size] = v;
      m[k] = Aseba::VariablesDataVector(vs.begin() + index,
                                        vs.begin() + index + size);
    }
    return m;
  }
};

struct PyNodesManager : public Aseba::NodesManager {

  using VariablesCallback =
      std::function<void(const Aseba::VariablesDataVector &)>;
  using MessageCallback =
      std::function<void(const std::shared_ptr<Aseba::Message> &, unsigned)>;
  using NodeCallback = std::function<void(unsigned)>;
  using TargetCallback = std::function<void(unsigned, const std::string &)>;
  using MessageVector = std::vector<std::unique_ptr<Aseba::Message>>;
  using WriteLock = std::unique_lock<std::shared_mutex>;
  using ReadLock = std::shared_lock<std::shared_mutex>;
  DashelHub hub;
  std::unique_ptr<std::thread> ping_thread;
  std::atomic_bool stopped;
  std::queue<QueuedMessage> in_msgs;
  std::mutex in_msgs_mutex;
  std::condition_variable in_msgs_cv;
  std::unique_ptr<std::thread> process_msgs_thread;
  std::shared_mutex nodes_mutex;
  std::map<unsigned, Aseba::VariablesMap> variable_maps;
  std::map<unsigned, unsigned> variable_size_map;
  EventMaps event_maps;
  std::vector<MessageCallback> message_callbacks;
  std::vector<TargetCallback> target_connection_callbacks;
  std::vector<TargetCallback> target_disconnection_callbacks;
  std::vector<NodeCallback> node_connection_callbacks;
  std::vector<NodeCallback> node_disconnection_callbacks;
  std::vector<AWaitedTarget> awaited_target_connections;
  std::vector<AWaitedTarget> awaited_target_disconnections;
  std::vector<AWaitedNodes> scans;
  std::vector<AWaitedNodes> awaited_nodes;
  std::vector<AWaitedNode> awaited_node_disconnections;
  std::vector<AWaitedMessage> awaited_messages;
  std::vector<AWaitedVariables> awaited_variables;
  bool query;

  PyNodesManager(int port = -1, bool query = false)
      : hub(this, port), ping_thread(nullptr), stopped(true), in_msgs(),
        in_msgs_mutex(), in_msgs_cv(), process_msgs_thread(), nodes_mutex(),
        variable_maps(), message_callbacks(), target_connection_callbacks(),
        target_disconnection_callbacks(), node_connection_callbacks(),
        node_disconnection_callbacks(), awaited_target_connections(),
        awaited_target_disconnections(), scans(), awaited_nodes(),
        awaited_node_disconnections(), awaited_messages(), awaited_variables(),
        query(query) {}

  void run_process_msgs() {
    while (!stopped) {
      std::shared_ptr<Aseba::Message> msg;
      unsigned target_index;
      {
        std::unique_lock<std::mutex> lck(in_msgs_mutex);
        in_msgs_cv.wait(lck, []() { return true; });
        // in_msgs_cv.wait(lck, [this]() { return !in_msgs.empty(); });
        if (!in_msgs.empty()) {
          std::tie(msg, target_index) = in_msgs.front();
          in_msgs.pop();
        }
      }
      if (msg) {
        process_message(msg, target_index);
      }
    }
  }

  void received_msg(const Aseba::Message *msg, unsigned target_index) {
    std::shared_ptr<Aseba::Message> smsg;
    smsg.reset(std::move(msg->clone()));
    {
      std::unique_lock<std::mutex> lck(in_msgs_mutex);
      in_msgs.push({smsg, target_index});
      in_msgs_cv.notify_all();
    }
  }

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

  void connectionClosed(unsigned index, const std::string &name) {
    for (const auto &cb : target_disconnection_callbacks) {
      py::gil_scoped_acquire acquire;
      cb(index, name);
    }
    AWaitedTarget::check(index, name, awaited_target_disconnections);
    // TODO: Should only remove nodes associated to that stream.
    // WriteLock rlock;
    // for (auto &[k, v] : nodes) {
    //   v.connected = false;
    //   nodeDisconnected(k);
    // }
    // nodes.clear();
  }

  void connectionCreated(unsigned index, const std::string &name) {
    for (const auto &cb : target_connection_callbacks) {
      py::gil_scoped_acquire acquire;
      cb(index, name);
    }
    AWaitedTarget::check(index, name, awaited_target_connections);
  }

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

  void remove_message_callback(int index) {
    if (index < 0) {
      index = std::max(0, static_cast<int>(message_callbacks.size()) + index);
    }
    if (index < message_callbacks.size()) {
      message_callbacks.erase(std::begin(message_callbacks) + index);
    }
  }

  void clear_message_callbacks() { message_callbacks.clear(); }

  void add_node_connection_callback(const NodeCallback &cb) {
    node_connection_callbacks.push_back(cb);
  }

  void add_node_disconnection_callback(const NodeCallback &cb) {
    node_disconnection_callbacks.push_back(cb);
  }

  void add_target_connection_callback(const TargetCallback &cb) {
    target_connection_callbacks.push_back(cb);
  }

  void add_target_disconnection_callback(const TargetCallback &cb) {
    target_disconnection_callbacks.push_back(cb);
  }

  std::shared_ptr<Event>
  make_event_from_msg(const std::shared_ptr<Aseba::Message> &msg) const {
    return Event::from_msg(msg, event_maps);
  }

  void add_event_callback(const EventCallback &cb) {
    if (cb) {
      MessageCallback mcb =
          [cb, this](const std::shared_ptr<Aseba::Message> &msg, unsigned) {
            auto e = make_event_from_msg(msg);
            if (e) {
              cb(e);
            }
          };
      message_callbacks.push_back(mcb);
    }
  }

  void process_message(const std::shared_ptr<Aseba::Message> &msg,
                       unsigned target_index) {
    {
      WriteLock lock;
      std::optional<uint16_t> msg_type = std::nullopt;
      if (msg->type == ASEBA_MESSAGE_NODE_PRESENT && !query &&
          nodes.count(msg->source) == 0) {
        // HACK to skip sending a GetNodeDescription.
        msg_type = msg->type;
        msg->type = 0;
      }
      Aseba::NodesManager::processMessage(msg.get());
      if (msg_type) {
        msg->type = *msg_type;
      }
    }
    for (const auto &cb : message_callbacks) {
      py::gil_scoped_acquire acquire;
      cb(msg, target_index);
    }
    AWaitedMessage::check(msg, target_index, awaited_messages);
    if (const auto v_msg =
            dynamic_cast<const Aseba::NodePresent *>(msg.get())) {
      AWaitedNodes::check(v_msg->source, scans);
    }
    if (const auto v_msg = dynamic_cast<const Aseba::Variables *>(msg.get())) {
      AWaitedVariables::check(v_msg, awaited_variables);
    }
  }

  bool connect_and_start(const std::string &target, unsigned wait_ms,
                         int max_retries, bool ping = true) {
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
      start(ping);
    }
    return connected;
  }

  void run_ping(unsigned interval_ms) {
    while (!stopped) {
      pingNetwork();
      std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }
  }

  void ping() {
    Aseba::ListNodes listNodes;
    sendMessage(listNodes);
  }

  void start(bool ping = true) {
    if (stopped) {
      stopped = false;
      if (ping) {
        ping_thread = std::make_unique<std::thread>(&PyNodesManager::run_ping,
                                                    this, 1000);
      }
      process_msgs_thread = std::make_unique<std::thread>(
          &PyNodesManager::run_process_msgs, this);
      hub.start();
    }
  }

  std::map<unsigned, std::string> get_connected_targets() const {
    return hub.get_targets();
  }

  bool connect(const std::string &target) {
    // if (stream) {
    //   std::cerr << "Already connected to " << stream->getTargetName()
    //             << std::endl;
    //   return false;
    // }
    try {
      hub.connect(target);
      return true;
    } catch (const Dashel::DashelException &e) {
      std::cerr << e.what() << std::endl;
      return false;
    }
  }

  bool is_connected() const { return hub.is_connected(); }

  void clear_in_msgs() {
    std::unique_lock<std::mutex> lck(in_msgs_mutex);
    std::queue<QueuedMessage> q;
    in_msgs.swap(q);
    in_msgs_cv.notify_all();
  }

  void stop() {
    stopped = true;
    clear_in_msgs();
    if (process_msgs_thread) {
      process_msgs_thread->join();
    }
    if (ping_thread) {
      ping_thread->join();
    }
    ping_thread = nullptr;
    process_msgs_thread = nullptr;
    hub.stop();
  }

  void close() {
    // if (stream) {
    //   hub.closeStream(stream);
    // }
    hub.close();
    // stream = nullptr;
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
    send_message(message);
  }

  void send_message(const Aseba::Message &message, int target = -1,
                    const std::set<unsigned> &exclude_target_indices = {}) {
    hub.sendMessage(&message, target, exclude_target_indices);
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
    const auto [index, size] = m.at(name);
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
      mcb = [cb](const std::shared_ptr<Aseba::Message> &msg, unsigned target) {
        if (const auto v_msg =
                std::dynamic_pointer_cast<Aseba::Variables>(msg)) {
          cb(v_msg->variables);
        }
      };
    }
    const auto [msg, _] = get_message(
        nodeId, static_cast<int>(ASEBA_MESSAGE_VARIABLES), wait_ms, mcb);
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

  unsigned get_variables_size(int nodeId) const {
    if (!variable_size_map.count(nodeId)) {
      return 0;
    }
    return variable_size_map.at(nodeId);
  }

  std::set<unsigned> scan(int number = -1, unsigned wait_ms = 1000,
                          const AWaitedNodes::Callback &cb = nullptr) {
    if (!wait_ms && !cb) {
      return {};
    }
    auto &awaited = scans.emplace_back(
        std::set<unsigned>(), std::set<unsigned>(), number, wait_ms, cb);
    ping();
    if (!wait_ms) {
      return {};
    }
    auto future = awaited.value->get_future();
    py::gil_scoped_release release;
    const auto s = future.wait_for(std::chrono::milliseconds(wait_ms));
    if (s == std::future_status::ready) {
      return future.get();
    }   
    awaited.complete = true;
    return awaited.nodes;
  }

  std::optional<VariablesMap>
  get_variables(int nodeId, unsigned wait_ms,
                const AWaitedVariables::Callback &cb = nullptr) {
    if (!variable_size_map.count(nodeId)) {
      return std::nullopt;
    }
    if (!wait_ms && !cb) {
      return std::nullopt;
    }
    const auto &m = getVariablesMap(nodeId);
    const unsigned size = variable_size_map.at(nodeId);
    auto &awaited = awaited_variables.emplace_back(nodeId, m, wait_ms, cb);
    get_variable_at_index(nodeId, 0, size, 0, nullptr);
    if (!wait_ms) {
      return std::nullopt;
    }
    auto future = awaited.value->get_future();
    py::gil_scoped_release release;
    const auto s = future.wait_for(std::chrono::milliseconds(wait_ms));
    if (s == std::future_status::ready) {
      return future.get();
    }
    return std::nullopt;
  }

  void load_script(unsigned nodeId, const std::string &code,
                   const std::vector<Aseba::NamedValue> &events = {},
                   const std::vector<Aseba::NamedValue> &constants = {}) {

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
    {
      WriteLock lock;
      variable_maps[nodeId] = *(compiler.getVariablesMap());
      variable_size_map[nodeId] = compute_variables_size(variable_maps[nodeId]);
      unsigned i = 0;
      for (const auto &e : events) {
        event_maps[nodeId][e.name] = i++;
      }
    }
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
    variable_size_map[nodeId] = compute_variables_size(variable_maps[nodeId]);
  }

  void nodeConnected(unsigned nodeId) override {
    for (const auto &cb : node_connection_callbacks) {
      py::gil_scoped_acquire acquire;
      cb(nodeId);
    }
    AWaitedNodes::check(nodeId, awaited_nodes);
  }

  void nodeDisconnected(unsigned nodeId) override {
    for (const auto &cb : node_disconnection_callbacks) {
      py::gil_scoped_acquire acquire;
      cb(nodeId);
    }
    AWaitedNode::check(nodeId, awaited_node_disconnections);
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
      mcb = [cb, this](const std::shared_ptr<Aseba::Message> &msg,
                       unsigned target) {
        auto e = Event::from_msg(msg, event_maps);
        if (e) {
          cb(e);
        }
      };
    }
    const auto [msg, _] = get_message(nodeId, type, wait_ms, mcb);
    return Event::from_msg(msg, event_maps);
  }

  AWaitedMessage::Value
  get_message(int nodeId = -1, int type = -1, int wait_ms = 0,
              const AWaitedMessage::Callback &cb = nullptr) {
    if (!wait_ms && !cb) {
      return {nullptr, 0};
    }
    auto &awaited_msg =
        awaited_messages.emplace_back(nodeId, type, wait_ms, cb);
    if (!wait_ms) {
      return {nullptr, 0};
    }
    auto future = awaited_msg.value->get_future();
    py::gil_scoped_release release;
    const auto s = future.wait_for(std::chrono::milliseconds(wait_ms));
    if (s == std::future_status::ready) {
      return future.get();
    }
    return {nullptr, 0};
  }

  std::set<unsigned> wait_nodes(const std::set<unsigned> &candidates,
                                int number, unsigned wait_ms,
                                const AWaitedNodes::Callback &cb = nullptr) {
    std::set<unsigned> c_nodes;
    if (number < 0 && candidates.size()) {
      number = static_cast<int>(candidates.size());
    }
    {
      ReadLock lock;
      for (auto &[k, n] : nodes) {
        if (n.isComplete() && n.connected &&
            (candidates.size() == 0 || candidates.count(k))) {
          c_nodes.insert(k);
          number--;
          if (number == 0) {
            break;
          }
        }
      }
    }
    if (cb && (c_nodes.size() || number == 0)) {
      cb(c_nodes, number == 0);
    }
    if (number == 0) {
      return c_nodes;
    }
    if (!wait_ms && !cb) {
      return c_nodes;
    }
    auto &awaited =
        awaited_nodes.emplace_back(c_nodes, candidates, number, wait_ms, cb);
    if (!wait_ms) {
      return c_nodes;
    }
    auto future = awaited.value->get_future();
    py::gil_scoped_release release;
    const auto s = future.wait_for(std::chrono::milliseconds(wait_ms));
    if (s == std::future_status::ready) {
      return future.get();
    } 
    awaited.complete = true;
    return awaited.nodes;
  }

  std::optional<unsigned>
  wait_node_connection(int nodeId = -1, unsigned wait_ms = 1000,
                       const AWaitedNode::Callback &cb = nullptr) {

    std::set<unsigned> c_nodes;
    if (nodeId > 0) {
      c_nodes.insert(static_cast<unsigned>(nodeId));
    }
    AWaitedNodes::Callback ncb = nullptr;
    if (cb) {
      ncb = [cb](const std::set<unsigned> &value, bool complete) {
        if (value.size() && complete) {
          cb(*(value.begin()));
        }
      };
    }
    const auto rs = wait_nodes(c_nodes, 1, wait_ms, ncb);
    if (rs.size()) {
      return *(rs.begin());
    }
    return std::nullopt;
  }

  std::tuple<int, std::string>
  wait_target_connection(int index = -1, unsigned wait_ms = 1000,
                         const AWaitedTarget::Callback &cb = nullptr) {
    if (hub.is_connected_to(index)) {
      return {index, hub.get_target_name(index)};
    }
    std::tuple<int, std::string> r{-1, ""};
    if (!wait_ms && !cb) {
      return r;
    }
    auto &awaited = awaited_target_connections.emplace_back(index, wait_ms, cb);
    if (!wait_ms) {
      return r;
    }
    auto future = awaited.value->get_future();
    py::gil_scoped_release release;
    const auto s = future.wait_for(std::chrono::milliseconds(wait_ms));
    if (s == std::future_status::ready) {
      r = future.get();
    }
    return r;
  }

  std::tuple<int, std::string>
  wait_target_disconnection(int index = -1, unsigned wait_ms = 1000,
                            const AWaitedTarget::Callback &cb = nullptr) {
    std::tuple<int, std::string> r{-1, ""};
    if (!hub.is_connected_to(index)) {
      return r;
    }
    if (!wait_ms && !cb) {
      return r;
    }
    auto &awaited =
        awaited_target_disconnections.emplace_back(index, wait_ms, cb);
    if (!wait_ms) {
      return r;
    }
    auto future = awaited.value->get_future();
    py::gil_scoped_release release;
    const auto s = future.wait_for(std::chrono::milliseconds(wait_ms));
    if (s == std::future_status::ready) {
      r = future.get();
    }
    return r;
  }

  std::optional<unsigned>
  wait_node_disconnection(int nodeId = -1, unsigned wait_ms = 1000,
                          const AWaitedNode::Callback &cb = nullptr) {
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
    auto &awaited_node =
        awaited_node_disconnections.emplace_back(nodeId, wait_ms, cb);
    if (!wait_ms) {
      return std::nullopt;
    }
    auto future = awaited_node.value->get_future();
    py::gil_scoped_release release;
    const auto s = future.wait_for(std::chrono::milliseconds(wait_ms));
    if (s == std::future_status::ready) {
      return future.get();
    }
    return std::nullopt;
  }
};

#endif
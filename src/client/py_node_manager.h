#ifndef PY_NODE_MANAGER_H_GUARD
#define PY_NODE_MANAGER_H_GUARD

#include "aseba/common/msg/NodesManager.h"
#include "aseba/common/msg/msg.h"
#include "aseba/compiler/compiler.h"
#include "aseba/transport/dashel_plugins/dashel-plugins.h"
#ifdef ZEROCONF
#include "aseba/common/zeroconf/zeroconf-dashelhub.h"
#endif
#include "dashel/dashel.h"

#include "awaited.h"
#include "utils.h"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <chrono>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>

inline std::shared_ptr<Aseba::Message> copy_message(const Aseba::Message &msg) {
  std::shared_ptr<Aseba::Message> message;
  message.reset(std::move(msg.clone()));
  return message;
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
using InMessage = std::tuple<std::shared_ptr<Aseba::Message>, unsigned>;
using OutMessage =
    std::tuple<std::shared_ptr<Aseba::Message>, std::set<Dashel::Stream *>>;

using TargetDescriptionCallback =
    std::function<void(const Aseba::TargetDescription *)>;
using VariablesCallback =
    std::function<void(const Aseba::VariablesDataVector &)>;
using MessageCallback =
    std::function<void(const std::shared_ptr<Aseba::Message> &, unsigned)>;
using NodeCallback = std::function<void(unsigned)>;
using TargetCallback = std::function<void(unsigned, const std::string &)>;
using MessageVector = std::vector<std::unique_ptr<Aseba::Message>>;
using Guard = std::lock_guard<std::recursive_mutex>;
template <typename T> class Queue {

  using Callback = std::function<void(const T &)>;

  bool stopped;
  std::queue<T> queue;
  std::mutex mutex;
  std::condition_variable cv;
  std::unique_ptr<std::thread> thread;

public:

  Queue() : stopped(true), queue(), mutex(), cv(), thread(nullptr) {}

  std::optional<T> get() {
    std::unique_lock<std::mutex> lck(mutex);
    cv.wait(lck, [this]() { 
      return !queue.empty() || stopped; 
    });
    // cv.wait(lck, []() { return true; });
    if (!queue.empty()) {
      auto value = queue.front();
      queue.pop();
      return value;
    }
    return std::nullopt;
  }

  void put(const T &item) {
    std::unique_lock<std::mutex> lck(mutex);
    queue.push(item);
    cv.notify_one();
  }

  void clear() {
    std::lock_guard<std::mutex> lck(mutex);
    std::queue<T> q;
    queue.swap(q);
  }

  void process(const Callback &cb) {
    while (!stopped) {
      auto r = get();
      if (r) {
        cb(*r);
      }
    }
  }

  void start(const Callback &cb) {
    stopped = false;
    thread = std::make_unique<std::thread>(&Queue<T>::process, this, cb);
  }

  void stop() {
    {
      std::lock_guard<std::mutex> lck(mutex);
      std::queue<T> q;
      queue.swap(q);
      stopped = true;
    }
    cv.notify_one();
    if (thread) {
      thread->join();
    }
    thread = nullptr;
  }
};

struct PyNodesManager : public Aseba::NodesManager, public Dashel::Hub {
  std::atomic_bool stopped;
  std::unique_ptr<std::thread> thread;
  std::unique_ptr<std::thread> ping_thread;
  bool query;
  Queue<InMessage> in_msgs;
  Queue<OutMessage> out_msgs;

  std::recursive_mutex mutex;
  std::map<unsigned, Aseba::VariablesMap> variable_maps;
  std::map<unsigned, unsigned> variable_size_map;
  EventMaps event_maps;

  std::map<Dashel::Stream *, unsigned> stream_indices;
  Dashel::Stream *in_stream;
  unsigned stream_index;

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

#ifdef ZEROCONF
  Aseba::DashelhubZeroconf zeroconf;
#endif

  PyNodesManager(int port = -1, bool query = false)
      : Aseba::NodesManager(), Dashel::Hub(true), stopped(true),
        thread(nullptr), ping_thread(nullptr), query(query), in_msgs(),
        out_msgs(), mutex(), variable_maps(), stream_indices(),
        in_stream(nullptr), stream_index(0), message_callbacks(),
        target_connection_callbacks(), target_disconnection_callbacks(),
        node_connection_callbacks(), node_disconnection_callbacks(),
        awaited_target_connections(), awaited_target_disconnections(), scans(),
        awaited_nodes(), awaited_node_disconnections(), awaited_messages(),
        awaited_variables()
#ifdef ZEROCONF
        ,
        zeroconf(dynamic_cast<Dashel::Hub &>(*this))
#endif
  {
    Dashel::initPlugins();
    if (port > 0) {
      in_stream = connect("tcpin:port=" + std::to_string(port));
      add_stream(in_stream);
    }
  }

  // +++++++++++++++ Aseba::NodesManager specialization +++++++++++++++

  void sendMessage(const Aseba::Message &message) override {
    send_message(copy_message(message));
  }

  // --------------- Aseba::NodesManager specialization ---------------

  // +++++++++++++++ Dashel::Hub specialization +++++++++++++++

  void incomingData(Dashel::Stream *stream) override {
#ifdef ZEROCONF
    if (zeroconf.isStreamHandled(stream)) {
      try {
        zeroconf.dashelIncomingData(stream);
      } catch (const std::exception &e) {
      }
      return;
    }
#endif // ZEROCONF
    Aseba::Message *message = nullptr;
    try {
      message = Aseba::Message::receive(stream);
    } catch (Dashel::DashelException e) {
      return;
    } catch (std::runtime_error e) {
      return;
    }
    if (message && !stopped) {
      if (!stream_indices.count(stream)) {
        std::cerr << "Unindexed stream " << stream->getTargetName()
                  << std::endl;
        return;
      }
      // TODO: do I really need to copy?
      in_msgs.put({copy_message(*message), stream_indices.at(stream)});
      delete message;
    }
  }

  void connectionCreated(Dashel::Stream *stream) override {
    add_stream(stream);
    if (stream_indices.count(stream)) {
      const auto index = stream_indices.at(stream);
      const auto name = stream->getTargetName();
      for (const auto &cb : target_connection_callbacks) {
        pybind11::gil_scoped_acquire acquire;
        cb(index, name);
      }
      AWaitedTarget::check(index, name, awaited_target_connections);
    }
  }

  void connectionClosed(Dashel::Stream *stream, bool abnormal) override {
#ifdef ZEROCONF
    zeroconf.dashelConnectionClosed(stream);
#endif // ZEROCONF
    if (stream_indices.count(stream)) {
      const auto index = stream_indices.at(stream);
      const auto name = stream->getTargetName();
      for (const auto &cb : target_disconnection_callbacks) {
        pybind11::gil_scoped_acquire acquire;
        cb(index, name);
      }
      AWaitedTarget::check(index, name, awaited_target_disconnections);
    }
    remove_stream(stream);
  }

  // --------------- Dashel::Hub specialization ---------------

  void process_in_message(const InMessage &in_msg) {
    const auto &[msg, target_index] = in_msg;
    {
      Guard lock(mutex);
      std::optional<uint16_t> msg_type = std::nullopt;
      if (msg->type == ASEBA_MESSAGE_NODE_PRESENT && !query &&
          nodes.count(msg->source) == 0) {
        // HACK to skip sending a GetNodeDescription.
        msg_type = msg->type;
        msg->type = 0;
      }
      // HACK
      auto *description = dynamic_cast<Aseba::Description *>(msg.get());
      if (description) {
        description->protocolVersion = ASEBA_PROTOCOL_VERSION;
      }
      Aseba::NodesManager::processMessage(msg.get());
      if (msg_type) {
        msg->type = *msg_type;
      }
    }
    for (const auto &cb : message_callbacks) {
      pybind11::gil_scoped_acquire acquire;
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

  void process_out_message(const OutMessage &out_msg) {
    const auto &[message, out_streams] = out_msg;
    lock();
    for (auto stream : out_streams) {
      message->serialize(stream);
      stream->flush();
    }
    unlock();
  }

#ifdef ZEROCONF
  void run_zeroconf() {
    while (!stopped && zeroconf.dashelStep(-1))
      ;
  }
#endif

  void start(bool ping = true) {
    if (stopped) {
      stopped = false;
#ifdef ZEROCONF
      thread =
          std::make_unique<std::thread>(&PyNodesManager::run_zeroconf, this);
#else
      thread = std::make_unique<std::thread>(&Dashel::Hub::run, this);
#endif
      if (ping) {
        ping_thread = std::make_unique<std::thread>(&PyNodesManager::run_ping,
                                                    this, 1000);
      }
      in_msgs.start(std::bind(&PyNodesManager::process_in_message, this,
                              std::placeholders::_1));
      out_msgs.start(std::bind(&PyNodesManager::process_out_message, this,
                               std::placeholders::_1));
    }
  }

  void stop() {
    stopped = true;
    out_msgs.clear();
    out_msgs.stop();
    in_msgs.clear();
    in_msgs.stop();
    if (ping_thread) {
      ping_thread->join();
    }
    ping_thread = nullptr;
    Dashel::Hub::stop();
    if (thread) {
      thread->join();
    }
    thread = nullptr;
    {
      Guard lock(mutex);
      nodes.clear();
    }
  }

  void close() {
    auto ss = dataStreams;
    for (auto stream : ss) {
      remove_stream(stream);
      closeStream(stream);
    }
  }

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

  std::map<unsigned, std::string> get_connected_targets() const {
    std::map<unsigned, std::string> targets;
    for (const auto [stream, index] : stream_indices) {
      targets[index] = stream->getTargetName();
    }
    return targets;
  }

#ifdef ZEROCONF
  void advertise(const std::string &name,
                 const std::map<unsigned, Aseba::TargetDescription> &ns) {
    if (!in_stream) {
      return;
    }
    std::vector<unsigned int> ids;
    std::vector<unsigned int> pids;
    std::string nname = "";
    unsigned protocolVersion{ASEBA_PROTOCOL_VERSION};
    for (auto const &[id, node] : ns) {
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

  bool has_node(unsigned nodeId) {
    Guard lock(mutex);
    return nodes.count(nodeId) > 0;
  }

  std::vector<unsigned> get_nodes() {
    Guard lock(mutex);
    std::vector<unsigned> ns;
    for (auto &[k, v] : nodes) {
      ns.push_back(k);
    }
    return ns;
  }

  std::map<unsigned, Aseba::TargetDescription> get_nodes_() {
    Guard lock(mutex);
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

  bool connect_and_start(const std::string &target, unsigned wait_ms,
                         int max_retries, bool ping = true) {
    bool failed = false;
    bool connected = false;
    max_retries = std::max(max_retries, 0);
    while (max_retries >= 0) {
      connected = try_to_connect(target);
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

  bool try_to_connect(const std::string &target) {
    try {
      connect(target);
      return true;
    } catch (const Dashel::DashelException &e) {
      std::cerr << e.what() << std::endl;
      return false;
    }
  }

  void clear_in_msgs() { in_msgs.clear(); }

  void stop_and_close() {
    stop();
    close();
  }

  void send_event(unsigned node_id, unsigned index,
                  const Aseba::VariablesDataVector &data) {
    send_user_message(index, data);
  }

  void emit_event(unsigned node_id, const std::wstring &name,
                  const Aseba::VariablesDataVector &data) {
    Guard lock(mutex);
    if (event_maps[node_id].count(name)) {
      const auto index = event_maps[node_id].at(name);
      send_event(node_id, index, data);
    }
  }

  template <typename T, typename... Ts> void send_message_of_type(Ts... args) {
    send_message(std::make_shared<T>(args...));
  }

  void send_message(const std::shared_ptr<Aseba::Message> &message,
                    int stream_index = -1,
                    const std::set<unsigned> &exclude_stream_indices = {}) {
    std::set<Dashel::Stream *> out_streams =
        get_streams(stream_index, exclude_stream_indices);
    if (out_streams.size()) {
      out_msgs.put({message, out_streams});
    }
  }

  void send_user_message(unsigned type,
                         const Aseba::VariablesDataVector &data) {
    send_message_of_type<Aseba::UserMessage>(type, data);
  }

  void set_variable_at_index(unsigned nodeId, unsigned index,
                             const Aseba::VariablesDataVector &value) {
    send_message_of_type<Aseba::SetVariables>(nodeId, index, value);
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
    send_message_of_type<Aseba::GetVariables>(nodeId, index, size);
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
    send_message_of_type<Aseba::ListNodes>();
    if (!wait_ms) {
      return {};
    }
    auto future = awaited.value->get_future();
    pybind11::gil_scoped_release release;
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
    pybind11::gil_scoped_release release;
    const auto s = future.wait_for(std::chrono::milliseconds(wait_ms));
    if (s == std::future_status::ready) {
      return future.get();
    }
    return std::nullopt;
  }

  const Aseba::TargetDescription *
  query_description(unsigned nodeId, unsigned wait_ms = 0,
                    const TargetDescriptionCallback &cb = nullptr) {
    const auto d = getDescription(nodeId);
    if (d)
      return d;
    send_message_of_type<Aseba::GetNodeDescription>(nodeId);
    AWaitedNode::Callback ncb;
    if (cb) {
      ncb = [cb, this](unsigned node) {
        const auto desc = getDescription(node);
        if (desc) {
          cb(desc);
        }
      };
    }
    auto node = wait_node_connection(nodeId, wait_ms, ncb);
    if (node) {
      return getDescription(*node);
    }
    return nullptr;
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
      Guard lock(mutex);
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
      send_message(std::move(msg));
    }
  }

  const Aseba::TargetDescription *getDescription(unsigned nodeId) {
    Guard lock(mutex);
    bool ok;
    auto r = Aseba::NodesManager::getDescription(nodeId, &ok);
    if (!ok) {
      return nullptr;
    }
    return r;
  }

  inline static Aseba::VariablesMap empty_variable_map = {};

  const Aseba::VariablesMap &getVariablesMap(unsigned nodeId) {
    Guard lock(mutex);
    if (variable_maps.count(nodeId)) {
      return variable_maps.at(nodeId);
    }
    return empty_variable_map;
  }

  std::vector<std::wstring> getVariables(unsigned nodeId) {
    const auto &m = getVariablesMap(nodeId);
    std::vector<std::wstring> vs(m.size());
    size_t i = 0;
    for (const auto &[name, _] : m) {
      vs[i++] = name;
    }
    return vs;
  }

  std::vector<std::wstring> getEvents(unsigned nodeId) {
    Guard lock(mutex);
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
      pybind11::gil_scoped_acquire acquire;
      cb(nodeId);
    }
    AWaitedNodes::check(nodeId, awaited_nodes);
  }

  void nodeDisconnected(unsigned nodeId) override {
    for (const auto &cb : node_disconnection_callbacks) {
      pybind11::gil_scoped_acquire acquire;
      cb(nodeId);
    }
    AWaitedNode::check(nodeId, awaited_node_disconnections);
    variable_maps.erase(nodeId);
    event_maps.erase(nodeId);
  }

  std::shared_ptr<Event> get_event(unsigned nodeId, const std::wstring &name,
                                   int wait_ms = 0,
                                   const EventCallback &cb = nullptr) {
    // TODO: protect with guard
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
    pybind11::gil_scoped_release release;
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
      Guard lock(mutex);
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
    pybind11::gil_scoped_release release;
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
    if (is_connected_to(index)) {
      return {index, get_target_name(index)};
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
    pybind11::gil_scoped_release release;
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
    if (!is_connected_to(index)) {
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
    pybind11::gil_scoped_release release;
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
      Guard lock(mutex);
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
    pybind11::gil_scoped_release release;
    const auto s = future.wait_for(std::chrono::milliseconds(wait_ms));
    if (s == std::future_status::ready) {
      return future.get();
    }
    return std::nullopt;
  }

  void advertise_nodes() {
#ifdef ZEROCONF
    advertise("Test 33333", get_nodes_());
#endif
  }

  void deadvertise_nodes() {
#ifdef ZEROCONF
    deadvertise("Test 33333");
#endif
  }
};

#endif
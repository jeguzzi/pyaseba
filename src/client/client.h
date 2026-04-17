#ifndef CLIENT_H_GUARD
#define CLIENT_H_GUARD

#include "aseba/common/msg/msg.h"
#include "aseba/compiler/compiler.h"
#include "aseba/transport/dashel_plugins/dashel-plugins.h"
#ifdef ZEROCONF
#include "advertise.h"
#include "aseba/common/zeroconf/zeroconf-dashelhub.h"
#endif
#include "aseba/common/productids.h"
#include "dashel/dashel.h"

#include "awaited.h"
#include "description_manager.h"
#include "queue.h"
#include "utils.h"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <tuple>

inline std::shared_ptr<Aseba::Message> copy_message(const Aseba::Message &msg) {
  std::shared_ptr<Aseba::Message> message;
  message.reset(std::move(msg.clone()));
  return message;
}

struct Event {
  unsigned source;
  std::wstring name;
  Aseba::VariablesDataVector data;

  Event(unsigned source, const std::wstring &name,
        const Aseba::VariablesDataVector &data)
      : source(source), name(name), data(data) {}
};

using EventCallback = std::function<void(const std::shared_ptr<Event> &)>;
using InMessage = std::tuple<std::shared_ptr<Aseba::Message>, unsigned>;
using OutMessage =
    std::tuple<std::shared_ptr<Aseba::Message>, std::set<Dashel::Stream *>>;

using TargetDescriptionCallback = std::function<void(const ClientNode *)>;
using VariablesCallback =
    std::function<void(const Aseba::VariablesDataVector &)>;
using MessageCallback =
    std::function<void(const std::shared_ptr<Aseba::Message> &, unsigned)>;
using NodeCallback = std::function<void(unsigned, unsigned)>;
using TargetCallback = std::function<void(unsigned, const std::string &)>;
using MessageVector = std::vector<std::unique_ptr<Aseba::Message>>;

struct Client : public DescriptionManager<Client>, public Dashel::Hub {
  int port;
  std::atomic_bool stopped;
  std::atomic_uint ping_ms;
  std::unique_ptr<std::thread> thread;
  std::unique_ptr<std::thread> ping_thread;
  Queue<InMessage> in_msgs;
  Queue<OutMessage> out_msgs;

  std::map<Dashel::Stream *, unsigned> stream_indices;
  std::map<std::string, unsigned> target_indices;
  Dashel::Stream *in_stream;
  unsigned stream_index;
  std::set<Dashel::Stream *> streams_to_be_closed;

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

  explicit Client(
      int port = -1, unsigned ping_period_ms = 1000, bool query = false,
      unsigned min_protocol_version = ASEBA_MIN_TARGET_PROTOCOL_VERSION,
      unsigned max_protocol_version = ASEBA_PROTOCOL_VERSION)
      : DescriptionManager<Client>(query, min_protocol_version,
                                   max_protocol_version),
        Dashel::Hub(true), port(port), stopped(true), ping_ms(ping_period_ms),
        thread(nullptr), ping_thread(nullptr), in_msgs(), out_msgs(),
        stream_indices(), target_indices(), in_stream(nullptr), stream_index(1),
        streams_to_be_closed(), message_callbacks(),
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
      start();
    }
  }

  ~Client() { stop_and_close(); }

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
    } catch (Dashel::DashelException &) {
      return;
    } catch (std::runtime_error &) {
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
    // std::cout << "connectionCreated " << stream->getTargetName() <<
    // std::endl;
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
    // std::cout << "connectionClosed " << stream->getTargetName() << "("
    //           << abnormal << ")" << std::endl;
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
    if (stream_indices.count(stream)) {
      disconnect(stream_indices.at(stream));
    }
    remove_stream(stream);
  }

  // --------------- Dashel::Hub specialization ---------------

  void process_in_message(const InMessage &in_msg) {
    const auto &[msg, target_index] = in_msg;
    process_message(msg.get(), target_index);
    for (const auto &cb : message_callbacks) {
      pybind11::gil_scoped_acquire acquire;
      cb(msg, target_index);
    }
    AWaitedMessage::check(msg, target_index, awaited_messages);
    if (const auto v_msg =
            dynamic_cast<const Aseba::NodePresent *>(msg.get())) {
      AWaitedNodes::check(v_msg->source, target_index, scans);
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
    while (!stopped && zeroconf.dashelStep(1)) {
      for (auto stream : streams_to_be_closed) {
        connectionClosed(stream, false);
        closeStream(stream);
      }
      streams_to_be_closed.clear();
    }
  }
#endif

  void start() {
    if (stopped) {
      stopped = false;
#ifdef ZEROCONF
      thread = std::make_unique<std::thread>(&Client::run_zeroconf, this);
#else
      thread = std::make_unique<std::thread>(&Dashel::Hub::run, this);
#endif
      if (ping_ms && !ping_thread) {
        ping_thread = std::make_unique<std::thread>(&Client::run_ping, this);
      }
      in_msgs.start(
          std::bind(&Client::process_in_message, this, std::placeholders::_1));
      out_msgs.start(
          std::bind(&Client::process_out_message, this, std::placeholders::_1));
    }
  }

  unsigned get_ping_period_ms() const { return ping_ms; }

  void set_ping_period_ms(unsigned value_ms = 1000) {
    if (value_ms == ping_ms)
      return;
    ping_ms = value_ms;
    if (value_ms && !ping_thread) {
      ping_thread = std::make_unique<std::thread>(&Client::run_ping, this);
    }
  }

  void stop(bool hub = true) {
    stopped = true;
    out_msgs.clear();
    out_msgs.stop();
    in_msgs.clear();
    in_msgs.stop();
    set_ping_period_ms(0);
    if (ping_thread) {
      ping_thread->join();
      ping_thread = nullptr;
    }
    if (hub) {
      Dashel::Hub::stop();
    }
    if (thread) {
      thread->join();
    }
    thread = nullptr;
    {
      reset();
    }
  }

  void close_all() {
    auto ss = dataStreams;
    for (auto stream : ss) {
      remove_stream(stream);
      closeStream(stream);
    }
  }

  bool close(unsigned index) {
    auto ss = stream_indices;
    bool r = false;
    for (auto [stream, i] : ss) {
      if (i == index) {
        streams_to_be_closed.insert(stream);
        if (dataStreams.size() == 1 || !in_stream) {
          stop(false);
        }
        wait_target_disconnection(index, 1000);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        r = true;
        break;
      }
    }
    return r;
  }

  void add_stream(Dashel::Stream *stream) {
    if (stream_indices.count(stream) == 0) {
      unsigned index;
      const auto name = stream->getTargetName();
      // std::cout << "add_stream " << name << std::endl;
      if (target_indices.count(name)) {
        index = target_indices.at(name);
      } else {
        index = stream_index;
        stream_index++;
        target_indices.emplace(name, index);
      }
      stream_indices.emplace(stream, index);
    }
  }

  void remove_stream(Dashel::Stream *stream) {
    if (stream_indices.count(stream)) {
      stream_indices.erase(stream);
    }
  }

  std::set<Dashel::Stream *>
  get_streams(const std::set<unsigned> &include = {},
              const std::set<unsigned> &exclude = {}) {
    std::set<Dashel::Stream *> streams;
    for (auto stream : dataStreams) {
      if (!stream_indices.count(stream)) {
        std::cerr << "Unindexed stream " << stream->getTargetName()
                  << std::endl;
        continue;
      }
      const auto i = stream_indices.at(stream);
      if (include.size() && (include.count(i) == 0)) {
        continue;
      }
      if (exclude.count(i)) {
        continue;
      }
      streams.insert(stream);
    }
    return streams;
  }

  std::set<unsigned>
  get_stream_indices(const std::set<unsigned> &include = {},
                     const std::set<unsigned> &exclude = {}) {
    std::set<unsigned> indices;
    for (auto [stream, i] : stream_indices) {
      if (include.size() && (include.count(i) == 0)) {
        continue;
      }
      if (exclude.count(i)) {
        continue;
      }
      indices.insert(i);
    }
    return indices;
  }

  Dashel::Stream *get_stream(unsigned index) const {
    for (const auto [stream, i] : stream_indices) {
      if (index == i)
        return stream;
    }
    return nullptr;
  }

  std::string get_target_name(unsigned index) const {
    const auto stream = get_stream(index);
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

  std::shared_ptr<Event> make_event_from_msg(const Aseba::Message *msg,
                                             unsigned target) {
    const auto node = get_node(msg->source, std::set<unsigned>{target});
    const auto user_msg = dynamic_cast<const Aseba::UserMessage *>(msg);
    if (node && user_msg) {
      const auto name = node->get_event_name(msg->type);
      if (name.size()) {
        return std::make_shared<Event>(user_msg->source, name, user_msg->data);
      }
    }
    return nullptr;
  }

  void add_event_callback(const EventCallback &cb) {
    if (cb) {
      MessageCallback mcb = [cb,
                             this](const std::shared_ptr<Aseba::Message> &msg,
                                   unsigned target) {
        auto e = make_event_from_msg(msg.get(), target);
        if (e) {
          cb(e);
        }
      };
      message_callbacks.push_back(mcb);
    }
  }

  static std::string complete_target(const std::string &partial,
                                     const pybind11::kwargs &kwargs) {
    std::string target(partial);
    if (kwargs) {
      pybind11::str params;
      bool first = true;
      for (const auto &[k, v] : kwargs) {
        if (!first) {
          params += pybind11::str(";");
        }
        first = false;
        params += k + pybind11::str("=") + pybind11::str(v);
      }
      if (target.find('=') != std::string::npos) {
        if (!first && target.back() != ';') {
          target += ";";
        }
      } else if (!first && target.back() != ':') {
        target += ":";
      }
      target += params.cast<std::string>();
    }
    return target;
  }

  unsigned connect_and_start_kwargs(const std::string &partial,
                                    unsigned wait_ms, int max_retries,
                                    const pybind11::kwargs &kwargs) {
    const auto target = complete_target(partial, kwargs);
    // std::cout << "target -> " << target << std::endl;
    return connect_and_start(target, wait_ms, max_retries);
  }

  unsigned connect_and_start(const std::string &target, unsigned wait_ms,
                             int max_retries) {
    bool failed = false;
    unsigned connected = 0;
    max_retries = std::max(max_retries, 0);
    while (max_retries >= 0) {
      connected = std::get<1>(try_to_connect(target));
      if (connected) {
        break;
      }
      max_retries--;
      failed = true;
      std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
    }
    if (connected && stopped) {
      if (failed) {
        // HACK: else coppelia-sim aseba not connected if started after python
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      }
      start();
    }
    return connected;
  }

  void run_ping() {
    while (ping_ms > 0) {
      // std::cout << "ping " << ping_ms << std::endl;
      ping_network();
      std::this_thread::sleep_for(std::chrono::milliseconds(ping_ms));
    }
  }

  std::tuple<Dashel::Stream *, unsigned>
  try_to_connect(const std::string &target) {
    Dashel::Stream *stream;
    try {
      stream = connect(target);
      add_stream(stream);
    } catch (const Dashel::DashelException &e) {
      std::cerr << e.what() << std::endl;
    }
    if (stream_indices.count(stream)) {
      // std::cout << (stream == nullptr) << std::endl;
      return {stream, stream_indices.at(stream)};
    }
    return {nullptr, 0};
  }

  unsigned try_to_connect_(const std::string &target) {
    return std::get<1>(try_to_connect(target));
  }

  void clear_in_msgs() { in_msgs.clear(); }

  void stop_and_close() {
    stop();
    close_all();
  }

  template <typename T, typename... Ts>
  void send_message_of_type(Ts... args, const std::set<unsigned> &include = {},
                            const std::set<unsigned> &exclude = {}) {
    send_message(std::make_shared<T>(args...), include, exclude);
  }

  void send_message(const std::shared_ptr<Aseba::Message> &message,
                    const std::set<unsigned> &include = {},
                    const std::set<unsigned> &exclude = {}) {
    auto streams = get_streams(include, exclude);
    if (streams.size()) {
      out_msgs.put({message, streams});
    }
  }

  void send_user_message(unsigned type, const Aseba::VariablesDataVector &data,
                         const std::set<unsigned> &include = {},
                         const std::set<unsigned> &exclude = {}) {
    send_message_of_type<Aseba::UserMessage, uint16_t,
                         Aseba::VariablesDataVector>(type, data, include,
                                                     exclude);
  }

  void send_event(unsigned index, const Aseba::VariablesDataVector &data,
                  const std::set<unsigned> &include = {},
                  const std::set<unsigned> &exclude = {}) {
    send_user_message(index, data, include, exclude);
  }

  void emit_event(unsigned node_id, const std::wstring &name,
                  const Aseba::VariablesDataVector &data,
                  const std::set<unsigned> &include = {},
                  const std::set<unsigned> &exclude = {}) {
    const auto indices = get_stream_indices();
    const auto node = get_node(node_id, indices);
    if (node) {
      const auto index = node->get_event_index(name);
      if (index >= 0) {
        send_event(index, data, indices);
      }
    }
  }

  void set_variable_at_index(unsigned nodeId, unsigned index,
                             const Aseba::VariablesDataVector &value,
                             const std::set<unsigned> &include = {},
                             const std::set<unsigned> &exclude = {}) {
    send_message_of_type<Aseba::SetVariables, uint16_t, uint16_t,
                         Aseba::VariablesDataVector>(nodeId, index, value,
                                                     include, exclude);
  }

  void set_variable(unsigned nodeId, const std::wstring &name,
                    const Aseba::VariablesDataVector &value,
                    const std::set<unsigned> &include = {},
                    const std::set<unsigned> &exclude = {}) {
    const auto indices = get_stream_indices(include, exclude);
    const auto node = get_node(nodeId, indices);
    if (!node)
      return;
    const auto &vs = node->variables;
    if (vs.count(name)) {
      const auto [index, size] = vs.at(name);
      set_variable_at_index(nodeId, index, value, indices);
    }
  }

  Aseba::VariablesDataVector
  get_variable_at_index(unsigned nodeId, unsigned index, unsigned size,
                        unsigned wait_ms, const VariablesCallback &cb = nullptr,
                        const std::set<unsigned> &include = {},
                        const std::set<unsigned> &exclude = {}) {
    send_message_of_type<Aseba::GetVariables, uint16_t, uint16_t, uint16_t>(
        nodeId, index, size, include, exclude);
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
    return {};
  }

  Aseba::VariablesDataVector
  get_variable(unsigned nodeId, const std::wstring &name, unsigned wait_ms,
               const VariablesCallback &cb = nullptr,
               const std::set<unsigned> &include = {},
               const std::set<unsigned> &exclude = {}) {
    const auto indices = get_stream_indices(include, exclude);
    const auto node = get_node(nodeId, indices);
    if (!node)
      return {};
    const auto &vs = node->variables;
    if (!vs.count(name))
      return {};
    const auto [index, size] = vs.at(name);
    return get_variable_at_index(nodeId, index, size, wait_ms, cb, indices);
  }

  VariablesMap get_variables(int nodeId, unsigned wait_ms,
                             const AWaitedVariables::Callback &cb = nullptr,
                             const std::set<unsigned> &include = {},
                             const std::set<unsigned> &exclude = {}) {
    const auto indices = get_stream_indices(include, exclude);
    const auto node = get_node(nodeId, indices);
    if (!node || (!wait_ms && !cb)) {
      return {};
    }
    const auto &m = node->variables;
    const auto size = node->variables_size;
    auto &awaited = awaited_variables.emplace_back(nodeId, m, wait_ms, cb);
    get_variable_at_index(nodeId, 0, size, 0, nullptr, indices);
    if (!wait_ms) {
      return {};
    }
    auto future = awaited.value->get_future();
    pybind11::gil_scoped_release release;
    const auto s = future.wait_for(std::chrono::milliseconds(wait_ms));
    if (s == std::future_status::ready) {
      return future.get();
    }
    return {};
  }

  AWaitedNodes::Value scan(int number = -1, unsigned wait_ms = 1000,
                           const AWaitedNodes::Callback &cb = nullptr) {
    if (!wait_ms && !cb) {
      return {};
    }
    auto &awaited =
        scans.emplace_back(AWaitedNodes::Value{}, std::set<unsigned>{},
                           std::set<unsigned>{}, number, wait_ms, cb);
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

  const ClientNode *
  query_description(unsigned nodeId, unsigned wait_ms = 0,
                    const TargetDescriptionCallback &cb = nullptr,
                    const std::set<unsigned> &include = {},
                    const std::set<unsigned> &exclude = {}) {
    const auto d = get_node(nodeId, include, exclude);
    if (d)
      return d;
    send_message_of_type<Aseba::GetNodeDescription, uint16_t>(nodeId, include,
                                                              exclude);
    AWaitedNode::Callback ncb;
    if (cb) {
      ncb = [cb, this](unsigned node, unsigned target) {
        const auto desc = get_node(node, {target});
        if (desc) {
          cb(desc);
        }
      };
    }
    const auto &[node, target] =
        wait_node_connection(nodeId, wait_ms, ncb, include, exclude);
    if (target) {
      return get_node(node, {target});
    }
    return nullptr;
  }

  void load_script_to_node(ClientNode &node, unsigned nodeId,
                           const std::string &code,
                           const std::vector<Aseba::NamedValue> &events = {},
                           const std::vector<Aseba::NamedValue> &constants = {},
                           const std::set<unsigned> &include = {}) {

    std::wistringstream aesl_code(widen(code.c_str()));
    unsigned allocatedVariablesCount;
    Aseba::Compiler compiler;
    Aseba::Error error;
    Aseba::BytecodeVector bytecode;
    compiler.setTargetDescription(&node);
    Aseba::CommonDefinitions common_definitions;
    common_definitions.events.assign(events.begin(), events.end());
    common_definitions.constants.assign(constants.begin(), constants.end());
    compiler.setCommonDefinitions(&common_definitions);
    bool result =
        compiler.compile(aesl_code, bytecode, allocatedVariablesCount, error);
    if (!result) {
      throw std::runtime_error("Failed to compile script: " + narrow(error.toWString()));
    }
    node.update(*(compiler.getVariablesMap()));
    node.update(events);
    node.script = code;
    MessageVector messages;
    auto bytes = std::vector<uint16_t>(bytecode.begin(), bytecode.end());
    Aseba::sendBytecode(messages, nodeId, bytes);
    for (auto &msg : messages) {
      send_message(std::move(msg), include);
    }
  }

  void load_script(unsigned nodeId, const std::string &code,
                   const std::map<std::wstring, int> &events = {},
                   const std::map<std::wstring, int> &constants = {},
                   const std::set<unsigned> &include = {},
                   const std::set<unsigned> &exclude = {}) {
    const auto indices = get_stream_indices(include, exclude);
    const auto node = get_node(nodeId, indices);
    if (node) {
      std::vector<Aseba::NamedValue> ve;
      std::vector<Aseba::NamedValue> vc;
      for (auto &[name, value] : events) {
        ve.emplace_back(name, value);
      }
      for (auto &[name, value] : constants) {
        vc.emplace_back(name, value);
      }
      load_script_to_node(*node, nodeId, code, ve, vc, indices);
    }
  }

  void description_received(unsigned nodeId, unsigned target) {}

  void node_connected(unsigned nodeId, unsigned target) {
    for (const auto &cb : node_connection_callbacks) {
      pybind11::gil_scoped_acquire acquire;
      cb(nodeId, target);
    }
    AWaitedNodes::check(nodeId, target, awaited_nodes);
  }

  void node_disconnected(unsigned nodeId, unsigned target) {
    for (const auto &cb : node_disconnection_callbacks) {
      pybind11::gil_scoped_acquire acquire;
      cb(nodeId, target);
    }
    AWaitedNode::check(nodeId, target, awaited_node_disconnections);
  }

  std::shared_ptr<Event> get_event(unsigned nodeId, const std::wstring &name,
                                   int wait_ms = 0,
                                   const EventCallback &cb = nullptr,
                                   const std::set<unsigned> &include = {},
                                   const std::set<unsigned> &exclude = {}) {
    // std::cout << "get_event " << wait_ms << std::endl;
    const auto indices = get_stream_indices(include, exclude);
    const auto node = get_node(nodeId, indices);
    if (!node || !node->has_event(name)) {
      // std::cerr << "no node or event\n";
      return nullptr;
    }
    const auto type = node->get_event_index(name);
    AWaitedMessage::Callback mcb = nullptr;
    if (cb) {
      mcb = [cb, this](const std::shared_ptr<Aseba::Message> &msg,
                       unsigned target) {
        assert(msg);
        auto e = make_event_from_msg(msg.get(), target);
        if (e) {
          cb(e);
        }
      };
    }
    const auto [msg, target] = get_message(nodeId, type, wait_ms, mcb, indices);
    if (msg) {
      return make_event_from_msg(msg.get(), target);
    }
    return nullptr;
  }

  AWaitedMessage::Value
  get_message(int nodeId = -1, int type = -1, int wait_ms = 0,
              const AWaitedMessage::Callback &cb = nullptr,
              const std::set<unsigned> &include = {},
              const std::set<unsigned> &exclude = {}) {
    if (!wait_ms && !cb) {
      return {nullptr, 0};
    }
    const auto indices = get_stream_indices(include, exclude);
    auto &awaited_msg =
        awaited_messages.emplace_back(nodeId, type, indices, wait_ms, cb);
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

  AWaitedNodes::Value wait_nodes(const std::set<unsigned> &candidates,
                                 int number, unsigned wait_ms,
                                 const AWaitedNodes::Callback &cb = nullptr,
                                 const std::set<unsigned> &include = {},
                                 const std::set<unsigned> &exclude = {}) {
    const auto indices = get_stream_indices(include, exclude);
    unsigned num = 0;
    AWaitedNodes::Value c_target_nodes;
    if (number < 0 && candidates.size()) {
      number = static_cast<int>(candidates.size());
    }
    for (auto &[t, ns] : get_node_ids(indices)) {
      for (auto k : ns) {
        if (candidates.size() == 0 || candidates.count(k)) {
          num++;
          c_target_nodes[t].insert(k);
        }
      }
    }
    if (num == number) {
      if (cb) {
        cb(c_target_nodes, true);
      }
      return c_target_nodes;
    }
    if (!wait_ms && !cb) {
      return c_target_nodes;
    }
    auto &awaited = awaited_nodes.emplace_back(c_target_nodes, candidates,
                                               indices, number, wait_ms, cb);
    if (!wait_ms) {
      return c_target_nodes;
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

  std::tuple<unsigned, unsigned>
  wait_node_connection(int nodeId = -1, unsigned wait_ms = 1000,
                       const AWaitedNode::Callback &cb = nullptr,
                       const std::set<unsigned> &include = {},
                       const std::set<unsigned> &exclude = {}) {
    std::set<unsigned> candidates;
    if (nodeId > 0) {
      candidates.insert(static_cast<unsigned>(nodeId));
    }
    AWaitedNodes::Callback ncb = nullptr;
    if (cb) {
      ncb = [cb](const std::map<unsigned, std::set<unsigned>> &value,
                 bool complete) {
        if (value.size() && complete) {
          for (const auto &[t, ns] : value) {
            for (const auto &n : ns) {
              cb(n, t);
              return;
            }
          }
        }
      };
    }
    const auto rs = wait_nodes(candidates, 1, wait_ms, ncb, include, exclude);
    for (const auto &[t, ns] : rs) {
      for (const auto &n : ns) {
        return std::make_tuple(n, t);
      }
    }
    return {0, 0};
  }

  std::tuple<unsigned, unsigned>
  wait_node_disconnection(int nodeId = -1, unsigned wait_ms = 1000,
                          const AWaitedNode::Callback &cb = nullptr,
                          const std::set<unsigned> &include = {},
                          const std::set<unsigned> &exclude = {}) {
    const auto indices = get_stream_indices(include, exclude);
    for (auto &[t, ns] : get_node_ids(indices, {}, false)) {
      for (auto k : ns) {
        if (nodeId < 0 || nodeId == k) {
          if (cb) {
            cb(k, t);
          }
          return {k, t};
        }
      }
    }
    if (!wait_ms && !cb) {
      return {0, 0};
    }
    auto &awaited_node =
        awaited_node_disconnections.emplace_back(nodeId, indices, wait_ms, cb);
    if (!wait_ms) {
      return {0, 0};
    }
    auto future = awaited_node.value->get_future();
    pybind11::gil_scoped_release release;
    const auto s = future.wait_for(std::chrono::milliseconds(wait_ms));
    if (s == std::future_status::ready) {
      return future.get();
    }
    return {0, 0};
  }

  std::tuple<unsigned, std::string>
  wait_target_connection(int index = -1, unsigned wait_ms = 1000,
                         const AWaitedTarget::Callback &cb = nullptr) {
    if (index == -1) {
      for (const auto &[s, i] : stream_indices) {
        return {i, s->getTargetName()};
      }
    } else if (is_connected_to(index)) {
      return {index, get_target_name(index)};
    }
    std::tuple<int, std::string> r{0, ""};
    if (!wait_ms && !cb) {
      return r;
    }
    std::set<unsigned> targets;
    if (index > 0) {
      targets.insert(static_cast<unsigned>(index));
    }
    auto &awaited =
        awaited_target_connections.emplace_back(targets, wait_ms, cb);
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

  std::tuple<unsigned, std::string>
  wait_target_disconnection(int index = 0, unsigned wait_ms = 1000,
                            const AWaitedTarget::Callback &cb = nullptr) {
    std::tuple<int, std::string> r{0, ""};
    if (index > 0 && !is_connected_to(index)) {
      return r;
    }
    if (!wait_ms && !cb) {
      return r;
    }
    std::set<unsigned> targets;
    if (index > 0) {
      targets.insert(static_cast<unsigned>(index));
    }
    auto &awaited =
        awaited_target_disconnections.emplace_back(targets, wait_ms, cb);
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

  void
  advertise_nodes(const std::string &title = "pyaseba",
                  const std::map<std::string, std::tuple<std::string, unsigned>>
                      &specs = {{"thymio-II", {"Thymio II", 8}}},
                  bool query_product_id = false) {
#ifdef ZEROCONF
    if (!in_stream)
      return;
    std::vector<AdvertisedNode> nodes;
    for (const auto &[t, ns] : target_nodes) {
      for (const auto &[id, node] : ns) {
        auto name = narrow(node.name);
        unsigned pid = 0;
        if (specs.count(name)) {
          std::tie(name, pid) = specs.at(name);
        } else if (query_product_id) {
          const auto value =
              get_variable(id, widen(ASEBA_PID_VAR_NAME), 1000, nullptr, {t});
          if (value.size() == 1) {
            pid = value[0];
          }
        }
        nodes.emplace_back(id, pid, name);
      }
    }
    const auto name = title + " " + std::to_string(port);
    const auto record = make_record(nodes);
    try {
      zeroconf.advertise(name, in_stream, record);
      // std::cout << "Advertised " << name << " with" << record.record()
      //           << std::endl;
    } catch (const std::runtime_error &e) {
      std::cerr << e.what() << std::endl;
    }
#else
    std::cerr << "zeroconf not supported" << std::endl;
#endif
  }

  void deadvertise_nodes(const std::string &title = "pyaseba") {
#ifdef ZEROCONF
    if (!in_stream)
      return;
    const auto name = title + " " + std::to_string(port);
    zeroconf.forget(name, in_stream);
#else
    std::cerr << "zeroconf not supported" << std::endl;
#endif
  }
};

#endif
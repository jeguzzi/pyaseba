#ifndef CLIENT_H_GUARD
#define CLIENT_H_GUARD

#include "advertise.h"
#include "aseba/common/msg/msg.h"
#include "aseba/common/productids.h"
#include "aseba/compiler/compiler.h"
#include "aseba_dashel.h"
#include "awaited.h"
#include "dashel/dashel.h"
#include "description_manager.h"
#include "queue.h"
#include "streams_manager.h"
#include "utils.h"

#ifdef ZEROCONF
#include "zeroconf/zeroconf-dashelhub.h"
#endif

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/warnings.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>

inline std::shared_ptr<Aseba::Message> copy_message(const Aseba::Message &msg) {
  std::shared_ptr<Aseba::Message> message;
  message.reset(std::move(msg.clone()));
  return message;
}

inline std::shared_ptr<Aseba::Message> move_message(Aseba::Message *msg) {
  std::shared_ptr<Aseba::Message> message;
  message.reset(msg);
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

template <typename T> struct CallbackCollection {

  using Guard = std::lock_guard<std::mutex>;

  std::vector<T> cbs;
  std::mutex mutex;

  CallbackCollection() : cbs(), mutex() {}

  template <typename... Args> void call(Args... args) {
    Guard lck(mutex);
    for (const auto &cb : cbs) {
      pybind11::gil_scoped_acquire acquire;
      cb(args...);
    }
  }

  void add(const T &cb) {
    Guard lck(mutex);
    cbs.push_back(cb);
  }

  void remove(int index) {
    Guard lck(mutex);
    if (index < 0) {
      index = std::max(0, static_cast<int>(cbs.size()) + index);
    }
    if (index < cbs.size()) {
      cbs.erase(std::begin(cbs) + index);
    }
  }

  void clear() {
    Guard lck(mutex);
    cbs.clear();
  }
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

using DeviceInfo = std::vector<uint8_t>;
using DeviceInfoCallback = std::function<void(const DeviceInfo &)>;
using DeviceNameCallback = std::function<void(const std::string &)>;

struct ThymioRFSettings {
  uint16_t network_id;
  uint16_t node_id;
  uint16_t channel;

  explicit ThymioRFSettings(uint16_t n, uint16_t i, uint16_t c)
      : network_id(n), node_id(i), channel(c) {}

  static std::optional<ThymioRFSettings> from_info(const DeviceInfo &info) {
    if (info.size() == 6) {
      const uint16_t n = info[0] + (info[1] << 8);
      const uint16_t i = info[2] + (info[3] << 8);
      const uint16_t c = info[4] + (info[5] << 8);
      return ThymioRFSettings{n, i, c};
    }
    return std::nullopt;
  }

  DeviceInfo info() const {
    return {static_cast<uint8_t>(network_id & 0xFF),
            static_cast<uint8_t>(network_id >> 8),
            static_cast<uint8_t>(node_id & 0xFF),
            static_cast<uint8_t>(node_id >> 8),
            static_cast<uint8_t>(channel & 0xFF),
            static_cast<uint8_t>(channel >> 8)};
  }
};

using ThymioRFSettingsCallback = std::function<void(const ThymioRFSettings &)>;

struct Client : public DescriptionManager<Client>, public Dashel::Hub {
  int port;
  std::string address;
  std::atomic_bool stopped;
  std::atomic_uint ping_ms;
  std::unique_ptr<std::thread> thread;
  std::unique_ptr<std::thread> ping_thread;
  Queue<InMessage> in_msgs;
  Queue<OutMessage> out_msgs;

  StreamsManager streams;

  CallbackCollection<MessageCallback> message_callbacks;
  CallbackCollection<TargetCallback> target_connection_callbacks;
  CallbackCollection<TargetCallback> target_disconnection_callbacks;
  CallbackCollection<NodeCallback> node_connection_callbacks;
  CallbackCollection<NodeCallback> node_disconnection_callbacks;

  AwaitedCollection<AwaitedTarget> awaited_target_connections;
  AwaitedCollection<AwaitedTarget> awaited_target_disconnections;
  AwaitedCollection<AwaitedNodes> scans;
  AwaitedCollection<AwaitedNodes> awaited_nodes;
  AwaitedCollection<AwaitedNode> awaited_node_disconnections;
  AwaitedCollection<AwaitedMessage> awaited_messages;
  AwaitedCollection<AwaitedVariables> awaited_variables;

#ifdef ZEROCONF
  Aseba::DashelhubZeroconf zeroconf;
#endif

  explicit Client(
      int port = -1, const std::string &address = "0.0.0.0",
      unsigned ping_period_ms = 1000, bool query = false,
      unsigned node_disconnection_timeout_ms = 3000,
      unsigned min_protocol_version = ASEBA_MIN_TARGET_PROTOCOL_VERSION,
      unsigned max_protocol_version = ASEBA_PROTOCOL_VERSION)
      : DescriptionManager<Client>(query, node_disconnection_timeout_ms,
                                   min_protocol_version, max_protocol_version),
        Dashel::Hub(true), port(port), address(address), stopped(true),
        ping_ms(ping_period_ms), thread(nullptr), ping_thread(nullptr),
        in_msgs(), out_msgs(), streams(), message_callbacks(),
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
    // Dashel::initPlugins();
    if (port > 0 && address.size()) {
      auto stream =
          connect("tcpin:port=" + std::to_string(port) + ";address=" + address);
      LOG_INFO("Listening on {}", stream->getTargetName());
      streams.set_in(stream);
      start();
    }
  }

  ~Client() { stop_and_close(); }

  // +++++++++++++++ Dashel::Hub specialization +++++++++++++++

  void incomingData(Dashel::Stream *stream) override {
    LOG_DEBUG("Incoming data from {0}", stream->getTargetName());
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
      message = receive(stream);
    } catch (const Dashel::DashelException &e) {
      LOG_ERROR("DashelException {0}", e.what());
      stream->flush();
    } catch (const std::runtime_error &e) {
      LOG_ERROR("runtime_error {0}", e.what());
      stream->flush();
    } catch (const std::exception &e) {
      LOG_ERROR("exception {0}", e.what());
      stream->flush();
    }
    if (message) {
      if (!stopped) {
        auto index = streams.get_index(stream);
        if (index) {
          in_msgs.put({move_message(message), index});
          return;
        } else {
          LOG_WARN("Unindexed stream {0}", stream->getTargetName());
        }
      }
      delete message;
    }
  }

  void connectionCreated(Dashel::Stream *stream) override {
    LOG_INFO("Created connection {0}", stream->getTargetName());
    // const auto protocol = stream->getProtocolName();
    // if (protocol.find("tcppoll") != std::string::npos) {
    //   return;
    // }
    streams.add(stream);
    const auto name = stream->getTargetName();
    const auto index = streams.get_index(stream);
    if (index) {
      target_connection_callbacks.call(index, name);
      awaited_target_connections.check(index, name);
    }
  }

  void connectionClosed(Dashel::Stream *stream, bool abnormal) override {
    LOG_INFO("Closed connection {0} ({1})", stream->getTargetName(), abnormal);
#ifdef ZEROCONF
    zeroconf.dashelConnectionClosed(stream);
#endif // ZEROCONF
    const auto name = stream->getTargetName();
    const auto index = streams.get_index(stream);
    if (index) {
      target_disconnection_callbacks.call(index, name);
      awaited_target_disconnections.check(index, name);
      disconnect(index, true);
    }
    streams.remove(stream);
  }

  // --------------- Dashel::Hub specialization ---------------

  void process_in_message(const InMessage &in_msg) {
    const auto &[msg, target_index] = in_msg;
    process_message(msg.get(), target_index);
    message_callbacks.call(msg, target_index);
    awaited_messages.check(msg, target_index);
    if (const auto v_msg =
            dynamic_cast<const Aseba::NodePresent *>(msg.get())) {
      scans.check(v_msg->source, target_index);
    }
    if (const auto v_msg = dynamic_cast<const Aseba::Variables *>(msg.get())) {
      awaited_variables.check(v_msg);
    }
  }

  void process_out_message(const OutMessage &out_msg) {
    const auto &[message, out_streams] = out_msg;
    if (!message)
      return;
    // lock();
    Aseba::Message::SerializationBuffer buffer;
    message->serializeSpecific(buffer);
    for (auto stream : out_streams) {
      if (!dataStreams.count(stream)) {
        continue;
      }
      serialize(*message, buffer, stream);
      stream->flush();
    }
    // unlock();
  }

  void run() {
    while (!stopped &&
#ifdef ZEROCONF
           zeroconf.dashelStep(1)
#else
           step(1)
#endif
    ) {
      while (const auto &r = out_msgs.get_nowait()) {
        process_out_message(*r);
      }
      // lock();
      for (auto stream : streams.get_closing()) {
        if (!dataStreams.count(stream)) {
          continue;
        }
        connectionClosed(stream, false);
        closeStream(stream);
      }
      streams.clear_closing();
      // unlock();
    }
  }

  void start() {
    if (stopped) {
      stopped = false;
      thread = std::make_unique<std::thread>(&Client::run, this);
      if (ping_ms && !ping_thread) {
        ping_thread = std::make_unique<std::thread>(&Client::run_ping, this);
      }
      in_msgs.start(
          std::bind(&Client::process_in_message, this, std::placeholders::_1));
      // out_msgs.start(
      //     std::bind(&Client::process_out_message, this,
      //     std::placeholders::_1));
    }
  }

  bool get_processing_paused() const { return in_msgs.get_paused(); }

  void set_processing_paused(bool value) { return in_msgs.set_paused(value); }

  bool get_sending_paused() const { return out_msgs.get_paused(); }

  void set_sending_paused(bool value) { return out_msgs.set_paused(value); }

  unsigned get_ping_period_ms() const { return ping_ms; }

  void set_ping_period_ms(unsigned value_ms = 1000) {
    if (value_ms == ping_ms)
      return;
    ping_ms = value_ms;
    if (value_ms && !ping_thread) {
      ping_thread = std::make_unique<std::thread>(&Client::run_ping, this);
    }
  }

  void cancel_all_waitables() {
    awaited_node_disconnections.cancel();
    awaited_node_disconnections.cancel();
    awaited_target_connections.cancel();
    awaited_target_disconnections.cancel();
    scans.cancel();
    awaited_nodes.cancel();
    awaited_node_disconnections.cancel();
    awaited_messages.cancel();
    awaited_variables.cancel();
  }

  void stop(bool hub = true) {
    pybind11::gil_scoped_release release;
    cancel_all_waitables();
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
    // TODO: lock?
    // lock();
    auto ss = dataStreams;
    for (auto stream : ss) {
      streams.remove(stream);
      closeStream(stream);
    }
    // unlock();
  }

  bool close(unsigned index, unsigned wait_ms = 1000) {
    auto stream = streams.get(index);
    if (stream) {
      streams.close(stream);
      // TODO: lock?
      if (dataStreams.size() == 1 || !streams.has_in()) {
        stop(false);
      }
      wait_target_disconnection(index, wait_ms);
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      return true;
    }
    return false;
  }

  void add_message_callback(const MessageCallback &cb) {
    message_callbacks.add(cb);
  }

  void remove_message_callback(int index) { message_callbacks.remove(index); }

  void clear_message_callbacks() { message_callbacks.clear(); }

  void add_node_connection_callback(const NodeCallback &cb) {
    node_connection_callbacks.add(cb);
  }

  void add_node_disconnection_callback(const NodeCallback &cb) {
    node_disconnection_callbacks.add(cb);
  }

  void add_target_connection_callback(const TargetCallback &cb) {
    target_connection_callbacks.add(cb);
  }

  void add_target_disconnection_callback(const TargetCallback &cb) {
    target_disconnection_callbacks.add(cb);
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
      message_callbacks.add(mcb);
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
    pybind11::gil_scoped_release release;
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
      ping_network();
      std::this_thread::sleep_for(std::chrono::milliseconds(ping_ms));
    }
  }

  std::tuple<Dashel::Stream *, unsigned>
  try_to_connect(const std::string &target) {
    Dashel::Stream *stream;
    try {
      LOG_INFO("Connecting to {0}", target);
      stream = connect(target);
      streams.add(stream);
    } catch (const Dashel::DashelException &e) {
      LOG_ERROR("Error while connecting to {0}: {1}", target, e.what());
    }
    const auto i = streams.get_index(stream);
    if (i) {
      return {stream, i};
    }
    return {nullptr, 0};
  }

  unsigned try_to_connect_(const std::string &target) {
    pybind11::gil_scoped_release release;
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
    const auto dest = streams.get_all(include, exclude);
    if (dest.size()) {
      out_msgs.put({message, dest});
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
    const auto indices = streams.get_indices(include, exclude);
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
    const auto indices = streams.get_indices(include, exclude);
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
    AwaitedMessage::Callback mcb = nullptr;
    if (cb) {
      mcb = [cb](const std::shared_ptr<Aseba::Message> &msg, unsigned target) {
        if (const auto v_msg =
                std::dynamic_pointer_cast<Aseba::Variables>(msg)) {
          cb(v_msg->variables);
        }
      };
    }
    const auto [msg, _] =
        get_message(nodeId, {ASEBA_MESSAGE_VARIABLES}, wait_ms, mcb);
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
    const auto indices = streams.get_indices(include, exclude);
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
                             const AwaitedVariables::Callback &cb = nullptr,
                             const std::set<unsigned> &include = {},
                             const std::set<unsigned> &exclude = {}) {
    const auto indices = streams.get_indices(include, exclude);
    const auto node = get_node(nodeId, indices);
    if (!node || (!wait_ms && !cb)) {
      return {};
    }
    get_variable_at_index(nodeId, 0, node->variables_size, 0, nullptr, indices);
    return awaited_variables.wait(wait_ms, cb, VariablesMap{}, nodeId,
                                  node->variables);
  }

#if USE_MOBSYA_ASEBA
  using ChangedVariables = std::vector<Aseba::ChangedVariables::area>;
  using ChangedVariablesCallback =
      std::function<void(const ChangedVariables &)>;

  ChangedVariables
  get_changed_variables(int nodeId, unsigned wait_ms,
                        const ChangedVariablesCallback &cb = nullptr,
                        const std::set<unsigned> &include = {},
                        const std::set<unsigned> &exclude = {}) {
    AwaitedMessage::Callback mcb = nullptr;
    if (cb) {
      mcb = [cb](const std::shared_ptr<Aseba::Message> &msg, unsigned target) {
        if (auto cmsg =
                std::dynamic_pointer_cast<Aseba::ChangedVariables>(msg)) {
          cb(cmsg->variables);
        }
      };
    }
    send_message_of_type<Aseba::GetChangedVariables, uint16_t>(nodeId, include,
                                                               exclude);
    const auto &[msg, _] =
        get_message(nodeId, {ASEBA_MESSAGE_CHANGED_VARIABLES}, wait_ms, mcb,
                    include, exclude);
    if (auto cmsg = std::dynamic_pointer_cast<Aseba::ChangedVariables>(msg)) {
      return cmsg->variables;
    }
    return {};
  }
#else
  using ChangedVariables = std::vector<std::tuple<unsigned, std::vector<int>>>;
  using ChangedVariablesCallback =
      std::function<void(const ChangedVariables &)>;

  ChangedVariables
  get_changed_variables(int nodeId, unsigned wait_ms,
                        const ChangedVariablesCallback &cb = nullptr,
                        const std::set<unsigned> &include = {},
                        const std::set<unsigned> &exclude = {}) {
    pybind11::warnings::warn(
        "Pyaseba was not built against Mobsya-Aseba: getting changed "
        "variables is not supported!");
    return {};
  }
#endif

  AwaitedNodes::Value scan(int number = -1, unsigned wait_ms = 1000,
                           const AwaitedNodes::Callback &cb = nullptr) {
    if (!wait_ms && !cb) {
      return {};
    }
    send_message_of_type<Aseba::ListNodes>();
    return scans.wait(wait_ms, cb, {}, std::set<unsigned>{},
                      std::set<unsigned>{}, number);
  }

  const ClientNode *
  query_description(unsigned nodeId, unsigned wait_ms = 0,
                    const TargetDescriptionCallback &cb = nullptr,
                    const std::set<unsigned> &include = {},
                    const std::set<unsigned> &exclude = {}) {
    // TODO: expose whether to query or return cached value.
    const auto d = get_node(nodeId, include, exclude);
    if (d)
      return d;
    send_message_of_type<Aseba::GetNodeDescription, uint16_t>(nodeId, include,
                                                              exclude);
    AwaitedNode::Callback ncb;
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

  std::shared_ptr<Aseba::Message>
  query_description_fragment(unsigned nodeId, int fragment,
                             unsigned wait_ms = 0,
                             const MessageCallback &cb = nullptr,
                             const std::set<unsigned> &include = {},
                             const std::set<unsigned> &exclude = {}) {
#ifdef USE_MOBSYA_ASEBA
    // MAYBE: With fragment -2 the node is going to whole description over
    // (possibly) multiple messages.
    // ... we just wait for the first. Let the callback / waitable stay alive
    // until we receive all messages?
    if (fragment < -2) {
      fragment = -1;
    }
    send_message_of_type<Aseba::GetNodeDescriptionFragment, int16_t, uint16_t>(
        fragment, nodeId, include, exclude);
    const auto [msg, _] = get_message(
        nodeId,
        {ASEBA_MESSAGE_DESCRIPTION, ASEBA_MESSAGE_NAMED_VARIABLE_DESCRIPTION,
         ASEBA_MESSAGE_LOCAL_EVENT_DESCRIPTION,
         ASEBA_MESSAGE_NATIVE_FUNCTION_DESCRIPTION},
        wait_ms, cb);
    return msg;
#else
    pybind11::warnings::warn(
        "Pyaseba was not built against Mobsya-Aseba: getting fragments of "
        "descriptions is not supported!");
    return nullptr;
#endif
  }

  DeviceInfo query_device_info(unsigned nodeId, DeviceInfoType type,
                               unsigned wait_ms = 1000,
                               const DeviceInfoCallback &cb = nullptr,
                               const std::set<unsigned> &include = {},
                               const std::set<unsigned> &exclude = {}) {
#ifdef USE_MOBSYA_ASEBA
    AwaitedMessage::Callback mcb = nullptr;
    if (cb) {
      mcb = [cb](const std::shared_ptr<Aseba::Message> &msg, unsigned) {
        if (const auto &info =
                std::dynamic_pointer_cast<Aseba::DeviceInfo>(msg)) {
          cb(info->data);
        }
      };
    }
    send_message_of_type<Aseba::GetDeviceInfo, uint16_t, DeviceInfoType>(
        nodeId, type, include, exclude);
    const auto [msg, _] =
        get_message(nodeId, {ASEBA_MESSAGE_DEVICE_INFO}, wait_ms, mcb);
    if (const auto &info = std::dynamic_pointer_cast<Aseba::DeviceInfo>(msg)) {
      return info->data;
    }
    return {};
#else
    pybind11::warnings::warn(
        "Pyaseba was not built against Mobsya-Aseba: getting device "
        "information is not supported!");
    return {};
#endif
  }

  DeviceInfo query_device_uuid(unsigned nodeId, unsigned wait_ms = 1000,
                               const DeviceInfoCallback &cb = nullptr,
                               const std::set<unsigned> &include = {},
                               const std::set<unsigned> &exclude = {}) {
    return query_device_info(nodeId, DEVICE_INFO_UUID, wait_ms, cb, include,
                             exclude);
  }

  std::string query_device_name(unsigned nodeId, unsigned wait_ms = 1000,
                                const DeviceNameCallback &cb = nullptr,
                                const std::set<unsigned> &include = {},
                                const std::set<unsigned> &exclude = {}) {
    DeviceInfoCallback mcb = nullptr;
    if (cb) {
      mcb = [cb](const DeviceInfo &info) {
        cb(std::string(info.begin(), info.end()));
      };
    }
    const auto &info = query_device_info(nodeId, DEVICE_INFO_NAME, wait_ms, mcb,
                                         include, exclude);
    return std::string(info.begin(), info.end());
  }

  std::optional<ThymioRFSettings>
  query_thymio_rf_settings(unsigned nodeId, unsigned wait_ms = 1000,
                           const ThymioRFSettingsCallback &cb = nullptr,
                           const std::set<unsigned> &include = {},
                           const std::set<unsigned> &exclude = {}) {
    DeviceInfoCallback mcb = nullptr;
    if (cb) {
      mcb = [cb](const DeviceInfo &info) {
        if (const auto s = ThymioRFSettings::from_info(info)) {
          cb(*s);
        }
      };
    }
    const auto info = query_device_info(nodeId, DEVICE_INFO_THYMIO2_RF_SETTINGS,
                                        wait_ms, mcb, include, exclude);
    return ThymioRFSettings::from_info(info);
  }

  void set_device_info(unsigned nodeId, DeviceInfoType type,
                       const DeviceInfo &data,
                       const std::set<unsigned> &include = {},
                       const std::set<unsigned> &exclude = {}) {
#ifdef USE_MOBSYA_ASEBA
    send_message_of_type<Aseba::SetDeviceInfo, uint16_t, DeviceInfoType,
                         DeviceInfo>(nodeId, type, data, include, exclude);
#else
    pybind11::warnings::warn(
        "Pyaseba was not built against Mobsya-Aseba: setting device "
        "information is not supported!");
#endif
  }

  void set_device_uuid(unsigned nodeId, const DeviceInfo &uuid,
                       const std::set<unsigned> &include = {},
                       const std::set<unsigned> &exclude = {}) {
    set_device_info(nodeId, DEVICE_INFO_UUID, uuid, include, exclude);
  }

  void set_device_name(unsigned nodeId, const std::string &name,
                       const std::set<unsigned> &include = {},
                       const std::set<unsigned> &exclude = {}) {
    DeviceInfo data;
    data.assign(name.begin(), name.end());
    set_device_info(nodeId, DEVICE_INFO_NAME, data, include, exclude);
  }

  void set_thymio_rf_settings(unsigned nodeId, const ThymioRFSettings &data,
                              const std::set<unsigned> &include = {},
                              const std::set<unsigned> &exclude = {}) {
    set_device_info(nodeId, DEVICE_INFO_THYMIO2_RF_SETTINGS, data.info(),
                    include, exclude);
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
      throw std::runtime_error("Failed to compile script: " +
                               narrow(error.toWString()));
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
    const auto indices = streams.get_indices(include, exclude);
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

  const std::string &get_script(unsigned nodeId,
                                const std::set<unsigned> &include = {},
                                const std::set<unsigned> &exclude = {}) {
    static const std::string empty = "";
    const auto node = get_node(nodeId, include, exclude);
    if (node) {
      return node->script;
    }
    return empty;
  }

  void description_received(unsigned nodeId, unsigned target) {}

  void node_connected(unsigned nodeId, unsigned target) {
    LOG_INFO("Connected node {0} on {1}", nodeId, target);
    node_connection_callbacks.call(nodeId, target);
    awaited_nodes.check(nodeId, target);
  }

  void node_disconnected(unsigned nodeId, unsigned target) {
    LOG_INFO("Disconnected node {0} on {1}", nodeId, target);
    node_disconnection_callbacks.call(nodeId, target);
    awaited_node_disconnections.check(nodeId, target);
  }

  std::shared_ptr<Event> get_event(unsigned nodeId, const std::wstring &name,
                                   int wait_ms = 0,
                                   const EventCallback &cb = nullptr,
                                   const std::set<unsigned> &include = {},
                                   const std::set<unsigned> &exclude = {}) {
    // std::cout << "get_event " << wait_ms << std::endl;
    const auto indices = streams.get_indices(include, exclude);
    const auto node = get_node(nodeId, indices);
    if (!node || !node->has_event(name)) {
      return nullptr;
    }
    const auto type = node->get_event_index(name);
    if (type < 0) {
      return nullptr;
    }
    AwaitedMessage::Callback mcb = nullptr;
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
    const auto [msg, target] = get_message(
        nodeId, {static_cast<unsigned>(type)}, wait_ms, mcb, indices);
    if (msg) {
      return make_event_from_msg(msg.get(), target);
    }
    return nullptr;
  }

  AwaitedMessage::Value
  get_message(int nodeId = -1, std::set<unsigned> types = {}, int wait_ms = 0,
              const AwaitedMessage::Callback &cb = nullptr,
              const std::set<unsigned> &include = {},
              const std::set<unsigned> &exclude = {}, bool pause = false) {
    const auto indices = streams.get_indices(include, exclude);
    if (pause) {
      in_msgs.next();
    }
    return awaited_messages.wait(wait_ms, cb, {nullptr, 0}, nodeId, types,
                                 indices);
  }

  AwaitedNodes::Value wait_nodes(const std::set<unsigned> &candidates,
                                 int number, unsigned wait_ms,
                                 const AwaitedNodes::Callback &cb = nullptr,
                                 const std::set<unsigned> &include = {},
                                 const std::set<unsigned> &exclude = {}) {
    const auto indices = streams.get_indices(include, exclude);
    unsigned num = 0;
    AwaitedNodes::Value current_nodes;
    if (number < 0 && candidates.size()) {
      number = static_cast<int>(candidates.size());
    }
    for (auto &[t, ns] : get_node_ids(indices)) {
      for (auto k : ns) {
        if (candidates.size() == 0 || candidates.count(k)) {
          num++;
          current_nodes[t].insert(k);
        }
      }
    }
    if (num == number) {
      if (cb) {
        cb(current_nodes, true);
      }
      return current_nodes;
    }
    return awaited_nodes.wait(wait_ms, cb, current_nodes, candidates, indices,
                              number);
  }

  std::tuple<unsigned, unsigned>
  wait_node_connection(int nodeId = -1, unsigned wait_ms = 1000,
                       const AwaitedNode::Callback &cb = nullptr,
                       const std::set<unsigned> &include = {},
                       const std::set<unsigned> &exclude = {}) {
    std::set<unsigned> candidates;
    if (nodeId > 0) {
      candidates.insert(static_cast<unsigned>(nodeId));
    }
    AwaitedNodes::Callback ncb = nullptr;
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
                          const AwaitedNode::Callback &cb = nullptr,
                          const std::set<unsigned> &include = {},
                          const std::set<unsigned> &exclude = {}) {
    const auto indices = streams.get_indices(include, exclude);
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
    return awaited_node_disconnections.wait(wait_ms, cb, {0, 0}, nodeId,
                                            indices);
  }

  std::tuple<unsigned, std::string>
  wait_target_connection(int index = -1, unsigned wait_ms = 1000,
                         const AwaitedTarget::Callback &cb = nullptr) {
    auto stream = streams.get(index);
    if (stream) {
      if (index < 0) {
        index = streams.get_index(stream);
      }
      return {index, stream->getTargetName()};
    }
    std::set<unsigned> targets;
    if (index > 0) {
      targets.insert(static_cast<unsigned>(index));
    }
    return awaited_target_connections.wait(wait_ms, cb, {0, ""}, targets);
  }

  std::tuple<unsigned, std::string>
  wait_target_disconnection(int index = 0, unsigned wait_ms = 1000,
                            const AwaitedTarget::Callback &cb = nullptr) {
    std::tuple<int, std::string> r;
    if (index > 0 && !streams.has_index(index)) {
      return r;
    }
    if (!wait_ms && !cb) {
      return r;
    }
    std::set<unsigned> targets;
    if (index > 0) {
      targets.insert(static_cast<unsigned>(index));
    }
    return awaited_target_disconnections.wait(wait_ms, cb, {0, ""}, targets);
  }

  std::string make_record_name(const std::string &name = "") const {
    if (name.empty()) {
      return std::string("pyaseba ") + std::to_string(port);
    }
    return name;
  }

  void deadvertise(const std::string &name = "") {
    if (!streams.has_in())
      return;
#ifdef ZEROCONF
    ::deadvertise(zeroconf, streams.get_in(), make_record_name(name));
#else
    pybind11::warnings::warn(
        "Pyaseba was built without Zeroconf support: skip de-advertise!");
#endif
  }

  void advertise(const std::string &name = "",
                 const std::vector<AdvertisedNode> nodes = {},
                 unsigned protocol_version = ASEBA_PROTOCOL_VERSION) {
#ifdef ZEROCONF
    if (!streams.has_in())
      return;
    const auto record_name = make_record_name(name);
    ::advertise(zeroconf, streams.get_in(), record_name, nodes,
                protocol_version);
#else
    pybind11::warnings::warn(
        "Pyaseba was built without Zeroconf support: skip advertise!");
#endif
  }

  std::vector<AdvertisedNode> make_record_nodes(
      const std::map<std::string, std::tuple<std::string, unsigned>> &specs =
          {{"thymio-II", {"Thymio II", 8}}},
      bool query_product_id = false) {
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
    return nodes;
  }

  void
  advertise_nodes(const std::string &name = "",
                  const std::map<std::string, std::tuple<std::string, unsigned>>
                      &specs = {{"thymio-II", {"Thymio II", 8}}},
                  bool query_product_id = false,
                  unsigned protocol_version = ASEBA_PROTOCOL_VERSION) {
    advertise(name, make_record_nodes(specs, query_product_id),
              protocol_version);
  }
};

#endif
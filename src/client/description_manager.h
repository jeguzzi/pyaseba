#ifndef DESCRIPTION_MANAGER_H_GUARD
#define DESCRIPTION_MANAGER_H_GUARD

#include "aseba/common/msg/TargetDescription.h"
#include "aseba/common/msg/msg.h"
#include "aseba/common/utils/utils.h"
#include "utils_client.h"
#include <mutex>
#include <set>
#include <string>

struct ClientNode : public Aseba::TargetDescription {

  using Events = std::map<std::wstring, unsigned>;

  ClientNode() = default;
  ClientNode(const Aseba::TargetDescription &targetDescription)
      : Aseba::TargetDescription(targetDescription),
        namedVariablesReceptionCounter(0), localEventsReceptionCounter(0),
        nativeFunctionReceptionCounter(0), connected(false), complete(false),
        variables(), variables_size(0), events() {}

  unsigned namedVariablesReceptionCounter{0};
  unsigned localEventsReceptionCounter{0};
  unsigned nativeFunctionReceptionCounter{0};
  bool connected;
  bool complete;
  Aseba::UnifiedTime lastSeen;
  Aseba::VariablesMap variables;
  unsigned variables_size;
  Events events;

  bool is_complete() const {
    return (namedVariablesReceptionCounter == namedVariables.size()) &&
           (localEventsReceptionCounter == localEvents.size()) &&
           (nativeFunctionReceptionCounter == nativeFunctions.size());
  }
  void update(const Aseba::NamedVariableDescription &msg) {
    if (namedVariablesReceptionCounter < namedVariables.size()) {
      namedVariables[namedVariablesReceptionCounter++] = msg;
      if (namedVariablesReceptionCounter == namedVariables.size()) {
        unsigned i;
        variables = getVariablesMap(i);
        variables_size = compute_variables_size(variables);
      }
      complete = is_complete();
    }
  }
  void update(const Aseba::LocalEventDescription &msg) {
    if (localEventsReceptionCounter < localEvents.size()) {
      localEvents[localEventsReceptionCounter++] = msg;
      complete = is_complete();
    }
  }
  void update(const Aseba::NativeFunctionDescription &msg) {
    if (nativeFunctionReceptionCounter < nativeFunctions.size()) {
      nativeFunctions[nativeFunctionReceptionCounter++] = msg;
      complete = is_complete();
    }
  }
  void update(const Aseba::VariablesMap &vm) {
    variables = vm;
    variables_size = compute_variables_size(vm);
  }

  void update(const std::vector<Aseba::NamedValue> &named_events) {
    unsigned i = 0;
    events.clear();
    for (const auto &e : named_events) {
      events[e.name] = i++;
    }
  }

  std::vector<std::wstring> get_event_names() const {
    std::vector<std::wstring> rs(events.size() + localEvents.size());
    size_t i = 0;
    for (const auto &[name, _] : localEvents) {
      rs[i++] = name;
    }
    for (const auto &[name, _] : events) {
      rs[i++] = name;
    }
    return rs;
  }

  std::wstring get_event_name(unsigned index) const {
    for (const auto &[name, i] : events) {
      if (i == index)
        return name;
    }
    return std::wstring();
  }

  std::vector<std::wstring> get_variable_names() const {
    std::vector<std::wstring> vs(variables.size());
    size_t i = 0;
    for (const auto &[name, _] : variables) {
      vs[i++] = name;
    }
    return vs;
  }


  std::vector<std::wstring>
  get_function_names() const {
    std::vector<std::wstring> vs(variables.size());
    size_t i = 0;
    for (const auto &[name, desc, params] : nativeFunctions) {
      vs[i++] = name;
    }
    return vs;    
  }

};

template <typename T> class DescriptionManager {

  using Nodes = std::map<unsigned, ClientNode>;
  using TargetNodes = std::map<unsigned, Nodes>;
  using Guard = std::lock_guard<std::recursive_mutex>;

protected:
  std::recursive_mutex mutex;
  TargetNodes target_nodes;
  std::map<unsigned, std::set<unsigned>> ignored_target_nodes;

  explicit DescriptionManager(
      bool query = true,
      unsigned min_protocol_version = ASEBA_MIN_TARGET_PROTOCOL_VERSION,
      unsigned max_protocol_version = ASEBA_PROTOCOL_VERSION)
      : target_nodes(), ignored_target_nodes(), query(query),
        min_protocol_version(min_protocol_version),
        max_protocol_version(max_protocol_version) {}

public:
  bool query;
  unsigned min_protocol_version;
  unsigned max_protocol_version;

  void reset(bool notify = false) {
    if (notify) {
      auto client = static_cast<T *>(this);
      for (auto &[target, ns] : target_nodes) {
        for (auto &[id, n] : ns) {
          if (n.connected) {
            n.connected = false;
            client->node_disconnected(id, target);
          }
        }
      }
    }
    Guard lock(mutex);
    target_nodes.clear();
    ignored_target_nodes.clear();
  }

  void disconnect(unsigned target, bool notify = false) {
    Guard lock(mutex);
    auto client = static_cast<T *>(this);
    if (!target_nodes.count(target))
      return;
    for (auto &[id, n] : target_nodes.at(target)) {
      if (n.connected) {
        n.connected = false;
        if (notify) {
          client->node_disconnected(id, target);
        }
      }
    }
  }

  bool has_node(unsigned id, const std::set<unsigned> &include = {},
                const std::set<unsigned> &exclude = {},
                std::optional<bool> connected = true) {
    Guard lock(mutex);
    for (const auto &[target, ns] : target_nodes) {
      if (!is_valid(target, include, exclude))
        continue;
      if (ns.count(id)) {
        const auto &node = target_nodes.at(target).at(id);
        return node.complete && (!connected || node.connected == *connected);
      }
    }
    return false;
  }

  ClientNode *get_node(unsigned id, const std::set<unsigned> &include = {},
                 const std::set<unsigned> &exclude = {},
                 std::optional<bool> connected = true) {
    Guard lock(mutex);
    ClientNode *node = nullptr;
    for (auto &[target, nodes] : target_nodes) {
      if (!is_valid(target, include, exclude))
        continue;
      if (nodes.count(id)) {
        node = &(nodes.at(id));
        if (node->complete && (!connected || node->connected == *connected)) {
          return node;
        }
      }
    }
    return nullptr;
  }

  std::map<unsigned, std::set<unsigned>>
  get_node_ids(const std::set<unsigned> &include = {},
               const std::set<unsigned> &exclude = {},
               std::optional<bool> connected = true) {
    Guard lock(mutex);
    std::map<unsigned, std::set<unsigned>> ns;
    for (const auto &[target, nodes] : target_nodes) {
      if (!is_valid(target, include, exclude))
        continue;
      for (auto &[k, node] : nodes) {
        if (node.complete && (!connected || node.connected == *connected)) {
          ns[target].insert(k);
        }
      }
    }
    return ns;
  }

  std::map<unsigned, std::map<unsigned, const ClientNode *>>
  get_nodes(const std::set<unsigned> &include = {},
            const std::set<unsigned> &exclude = {},
            std::optional<bool> connected = true) {
    Guard lock(mutex);
    std::map<unsigned, std::map<unsigned, const ClientNode *>> rs;
    for (const auto &[target, ns] : target_nodes) {
      if (!is_valid(target, include, exclude))
        continue;
      for (auto &[k, node] : ns) {
        if (node.complete && (!connected || node.connected == *connected)) {
          rs[target][k] = &node;
        }
      }
    }
    return rs;
  }

  // const Aseba::VariablesMap &
  // get_variable_map(unsigned nodeId, const std::set<unsigned> &include = {},
  //                  const std::set<unsigned> &exclude = {}) {
  //   static Aseba::VariablesMap empty_variable_map = {};
  //   const Node *node = get_node(nodeId, include, exclude);
  //   if (node) {
  //     return node->variables;
  //   }
  //   return empty_variable_map;
  // }

  void ping_network() {
    auto client = static_cast<T *>(this);
    client->template send_message_of_type<Aseba::ListNodes>();
    // check nodes that have not been seen for long, mark them as disconnected
    const Aseba::UnifiedTime now;
    const Aseba::UnifiedTime delayToDisconnect(3000);
    bool isAnyConnected(false);
    {
      Guard lock(mutex);
      for (auto &[target, nodes] : target_nodes) {
        for (auto &[id, node] : nodes) {
          // if node supports listing,
          if (node.protocolVersion >= 5 &&
              (now - node.lastSeen) > delayToDisconnect && node.connected) {
            node.connected = false;
            client->node_disconnected(id, target);
          }
          isAnyConnected = isAnyConnected || node.connected;
        }
      }
    }
    // if no node is connected, broadcast get description as well, for old
    // targets (protocol 4)
    if (!isAnyConnected) {
      client->template send_message_of_type<Aseba::GetDescription>();
    }
  }

  void process_message(const Aseba::Message *message, unsigned target_index) {
    Guard lock(mutex);
    auto client = static_cast<T *>(this);
    auto &nodes = target_nodes[target_index];
    auto &ignored_nodes = ignored_target_nodes[target_index];
    const auto id = message->source;
    if (nodes.count(id) == 0) {
      if (ignored_nodes.count(id) == 0) {
        if (dynamic_cast<const Aseba::NodePresent *>(message)) {
          client->template send_message_of_type<Aseba::GetNodeDescription,
                                                uint16_t>(id, {target_index});
        } else if (dynamic_cast<const Aseba::Disconnected *>(message)) {
          nodes.erase(id);
        }
        if (!dynamic_cast<const Aseba::Description *>(message)) {
          return;
        }
      } else {
        return;
      }
    }
    if (auto description = dynamic_cast<const Aseba::Description *>(message)) {
      if ((description->protocolVersion < min_protocol_version) ||
          (description->protocolVersion > max_protocol_version)) {
        ignored_nodes.insert(id);
        return;
      }
      nodes.emplace(id, ClientNode(*description));
      // TODO: check complete?
    }
    if (!nodes.count(id))
      return;
    if (dynamic_cast<const Aseba::Disconnected *>(message)) {
      nodes.erase(id);
      return;
    }
    auto &node = nodes.at(id);
    node.lastSeen = Aseba::UnifiedTime();
    if (node.complete) {
      if (!node.connected) {
        node.connected = true;
        client->node_connected(id, target_index);
      }
      return;
    }
    if (const auto description =
            dynamic_cast<const Aseba::NamedVariableDescription *>(message)) {
      node.update(*description);
    } else if (const auto description =
                   dynamic_cast<const Aseba::LocalEventDescription *>(
                       message)) {
      node.update(*description);
    } else if (const auto description =
                   dynamic_cast<const Aseba::NativeFunctionDescription *>(
                       message)) {
      node.update(*description);
    }
    if (node.complete) {
      node.connected = true;
      client->description_received(id, target_index);
      client->node_connected(id, target_index);
    }
  }
};

#endif

#ifndef AWAITED_H_GUARD
#define AWAITED_H_GUARD

#include "aseba/common/msg/msg.h"
#include "utils.h"
#include "utils_client.h"

#include <future>
#include <map>
#include <memory>
#include <pybind11/pybind11.h>
#include <string>
#include <thread>
#include <tuple>

template <bool P, typename T> struct AWaitedCallback {
  using type = typename std::conditional<P, std::function<void(T, bool)>,
                                         std::function<void(T)>>::type;
  static void apply(const type &cb, const T &arg, bool complete) {
    if constexpr (P) {
      cb(arg, complete);
    } else if (complete) {
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
    } else if (complete) {
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
    if (complete)
      return;
    complete = static_cast<C *>(this)->is_complete(args...);
    Value v{};
    if (complete || P) {
      v = static_cast<C *>(this)->get(args...);
    }
    if (complete && value) {
      value->set_value(v);
    }
    if (cb) {
      pybind11::gil_scoped_acquire acquire;
      AC::apply(cb, v, complete);
    }
  }

  static void purge(std::vector<C> &queue) {
    queue.erase(std::remove_if(queue.begin(), queue.end(),
                               [](const auto &a) { return a.complete; }),
                queue.end());
  }

  static void check(T... args, std::vector<C> &queue) {
    for (auto &awaited : queue) {
      awaited.update(args...);
    }
    purge(queue);
  }

  static void cancel(std::vector<C> &queue) {
    for (auto &awaited : queue) {
      if (!awaited.complete && awaited.value) {
        awaited.value->set_value({});
        awaited.complete = true;
      }
    }
    purge(queue);
  }
};

template <typename C, typename... T>
void check(T... args, std::vector<C> &queue) {
  C::check(args..., queue);
}

template <typename C> void cancel(std::vector<C> &queue) { C::cancel(queue); }

template <typename C> void purge(std::vector<C> &queue) { C::purge(queue); }

struct AWaitedNode : AWaited<AWaitedNode, false, std::tuple<uint16_t, uint16_t>,
                             uint16_t, uint16_t> {
  int target;
  std::set<unsigned> targets;
  using A = AWaited<AWaitedNode, false, std::tuple<uint16_t, uint16_t>,
                    uint16_t, uint16_t>;
  using A::Callback;
  using A::Value;

  explicit AWaitedNode(int node, const std::set<unsigned> &targets,
                       bool awaited, const Callback &cb = nullptr)
      : A(awaited, cb), target(node), targets(targets) {}

  bool is_complete(uint16_t node, uint16_t t) {
    return (targets.size() == 0 || targets.count(t)) &&
           (target < 0 || node == target);
  }

  Value get(uint16_t node, uint16_t t) { return {node, t}; }
};

struct AWaitedTarget
    : AWaited<AWaitedTarget, false, std::tuple<unsigned, std::string>, unsigned,
              std::string> {
  std::set<unsigned> targets;
  using A = AWaited<AWaitedTarget, false, std::tuple<unsigned, std::string>,
                    unsigned, std::string>;
  using A::Callback;
  using A::Value;

  AWaitedTarget(const std::set<unsigned> targets, bool awaited,
                const Callback &cb = nullptr)
      : A(awaited, cb), targets(targets) {}

  bool is_complete(unsigned index, const std::string &name) {
    return (targets.size() == 0 || targets.count(index));
  }

  Value get(unsigned index, const std::string &name) { return {index, name}; }
};

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
  std::set<unsigned> targets;

  AWaitedMessage(int source, int type, const std::set<unsigned> &targets,
                 bool awaited, const Callback &cb = nullptr)
      : A(awaited, cb), target_source(source), target_type(type),
        targets(targets) {}

  bool is_complete(const std::shared_ptr<Aseba::Message> &msg,
                   unsigned target_index) {
    return (targets.size() == 0 || targets.count(target_index) > 0) &&
           (target_source < 0 || target_source == msg->source) &&
           (target_type < 0 || target_type == msg->type);
  }

  Value get(const std::shared_ptr<Aseba::Message> &msg, unsigned target_index) {
    return {msg, target_index};
  }
};

struct AWaitedNodes
    : AWaited<AWaitedNodes, true, std::map<unsigned, std::set<unsigned>>,
              unsigned, unsigned> {

  using A = AWaited<AWaitedNodes, true, std::map<unsigned, std::set<unsigned>>,
                    unsigned, unsigned>;
  using A::Callback;
  using A::Value;

  Value nodes;
  std::set<unsigned> candidates;
  std::set<unsigned> targets;
  int number;

  AWaitedNodes(Value nodes, const std::set<unsigned> &candidates,
               const std::set<unsigned> &targets, int number, bool awaited,
               const Callback &cb = nullptr)
      : A(awaited, cb), nodes(nodes), candidates(candidates), targets(targets),
        number(number) {}

  bool is_complete(unsigned n, unsigned t) {
    if (candidates.size() && !candidates.count(n))
      return false;
    if (targets.size() && !targets.count(t))
      return false;
    nodes[t].insert(n);
    unsigned num = 0;
    for (const auto &[_, ns] : nodes) {
      num += ns.size();
    }
    return (number >= 0 && num >= number);
  }

  Value get(unsigned n, unsigned t) { return nodes; }
};

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
        vs(rs.size()), target_node(node) {}

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

#endif
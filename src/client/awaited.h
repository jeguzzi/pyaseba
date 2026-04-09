#ifndef AWAITED_H_GUARD
#define AWAITED_H_GUARD

#include "aseba/common/msg/msg.h"
#include "utils.h"

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
      pybind11::gil_scoped_acquire acquire;
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

#endif
#ifndef AWAITED_H_GUARD
#define AWAITED_H_GUARD

#include "aseba/common/msg/msg.h"
#include "utils.h"
#include "utils_client.h"

#include <chrono>
#include <condition_variable>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <pybind11/pybind11.h>
#include <string>
#include <tuple>
#include <vector>

#ifdef ENABLE_LOGGING
#include <fmt/chrono.h>
#endif

template <bool P, typename T> struct AwaitedCallback {
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

template <bool P, typename... T> struct AwaitedCallback<P, std::tuple<T...>> {
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

template <typename C, bool P, typename V, typename... T> struct Awaited {
  using Value = V;
  using AC = AwaitedCallback<P, V>;
  using Callback = typename AC::type;
  using Guard = std::lock_guard<std::mutex>;
  using Lock = std::unique_lock<std::mutex>;
  using Clock = std::chrono::steady_clock;
  using TimePoint = std::chrono::time_point<Clock>;

  std::mutex mutex;
  std::condition_variable cv;
  Callback cb;
  bool complete;
  Value value;
  std::optional<TimePoint> tp;

  explicit Awaited(unsigned wait_ms, const Callback &cb,
                   const Value &v = Value{})
      : cb(cb), complete(false), value(v), tp(std::nullopt) {
    if (wait_ms) {
      tp = Clock::now() + std::chrono::milliseconds(wait_ms);
    }
  }

  ~Awaited() { 
    cancel();
  }

  void check(T... args) {
    if (complete) {
      return;
    }
    {
      Guard lck(mutex);
      complete = static_cast<C *>(this)->update(args...);
      if (cb && (complete || P)) {
        if (tp && Clock::now() > *tp) {
          LOG_WARN("Timed out");
        } else {
          pybind11::gil_scoped_acquire acquire;
          AC::apply(cb, value, complete);
        }
      }
      if (!complete)
        return;
    }
    cv.notify_one();
  }

  void cancel() {
    if (complete)
      return;
    {
      Guard lck(mutex);
      complete = true;
      tp = std::nullopt;
    }
    cv.notify_one();
  }

  bool wait() {
    if (complete) {
      return complete;
    }
    Lock lck(mutex);
    LOG_INFO("Begin waiting");
    const bool r = cv.wait_until(lck, *tp, [this]() { return complete; });
    LOG_INFO("Waited: complete={}", r);
    complete = true;
    return r;
  }
};

template <typename C> struct AwaitedCollection {
  using Guard = std::lock_guard<std::mutex>;
  using Type = C;
  std::mutex mutex;
  std::list<C> items;

  explicit AwaitedCollection() : mutex(), items() {}

  ~AwaitedCollection() { cancel(); }

  template <typename... T> void check(T... args) {
    Guard lck(mutex);
    for (auto &item : items) {
      item.check(args...);
    }
  }

  void cancel() {
    Guard lck(mutex);
    for (auto &item : items) {
      item.cancel();
    }
    items.clear();
  }

  template <typename... T>
  typename C::Value wait(unsigned wait_ms, const typename C::Callback &cb,
                         const typename C::Value &value, T... args) {
    if (!wait_ms && !cb) {
      return value;
    }
    pybind11::gil_scoped_release release;
    mutex.lock();
    items.remove_if([](const auto &i) { return i.complete; });
    auto &item = items.emplace_back(wait_ms, cb, value, args...);
    mutex.unlock();
    if (wait_ms) {
      item.wait();
    }
    return item.value;
  }
};

struct AwaitedNode : Awaited<AwaitedNode, false, std::tuple<uint16_t, uint16_t>,
                             uint16_t, uint16_t> {
  int target;
  std::set<unsigned> targets;
  using A = Awaited<AwaitedNode, false, std::tuple<uint16_t, uint16_t>,
                    uint16_t, uint16_t>;
  using A::Callback;
  using A::Value;

  explicit AwaitedNode(unsigned wait_ms, const Callback &cb, const Value &v,
                       int node, const std::set<unsigned> &targets)
      : A(wait_ms, cb, v), target(node), targets(targets) {}

  bool update(uint16_t node, uint16_t t) {
    const bool r = (targets.size() == 0 || targets.count(t)) &&
                   (target < 0 || node == target);
    if (r) {
      value = {node, t};
    }
    return r;
  }
};

struct AwaitedTarget
    : Awaited<AwaitedTarget, false, std::tuple<unsigned, std::string>, unsigned,
              std::string> {
  using A = Awaited<AwaitedTarget, false, std::tuple<unsigned, std::string>,
                    unsigned, std::string>;
  using A::Callback;
  using A::Value;

  std::set<unsigned> targets;

  AwaitedTarget(unsigned wait_ms, const Callback &cb, const Value &v,
                const std::set<unsigned> &ts)
      : A(wait_ms, cb, v), targets(ts) {}

  bool update(unsigned index, const std::string &name) {
    const bool r = (targets.size() == 0 || targets.count(index));
    if (r) {
      value = {index, name};
    }
    return r;
  }
};

struct AwaitedMessage
    : Awaited<AwaitedMessage, false,
              std::tuple<std::shared_ptr<Aseba::Message>, unsigned>,
              const std::shared_ptr<Aseba::Message> &, unsigned> {
  using A = Awaited<AwaitedMessage, false,
                    std::tuple<std::shared_ptr<Aseba::Message>, unsigned>,
                    const std::shared_ptr<Aseba::Message> &, unsigned>;
  using A::Callback;
  using A::Value;

  int target_source;
  int target_type;
  std::set<unsigned> targets;

  AwaitedMessage(unsigned wait_ms, const Callback &cb, const Value &v,
                 int source, int type, const std::set<unsigned> &targets)
      : A(wait_ms, cb, v), target_source(source), target_type(type),
        targets(targets) {}

  bool update(const std::shared_ptr<Aseba::Message> &msg,
              unsigned target_index) {
    const bool r = (targets.size() == 0 || targets.count(target_index) > 0) &&
                   (target_source < 0 || target_source == msg->source) &&
                   (target_type < 0 || target_type == msg->type);
    if (r) {
      value = {msg, target_index};
    }
    return r;
  }
};

struct AwaitedNodes
    : Awaited<AwaitedNodes, true, std::map<unsigned, std::set<unsigned>>,
              unsigned, unsigned> {

  using A = Awaited<AwaitedNodes, true, std::map<unsigned, std::set<unsigned>>,
                    unsigned, unsigned>;
  using A::Callback;
  using A::Value;

  std::set<unsigned> candidates;
  std::set<unsigned> targets;
  int number;

  AwaitedNodes(unsigned wait_ms, const Callback &cb, const Value &v,
               const std::set<unsigned> &candidates,
               const std::set<unsigned> &targets, int number)
      : A(wait_ms, cb, v), candidates(candidates), targets(targets),
        number(number) {}

  bool update(unsigned n, unsigned t) {
    if (candidates.size() && !candidates.count(n))
      return false;
    if (targets.size() && !targets.count(t))
      return false;
    value[t].insert(n);
    unsigned num = 0;
    for (const auto &[_, ns] : value) {
      num += ns.size();
    }
    return (number >= 0 && num >= number);
  }
};

using VariablesMap = std::map<std::wstring, Aseba::VariablesDataVector>;

struct AwaitedVariables : public Awaited<AwaitedVariables, false, VariablesMap,
                                         const Aseba::Variables *> {

  using A =
      Awaited<AwaitedVariables, false, VariablesMap, const Aseba::Variables *>;
  using A::Callback;
  using A::Value;

  Aseba::VariablesMap d;
  std::vector<bool> rs;
  Aseba::VariablesDataVector vs;
  int target_node;

  AwaitedVariables(unsigned wait_ms, const Callback &cb, const Value &v,
                   int node, const Aseba::VariablesMap &d)
      : A(wait_ms, cb, v), d(d), rs(compute_variables_size(d), false),
        vs(rs.size()), target_node(node) {}

  bool update(const Aseba::Variables *msg) {
    if (target_node >= 0 && target_node != msg->source)
      return false;
    const auto start = msg->start;
    const auto &values = msg->variables;
    std::copy(values.begin(), values.end(), vs.begin() + start);
    for (size_t i = start; i < start + values.size(); i++) {
      rs[i] = true;
    }
    const bool r = (std::find(rs.begin(), rs.end(), false) == rs.end());
    if (r) {
      for (const auto &[k, v] : d) {
        const auto [index, size] = v;
        value[k] = Aseba::VariablesDataVector(vs.begin() + index,
                                              vs.begin() + index + size);
      }
    }
    return r;
  }
};

#endif
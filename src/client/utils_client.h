#ifndef UTILS_CLIENT_H_GUARD
#define UTILS_CLIENT_H_GUARD

#include "aseba/common/msg/TargetDescription.h"
#include <chrono>
#include <set>

inline bool is_valid(unsigned index, const std::set<unsigned> &include,
                     const std::set<unsigned> &exclude) {
  return (include.size() == 0 || include.count(index) > 0) &&
         exclude.count(index) == 0;
}

inline unsigned compute_variables_size(const Aseba::VariablesMap &m) {
  unsigned c = 0;
  for (const auto &[k, v] : m) {
    const auto &[_, size] = v;
    c += size;
  }
  return c;
}

inline double now_ms() {
  const auto now = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             now.time_since_epoch())
      .count();
}

#endif

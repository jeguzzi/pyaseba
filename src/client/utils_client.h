#ifndef UTILS_CLIENT_H_GUARD
#define UTILS_CLIENT_H_GUARD

#include "aseba/common/msg/TargetDescription.h"

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

#endif

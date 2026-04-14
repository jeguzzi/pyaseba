#ifndef ADVERTISE_H
#define ADVERTISE_H

#include "aseba/common/zeroconf/zeroconf.h"

// <id, pid, name>
using AdvertisedNode = std::tuple<unsigned, unsigned, std::string>;

inline Aseba::Zeroconf::TxtRecord
make_record(const std::vector<AdvertisedNode> &nodes) {
  std::vector<unsigned int> ids;
  std::vector<unsigned int> pids;
  std::string nname = "";
  unsigned protocolVersion{ASEBA_PROTOCOL_VERSION};
  for (auto const &[id, pid, n] : nodes) {
    if (nname.empty()) {
      nname = n;
    } else if (nname != n) {
      nname = "Group";
    }
    ids.push_back(id);
    pids.push_back(pid);
  }
  if (nname.empty()) {
    nname = "Empty Group";
  }
  return {protocolVersion, nname, false, ids, pids};
}

#endif
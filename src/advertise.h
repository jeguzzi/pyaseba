#ifndef ADVERTISE_H
#define ADVERTISE_H

#include "zeroconf/zeroconf.h"

// <id, pid, name>
using AdvertisedNode = std::tuple<unsigned, unsigned, std::string>;

inline Aseba::Zeroconf::TxtRecord
make_record(const std::vector<AdvertisedNode> &nodes, bool include_nodes = false, unsigned protocol_version = ASEBA_PROTOCOL_VERSION) {
  std::vector<unsigned int> ids;
  std::vector<unsigned int> pids;
  std::string nname = "";
  for (auto const &[id, pid, n] : nodes) {
    if (nname.empty()) {
      nname = n;
    } else if (nname != n) {
      nname = "Group";
    }
    if (include_nodes) {
      // std::cout << "id " << id << std::endl;
      // std::cout << "pid " << pid << std::endl;
      ids.push_back(id);
      pids.push_back(pid);      
    }
  }
  if (nname.empty()) {
    nname = "Empty Group";
  }
  return Aseba::Zeroconf::TxtRecord{protocol_version, nname, false, ids, pids};
}

#endif
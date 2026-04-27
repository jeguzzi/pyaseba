#ifndef ADVERTISE_H
#define ADVERTISE_H

#include "aseba/common/consts.h"
#include "dashel/dashel.h"
#include "utils.h"
#ifdef ZEROCONF
#include "zeroconf/zeroconf.h"
#endif

// <id, pid, name>
using AdvertisedNode = std::tuple<unsigned, unsigned, std::string>;

#ifdef ZEROCONF
inline Aseba::Zeroconf::TxtRecord
make_record(const std::vector<AdvertisedNode> &nodes,
            bool include_nodes = false,
            unsigned protocol_version = ASEBA_PROTOCOL_VERSION) {
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
      ids.push_back(id);
      pids.push_back(pid);
    }
  }
  if (nname.empty()) {
    nname = "Empty Group";
  }
  return Aseba::Zeroconf::TxtRecord{protocol_version, nname, false, ids, pids};
}

inline void deadvertise(Aseba::Zeroconf &zeroconf, const Dashel::Stream *stream,
                        const std::string &name) {
  if (!stream)
    return;
  zeroconf.forget(name, stream);
  LOG_INFO("De-advertised {0}", name);
}

inline void advertise(Aseba::Zeroconf &zeroconf, const Dashel::Stream *stream,
                      const std::string &name,
                      const std::vector<AdvertisedNode> nodes = {},
                      unsigned protocol_version = ASEBA_PROTOCOL_VERSION) {
  if (!stream)
    return;
  const auto record = make_record(nodes, true, protocol_version);
  try {
    zeroconf.advertise(name, stream, record);
    LOG_INFO("Advertised {}", name);
  } catch (const std::runtime_error &e) {
    LOG_ERROR("Error while advertising {}: {}", name, e.what());
  }
}

#endif

#endif
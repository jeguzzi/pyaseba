#ifndef STREAMS_MANAGER_H_GUARD
#define STREAMS_MANAGER_H_GUARD

#include "dashel/dashel.h"
#include <map>
#include <mutex>
#include <set>
#include <string>

class StreamsManager {
  using Stream = Dashel::Stream;
  using Guard = std::lock_guard<std::mutex>;

private:
  std::mutex mutex;
  unsigned next_index;
  std::map<Stream *, unsigned> indices;
  std::map<unsigned, Stream *> streams;
  std::map<std::string, unsigned> target_indices;
  Stream *in_stream;
  std::set<Stream *> streams_to_be_closed;

public:
  explicit StreamsManager()
      : mutex(), next_index(1), indices(), streams(), target_indices(),
        in_stream(nullptr), streams_to_be_closed() {}

  void add(Stream *stream) {
    Guard lck(mutex);
    if (indices.count(stream) == 0) {
      unsigned index;
      const auto name = stream->getTargetName();
      if (target_indices.count(name)) {
        index = target_indices.at(name);
      } else {
        index = next_index;
        next_index++;
        target_indices.emplace(name, index);
      }
      indices.emplace(stream, index);
      streams.emplace(index, stream);
    }
  }

  void remove(Stream *stream) {
    Guard lck(mutex);
    if (indices.count(stream)) {
      streams.erase(indices.at(stream));
      indices.erase(stream);
    }
  }

  std::set<Stream *> get_all(const std::set<unsigned> &include = {},
                             const std::set<unsigned> &exclude = {}) {
    Guard lck(mutex);
    std::set<Stream *> rs;
    for (auto [stream, i] : indices) {
      if ((!include.size() || include.count(i)) && !exclude.count(i)) {
        rs.insert(stream);
      }
    }
    return rs;
  }

  std::set<unsigned> get_indices(const std::set<unsigned> &include = {},
                                 const std::set<unsigned> &exclude = {}) {
    Guard lck(mutex);
    std::set<unsigned> rs;
    for (auto [_, i] : indices) {
      if ((!include.size() || include.count(i)) && !exclude.count(i)) {
        rs.insert(i);
      }
    }
    return rs;
  }

  Stream *get(int index) {
    Guard lck(mutex);
    if (index < 0) {
      for (auto [stream, _] : indices) {
        return stream;
      }
    } else if (streams.count(index)) {
      return streams.at(index);
    }
    return nullptr;
  }

  Stream *get_in() { 
    return in_stream; 
  }

  void set_in(Stream *value) {
    {
      Guard lck(mutex);
      in_stream = value;
    }
    add(value);
  }

  bool has_in() { 
    return in_stream; 
  }

  unsigned get_index(Stream *stream) {
    Guard lck(mutex);
    if (indices.count(stream)) {
      return indices.at(stream);
    }
    return 0;
  }

  std::string get_target_name(unsigned index) {
    Guard lck(mutex);
    const auto stream = get(index);
    if (stream) {
      return stream->getTargetName();
    }
    return "";
  }

  bool is_connected() {
    Guard lck(mutex);
    return streams.size() > 0;
  }

  bool has_index(int index) {
    Guard lck(mutex);
    if (index < 0) {
      return streams.size() > 0;
    }
    return streams.count(index) > 0;
  }

  std::map<unsigned, std::string> get_target_names() {
    std::map<unsigned, std::string> rs;
    for (const auto [stream, index] : indices) {
      rs[index] = stream->getTargetName();
    }
    return rs;
  }

  void close(Stream *stream) {
    Guard lck(mutex);
    streams_to_be_closed.insert(stream);
  }

  std::set<Stream *> get_closing() {
    Guard lck(mutex);
    return streams_to_be_closed;
  }

  void clear_closing() {
    Guard lck(mutex);
    streams_to_be_closed.clear();
  }
};

#endif // STREAMS_MANAGER_H_GUARD
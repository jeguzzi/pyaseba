#ifndef QUEUE_H_GUARD
#define QUEUE_H_GUARD

#include <mutex>
#include <optional>
#include <queue>
#include <thread>

template <typename T> class Queue {

  using Callback = std::function<void(const T &)>;

  bool stopped;
  bool paused;
  bool step;
  std::queue<T> queue;
  std::mutex mutex;
  std::condition_variable cv;
  std::unique_ptr<std::thread> thread;
  std::mutex process_mutex;
  std::condition_variable process_cv;
  size_t _max_size;

public:
  Queue(size_t max_size = 1000)
      : stopped(true), paused(false), step(false), queue(), mutex(), cv(),
        thread(nullptr), process_mutex(), process_cv(), _max_size(max_size) {}

  bool is_stopped() const { return stopped; }

  bool get_paused() const { return paused; }

  void set_paused(bool value) { paused = value; }

  void next() {
    {
      std::lock_guard<std::mutex> lck(process_mutex);
      paused = true;
      step = true;
    }
    process_cv.notify_one();
  }

  std::optional<T> get() {
    std::unique_lock<std::mutex> lck(mutex);
    if (!queue.empty()) {
      auto value = queue.front();
      queue.pop();
      return value;
    }
    cv.wait(lck, [this]() { return !queue.empty() || stopped; });
    // cv.wait(lck, []() { return true; });
    if (!queue.empty()) {
      auto value = queue.front();
      queue.pop();
      return value;
    }
    return std::nullopt;
  }

  std::optional<T> get_nowait() {
    std::unique_lock<std::mutex> lck(mutex);
    if (!queue.empty()) {
      auto value = queue.front();
      queue.pop();
      return value;
    }
    return std::nullopt;
  }

  void put(const T &item) {
    std::unique_lock<std::mutex> lck(mutex);
    if (queue.size() >= _max_size)
      return;
    queue.push(item);
    cv.notify_one();
  }

  void clear() {
    std::lock_guard<std::mutex> lck(mutex);
    std::queue<T> q;
    queue.swap(q);
  }

  void process(const Callback &cb) {
    stopped = false;
    while (!stopped) {
      const auto &r = get();
      if (r) {
        if (paused) {
          std::unique_lock<std::mutex> lck(process_mutex);
          process_cv.wait(lck, [this]() { return step; });
          step = false;
        }
        cb(*r);
      }
    }
  }

  void start(const Callback &cb) {
    if (!stopped)
      return;
    stopped = false;
    paused = false;
    thread = std::make_unique<std::thread>(&Queue<T>::process, this, cb);
  }

  void stop() {
    {
      std::lock_guard<std::mutex> lck(mutex);
      std::queue<T> q;
      queue.swap(q);
      stopped = true;
    }
    cv.notify_one();
    next();
    if (thread) {
      thread->join();
    }
    thread = nullptr;
  }
};

#endif
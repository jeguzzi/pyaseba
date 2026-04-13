#ifndef QUEUE_H_GUARD
#define QUEUE_H_GUARD

#include <mutex>
#include <optional>
#include <queue>
#include <thread>

template <typename T> class Queue {

  using Callback = std::function<void(const T &)>;

  bool stopped;
  std::queue<T> queue;
  std::mutex mutex;
  std::condition_variable cv;
  std::unique_ptr<std::thread> thread;

public:
  Queue() : stopped(true), queue(), mutex(), cv(), thread(nullptr) {}

  std::optional<T> get() {
    std::unique_lock<std::mutex> lck(mutex);
    cv.wait(lck, [this]() { return !queue.empty() || stopped; });
    // cv.wait(lck, []() { return true; });
    if (!queue.empty()) {
      auto value = queue.front();
      queue.pop();
      return value;
    }
    return std::nullopt;
  }

  void put(const T &item) {
    std::unique_lock<std::mutex> lck(mutex);
    queue.push(item);
    cv.notify_one();
  }

  void clear() {
    std::lock_guard<std::mutex> lck(mutex);
    std::queue<T> q;
    queue.swap(q);
  }

  void process(const Callback &cb) {
    while (!stopped) {
      auto r = get();
      if (r) {
        cb(*r);
      }
    }
  }

  void start(const Callback &cb) {
    stopped = false;
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
    if (thread) {
      thread->join();
    }
    thread = nullptr;
  }
};

#endif
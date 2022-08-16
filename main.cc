#include <limits.h>
#include <pthread.h>
#include <sys/types.h>

#include <cassert>
#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <thread>
#include <utility>

//#include "backward.hpp"

using namespace std;
using namespace std::chrono_literals;

////////////////////////////////////////////////////////////////////////

namespace os {

////////////////////////////////////////////////////////////////////////

namespace details {

////////////////////////////////////////////////////////////////////////

template <typename Lambda>
void* call_target(void* target) {
  Lambda* lambda = reinterpret_cast<Lambda*>(target);
  (*lambda)();
  delete lambda;
  return nullptr;
}

////////////////////////////////////////////////////////////////////////

} // namespace details

////////////////////////////////////////////////////////////////////////

class Thread {
 public:
  Thread()
    : thread_handle_{},
      joinable_(false) {}

  ~Thread() noexcept(false) {
    if (joinable_) {
      throw std::logic_error{"A thread was left not joined/not detached"};
    }
  }

  Thread(const Thread& other) = delete;

  template <typename Callable>
  Thread(Callable&& callable, const uint64_t stack_size_in_bytes = 8388608)
    : joinable_{true} {
    State_<Callable>* state =
        new State_<Callable>{std::forward<Callable>(callable)};

    using lambda_type = std::remove_reference_t<decltype(*state)>;

    pthread_attr_t attr;

    int res = pthread_attr_init(&attr);
    assert(res == 0);

    res = pthread_attr_setstacksize(&attr, stack_size_in_bytes);
    assert(res == 0);

    auto creation_result = pthread_create(
        &thread_handle_,
        &attr,
        &details::call_target<lambda_type>, // void* (*pointer) (void*)
        state);

    res = pthread_attr_destroy(&attr);
    assert(res == 0);

    if (creation_result != 0) {
      delete state;
      throw std::runtime_error{
          "Failed to create a new thread with "
          "'pthread_create(), error code = '"
          + std::to_string(creation_result)};
    }
  }

  Thread(Thread&& other)
    : thread_handle_{std::exchange(other.thread_handle_, pthread_t{})},
      joinable_{std::exchange(other.joinable_, false)} {}

  Thread& operator=(const Thread& other) = delete;

  Thread& operator=(Thread&& other) {
    if (this != &other) {
      if (joinable_) {
        throw std::logic_error{"Trying to drop not joined/not detached thread"};
      }

      thread_handle_ = std::exchange(other.thread_handle_, pthread_t{});
      joinable_ = std::exchange(other.joinable_, false);
    }
    return *this;
  }

  pthread_t native_handle() const {
    return thread_handle_;
  }

  bool is_joinable() const {
    return joinable_;
  }

  void join() {
    if (joinable_) {
      int join_result = pthread_join(thread_handle_, nullptr);
      if (join_result != 0) {
        throw std::runtime_error{"Join error: " + std::to_string(join_result)};
      }
    }
    joinable_ = false;
  }

  void detach() {
    if (!joinable_) {
      throw std::logic_error{"Trying to detach already joined/detached thread"};
    }
    joinable_ = false;
  }

 private:
  template <typename Callable_>
  struct State_ {
    Callable_ callable_;

    State_(Callable_&& callable)
      : callable_(std::forward<Callable_>(callable)) {}

    void operator()() {
      std::invoke(callable_);
    }
  };

  pthread_t thread_handle_;
  bool joinable_;
};

////////////////////////////////////////////////////////////////////////

} // namespace os

////////////////////////////////////////////////////////////////////////

void foo() {
  std::this_thread::sleep_for(2000ms);
}

int main(int argc, char** argv) {
  // A simple helper class that registers for you the most
  // common signals and other callbacks to segfault,
  // hardware exception, un-handled exception etc.
  // Also provides a really good stack trace on failure.
  // backward::SignalHandling sh;

  // os::Thread t{[=]() { std::this_thread::sleep_for(2000ms); return false; }};
  std::cout << PTHREAD_STACK_MIN << std::endl;
  uint64_t bytes = 8'388'608;
  uint64_t _value = 8;
  uint64_t _unit = 1024 * 1024;
  os::Thread t{foo};

  t.join();
  // using type =
  // os::details::call_target(&foo);

  return 0;
}

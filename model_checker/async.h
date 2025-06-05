#pragma once

#include "model_checker/work_queue.h"
#include <cassert>
#include <coroutine>
#include <exception>
#include <vector>

namespace model {

struct Async {
  struct promise_type {
    Async get_return_object() const noexcept { return {}; }
    std::suspend_never initial_suspend() const noexcept { return {}; }
    void return_void() const noexcept {}
    void unhandled_exception() const noexcept { std::terminate(); }
    std::suspend_never final_suspend() const noexcept { return {}; }
  };

  Async(Async &&) noexcept = delete;

private:
  // Make constructor private to flag an error in case a function
  // returns Detached without using coroutine operations (i.e.,
  // co_await or co_return) to avoid weird surprises.
  Async() noexcept = default;
};

enum class ActionResult { OK = 0, TIMEOUT = 1 };

class RunnableActionSet;

template <typename T, typename... Args>
concept is_captureless_lambda =
    requires(T f) { static_cast<Async (*)(RunnableActionSet &, Args...)>(f); };

class RunnableActionSet {
public:
  RunnableActionSet(WorkQueue &work_queue,
                    size_t max_decisions = std::numeric_limits<size_t>::max())
      : work_queue_(work_queue), max_decisions_(max_decisions) {}

  template <typename... Args>
  void add_action(is_captureless_lambda<Args...> auto action, Args &&...args) {
    assert(decision_count_ == 0);
    action(*this, std::forward<Args>(args)...);
  }

  // co_await pool.bg() suspends the current coroutine and resumes it
  // as soon as possible in a different thread.
  [[nodiscard]] auto bg() {
    struct AwaitBackground {
      RunnableActionSet &set;
      bool await_ready() const noexcept { return false; }
      void await_suspend(std::coroutine_handle<> h) const noexcept {
        set.actions_.push_back(h);
        if (set.decision_count_ != 0) {
          set.run_next_decision();
        }
      }
      void await_resume() const noexcept {}
    };
    return AwaitBackground(*this);
  }

  ActionResult run();

private:
  void run_next_decision();

  size_t decision_count_ = 0;
  size_t max_decisions_ = 0;
  WorkQueue &work_queue_;
  std::vector<std::coroutine_handle<>> actions_;
};

} // namespace model
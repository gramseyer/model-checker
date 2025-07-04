#pragma once

#include <cassert>
#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <vector>

#include "model_checker/work_queue.h"

namespace model {

struct Async {
  // NOLINTBEGIN(readability-convert-member-functions-to-static)
  // NOLINTNEXTLINE(readability-identifier-naming)
  struct promise_type {
    Async get_return_object() noexcept { return {}; }
    std::suspend_never initial_suspend() const noexcept { return {}; }
    void return_void() const noexcept {}
    void unhandled_exception() const noexcept { std::terminate(); }
    std::suspend_never final_suspend() const noexcept { return {}; }
  };
  // NOLINTEND(readability-convert-member-functions-to-static)
  Async(Async &&) noexcept = delete;

private:
  // Make constructor private to flag an error in case a function
  // returns Async without using coroutine operations (i.e.,
  // co_await or co_return) to avoid weird surprises.
  Async() noexcept = default;
};

enum class ActionResult { kOk = 0, kTimeout = 1 };

class RunnableActionSet;

template<typename T, typename... Args>
concept is_captureless_lambda =
    requires(T f) { static_cast<Async (*)(RunnableActionSet &, Args...)>(f); };

class RunnableActionSet {
public:
  RunnableActionSet(WorkQueue &work_queue,
                    size_t max_decisions = std::numeric_limits<size_t>::max())
    : max_decisions_(max_decisions), work_queue_(work_queue)
  {}

  // disable copy and move
  RunnableActionSet(const RunnableActionSet &) = delete;
  RunnableActionSet &operator=(const RunnableActionSet &) = delete;
  RunnableActionSet(RunnableActionSet &&) = delete;
  RunnableActionSet &operator=(RunnableActionSet &&) = delete;

  ~RunnableActionSet();

  template<typename... Args>
  void add_action(is_captureless_lambda<Args...> auto action, Args &&...args)
  {
    assert(decision_count_ == 0);
    action(*this, std::forward<Args>(args)...);
  }

  [[nodiscard]] auto bg()
  {
    struct AwaitBackground {
      RunnableActionSet &set;
      // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
      bool await_ready() noexcept { return false; }
      void await_suspend(std::coroutine_handle<> h) const noexcept
      {
        set.actions_.push_back(h);
        if (set.decision_count_ != 0) {
          set.run_next_decision();
        }
      }
      void await_resume() const noexcept {}
    };
    return AwaitBackground(*this);
  }

  // Does not pause the coroutine.  Just executes a choice (with a given branch
  // count) and returns the chosen option, which the caller can intepret as it
  // wishes.
  [[nodiscard]] uint8_t choice(uint8_t option_count)
  {
    return do_manual_choice(option_count);
  }

  ActionResult run();

private:
  void run_next_decision();
  uint8_t do_manual_choice(uint8_t option_count);

  size_t decision_count_ = 0;
  size_t max_decisions_ = 0;
  WorkQueue &work_queue_;
  std::vector<std::coroutine_handle<>> actions_;
};

} // namespace model

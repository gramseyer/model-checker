#include "model_checker/async.h"

namespace model {

void RunnableActionSet::run_next_decision() {
  if (actions_.empty() || decision_count_ >= max_decisions_) {
    return;
  }

  size_t idx = decision_count_++;
  size_t action_count = actions_.size();

  uint8_t next_choice = work_queue_.get_choice(idx, action_count);

  std::coroutine_handle<> action = actions_[next_choice];
  actions_.erase(actions_.begin() + next_choice);

  action.resume();
}

ActionResult RunnableActionSet::run() {
  assert(decision_count_ == 0);
  while (!actions_.empty() && decision_count_ < max_decisions_) {
    run_next_decision();
  }
  if (actions_.empty()) {
    return ActionResult::OK;
  }
  assert(decision_count_ == max_decisions_);
  return ActionResult::TIMEOUT;
}

} // namespace model
#include "model_checker/work_queue.h"

#include <cassert>
#include <cstdint>
#include <vector>

namespace model {

std::unique_ptr<WorkQueue> WorkQueue::steal_work() {
  std::lock_guard lock(mtx_);
  if (done_) {
    return nullptr;
  }

  // We steal from near the root of the tree, but the first branch point might
  // have been fully stolen and so we need to continue down to lower levels
  std::vector<uint8_t> new_committed_choices = committed_choices_;
  for (auto &[choice, next_choices] : passed_choices_) {
    if (next_choices.empty()) {
      new_committed_choices.push_back(choice);
      continue;
    }

    new_committed_choices.push_back(next_choices.back());
    next_choices.pop_back();
    return std::make_unique<WorkQueue>(new_committed_choices);
  }

  return nullptr;
}

uint8_t WorkQueue::get_choice(uint8_t height, uint8_t n_opts) {
  if (height < committed_choices_.size()) {
    return committed_choices_[height];
  }

  size_t pass_index = height - committed_choices_.size();
  if (pass_index < passed_choices_.size()) {
    return passed_choices_[pass_index].first;
  }

  std::lock_guard lock(mtx_);

  assert(pass_index == passed_choices_.size());
  std::vector<uint8_t> next_choices;
  next_choices.reserve(n_opts - 1);
  for (size_t i = 1; i < n_opts; i++) {
    next_choices.push_back(n_opts - i);
  }
  passed_choices_.emplace_back(0, next_choices);
  return 0;
}

void WorkQueue::advance_cursor() {
  std::lock_guard lock(mtx_);

  for (ssize_t i = passed_choices_.size() - 1; i >= 0; --i) {
    auto &[choice, next_choices] = passed_choices_[i];
    if (next_choices.empty()) {
      // continue to a lower layer
      passed_choices_.pop_back();
      continue;
    }

    choice = next_choices.back();
    next_choices.pop_back();
    return;
  }
  // if we get all the way to committed_choices_, we must have finished
  // the entire search tree
  done_ = true;
}

} // namespace model
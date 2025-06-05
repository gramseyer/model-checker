#include "model_checker/work_queue.h"
#include <cassert>
#include <print>

namespace model {

WorkQueue WorkQueue::steal_work() {
  // TODO(unimpl)
  throw std::runtime_error("Not implemented");
}

uint8_t WorkQueue::get_choice(uint8_t height, uint8_t n_opts) {
  if (height < committed_choices_.size()) {
    return committed_choices_[height];
  }

  size_t pass_index = height - committed_choices_.size();
  if (pass_index < passed_choices_.size()) {
    return passed_choices_[pass_index].first;
  }

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
  std::println("advancing cursor: passed_choices_ size {}",
               passed_choices_.size());
  
  for (ssize_t i = passed_choices_.size() - 1; i >= 0; --i) {
    auto &[choice, next_choices] = passed_choices_[i];
    if (next_choices.empty()) {
      // continue to a lower layer
      passed_choices_.pop_back();
      std::println("popping back a layer: current {}", i);
      continue;
    }

    choice = next_choices.back();
    next_choices.pop_back();
    return;
  }
  std::println("got to end");
  // if we get all the way to committed_choices_, we must have finished
  // the entire search tree
  done_ = true;
}

} // namespace model
#include "model_checker/work_queue.h"

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace model {

using ssize_t = std::make_signed_t<size_t>;

std::unique_ptr<WorkQueue>
WorkQueue::steal_work()
{
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

uint8_t
WorkQueue::get_choice(uint8_t height, uint8_t n_opts)
{
  assert(n_opts >= 1);
  if (height < committed_choices_.size()) {
    assert(committed_choices_[height] < n_opts);
    return committed_choices_[height];
  }

  size_t pass_index = height - committed_choices_.size();
  if (pass_index < passed_choices_.size()) {
    assert(passed_choices_[pass_index].first < n_opts);
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

void
WorkQueue::advance_cursor()
{
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

WorkQueueManager::WorkQueueManager(size_t n_work_queues,
                                   std::vector<uint8_t> initial_path)
  : work_queues_(n_work_queues)
{
  work_queues_[0].work_ = std::make_shared<WorkQueue>(initial_path);
}

void
WorkQueueManager::mark_as_stealable(QueueState &state)
{

  if (state.in_steal_queue_) {
    return;
  }

  std::lock_guard lock(mtx_);
  if (shortcircuit_done_) {
    return;
  }
  state.in_steal_queue_ = true;
  stealable_set_.push(&state);
  cv_.notify_all();
}

void
WorkQueueManager::shortcircuit_done()
{
  std::lock_guard lock(mtx_);
  stealable_set_ = {};
  shortcircuit_done_ = true;
}

WorkQueue *
WorkQueueManager::get_work_queue(size_t idx)
{
  assert(idx < work_queues_.size());

  if (work_queues_[idx].work_ != nullptr && !work_queues_[idx].work_->done()) {
    return work_queues_[idx].work_.get();
  }

  // This is kind of sketchy; in bad patterns we might wind up just spinning
  // while waiting for one queue to find new choices, but I suspect this is good
  // enough for most cases.

  std::unique_lock lock(mtx_);
  pending_steals_++;

  auto done = [this]() { return (pending_steals_ == work_queues_.size()); };

  while (true) {
    if (stealable_set_.empty()) {
      cv_.wait(lock, [&]() { return !stealable_set_.empty() || done(); });
    }
    // If everyone is waiting to steal work, then nobody has work left and we
    // must be done.
    if (done()) {
      assert(pending_steals_ == work_queues_.size());
      cv_.notify_all();
      return nullptr;
    }
    auto *steal_from = stealable_set_.front();
    assert(steal_from);
    assert(steal_from->work_);

    if (auto ptr = steal_from->work_->steal_work()) {
      // not in steal queue because it's new work
      work_queues_[idx].work_ = std::move(ptr);
      work_queues_[idx].in_steal_queue_ = false;
      assert(pending_steals_ > 0);
      pending_steals_--;
      return work_queues_[idx].work_.get();
    }
    // not in steal queue because it's popped from the queue
    steal_from->in_steal_queue_ = false;
    stealable_set_.pop();
  }
}

std::vector<uint8_t>
WorkQueue::get_current_path() const
{
  std::vector<uint8_t> path;
  path.reserve(decision_count());
  for (const auto &choice : committed_choices_) {
    path.push_back(choice);
  }
  for (auto const &[choice, _] : passed_choices_) {
    path.push_back(choice);
  }
  return path;
}

std::string
show_path(const std::vector<uint8_t> &path)
{
  // Support for format(vector) is missing on all but the latest compilers,
  // apparently.
  std::string out = "{";
  bool first = true;
  for (auto c : path) {
    if (!first) {
      out += ", ";
    }
    out += std::to_string(c);
    first = false;
  }
  out += "}";
  return out;
}

} // namespace model

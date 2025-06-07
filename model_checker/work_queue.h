#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <utility>
#include <vector>

namespace model {

// One thread's work to do on one (sub)tree of the search space.
// Part of the queue can be stolen by another thread.
// advance_cursor() iterates through paths.
// Alternate choices (increased depth on the search tree) are added to the queue
// on demand (as get_choice() is called).
class WorkQueue {
public:
  WorkQueue() = default;
  WorkQueue(std::vector<uint8_t> committed_choices)
    : committed_choices_(std::move(committed_choices))
  {}

  // disable copy and move
  WorkQueue(const WorkQueue &) = delete;
  WorkQueue &operator=(const WorkQueue &) = delete;
  WorkQueue(WorkQueue &&) = delete;
  WorkQueue &operator=(WorkQueue &&) = delete;

  // Work steal might still fail even if the work queue isn't done;
  // we might be in the middle of a computation and just haven't found a branch
  // point yet
  std::unique_ptr<WorkQueue> steal_work();

  // Should only be called by the thread that owns the work queue.
  uint8_t get_choice(uint8_t height, uint8_t n_opts);
  // call when the current choice completes
  void advance_cursor();
  bool done() const { return done_; }

  size_t decision_count() const
  {
    return committed_choices_.size() + passed_choices_.size();
  }

  std::vector<uint8_t> get_current_path() const;

private:
  // steal_work can modify passed_choices_[i].second, but not .first or
  // passed_choices_. advance_cursor can modify passed_choices_.  Hence, mtx_ is
  // held during these methods. get_choice can modify passed_choices_ _if and
  // only if_ we add a new choice, so get_choice only acquires the mutex in that
  // scenario.

  std::mutex mtx_;
  // The work queue will be done once we finish exploring the search subtree
  // that starts with this prefix.
  const std::vector<uint8_t> committed_choices_;
  // passed_choices_.first is the choice currently being explored.
  // passed_choices_.second is the remaining choices to explore, at that branch.
  // These might get stolen by another thread.
  std::vector<std::pair<uint8_t, std::vector<uint8_t>>> passed_choices_;
  bool done_ = false;
};

std::string show_path(const std::vector<uint8_t> &path);

class WorkQueueManager {
public:
  WorkQueueManager(size_t n_work_queues,
                   std::vector<uint8_t> initial_path = {});

  // Steals work if current work queue is done
  // returns nullptr if overall work is done
  WorkQueue *get_work_queue(size_t idx);
  void mark_self_as_stealable(size_t idx)
  {
    mark_as_stealable(work_queues_[idx]);
  }

  bool done() const;

  void shortcircuit_done();

private:
  struct QueueState {
    QueueState() = default;
    std::shared_ptr<WorkQueue> work_ = nullptr;
    std::atomic<bool> in_steal_queue_ = false;
  };

  void mark_as_stealable(QueueState &state);

  std::vector<QueueState> work_queues_;

  std::mutex mtx_;
  std::condition_variable cv_;
  uint32_t pending_steals_ = 0;
  std::queue<QueueState *> stealable_set_;
  bool shortcircuit_done_ = false;
};

} // namespace model

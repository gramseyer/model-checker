#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

namespace model {

class WorkQueue {
public:
  WorkQueue() = default;
  WorkQueue(std::vector<uint8_t> committed_choices)
      : mtx_(), committed_choices_(std::move(committed_choices)) {}

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

  size_t decision_count() const {
    return committed_choices_.size() + passed_choices_.size();
  }

private:
  // steal_work can modify passed_choices_[i].second, but not .first or
  // passed_choices_. advance_cursor can modify passed_choices_.  Hence, mtx_ is
  // held during these methods. get_choice can modify passed_choices_ _if and
  // only if_ we add a new choice, so get_choice only acquires the mutex in that
  // scenario.

  std::mutex mtx_;
  const std::vector<uint8_t> committed_choices_;
  std::vector<std::pair<uint8_t, std::vector<uint8_t>>> passed_choices_;
  bool done_ = false;
};

class WorkQueueManager {
public:
  WorkQueueManager(size_t n_work_queues);

  // Steals work if current work queue is done
  // returns nullptr if overall work is done
  WorkQueue *get_work_queue(size_t idx);
  void mark_self_as_stealable(size_t idx) {
    mark_as_stealable(work_queues_[idx]);
  }

  bool done() const;

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
  int32_t pending_steals_ = 0;
  std::queue<QueueState *> stealable_set_;
};

} // namespace model
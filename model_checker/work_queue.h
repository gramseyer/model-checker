#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace model {

class WorkQueue {
public:
  WorkQueue() = default;
  WorkQueue(std::vector<uint8_t> committed_choices)
      : committed_choices_(std::move(committed_choices)) {}

  // Work steal might still fail even if the work queue isn't done;
  // we might be in the middle of a computation and just haven't found a branch
  // point yet
  std::optional<WorkQueue> steal_work();

  uint8_t get_choice(uint8_t height, uint8_t n_opts);
  // call when the current choice completes
  void advance_cursor();
  bool done() const { return done_; }

private:
  std::vector<uint8_t> committed_choices_;
  std::vector<std::pair<uint8_t, std::vector<uint8_t>>> passed_choices_;
  bool done_ = false;
};

} // namespace model
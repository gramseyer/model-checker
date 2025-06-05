#pragma once

#include <cstdint>
#include <vector>

namespace model {

class WorkQueue {
public:
  WorkQueue steal_work();

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
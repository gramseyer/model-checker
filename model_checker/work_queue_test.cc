#include <gtest/gtest.h>

#include "model_checker/work_queue.h"

namespace model {

TEST(WorkQueue, Choices) {
  WorkQueue work_queue;

  EXPECT_EQ(work_queue.get_choice(0, 2), 0);
  EXPECT_EQ(work_queue.get_choice(1, 3), 0);

  work_queue.advance_cursor();
  EXPECT_EQ(work_queue.get_choice(0, 2), 0);
  EXPECT_EQ(work_queue.get_choice(1, 3), 1);

  EXPECT_EQ(work_queue.get_choice(2, 2), 0);
  work_queue.advance_cursor();
  EXPECT_EQ(work_queue.get_choice(0, 2), 0);
  EXPECT_EQ(work_queue.get_choice(1, 3), 1);
  EXPECT_EQ(work_queue.get_choice(2, 2), 1);


  work_queue.advance_cursor();
  EXPECT_EQ(work_queue.get_choice(0, 2), 0);
  EXPECT_EQ(work_queue.get_choice(1, 3), 2);

  work_queue.advance_cursor();

  EXPECT_EQ(work_queue.get_choice(0, 2), 1);
  work_queue.advance_cursor();
  
  EXPECT_TRUE(work_queue.done());
}

} // namespace model
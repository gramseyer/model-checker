#include <gtest/gtest.h>

#include "model_checker/work_queue.h"

namespace model {

TEST(WorkQueue, Choices)
{
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

TEST(WorkQueue, StealWork)
{
  WorkQueue work_queue;
  EXPECT_FALSE(work_queue.steal_work());

  // populate with some work
  EXPECT_EQ(work_queue.get_choice(0, 3), 0);
  work_queue.advance_cursor();

  EXPECT_EQ(work_queue.get_choice(0, 3), 1);
  EXPECT_EQ(work_queue.get_choice(1, 3), 0);
  EXPECT_EQ(work_queue.get_choice(2, 3), 0);

  // the original work queue gets stuck and all the work gets stolen

  {
    auto work = work_queue.steal_work();
    ASSERT_TRUE(work);
    EXPECT_EQ(work->get_choice(0, 3), 2);
    EXPECT_EQ(work->get_choice(1, 3), 0);
    EXPECT_EQ(work->get_choice(2, 3), 0);
  }

  {
    auto work = work_queue.steal_work();
    ASSERT_TRUE(work);
    EXPECT_EQ(work->get_choice(0, 3), 1);
    EXPECT_EQ(work->get_choice(1, 3), 1);
    EXPECT_EQ(work->get_choice(2, 3), 0);
  }

  {
    auto work = work_queue.steal_work();
    ASSERT_TRUE(work);
    EXPECT_EQ(work->get_choice(0, 3), 1);
    EXPECT_EQ(work->get_choice(1, 3), 2);
    EXPECT_EQ(work->get_choice(2, 3), 0);
  }

  {
    auto work = work_queue.steal_work();
    ASSERT_TRUE(work);
    EXPECT_EQ(work->get_choice(0, 3), 1);
    EXPECT_EQ(work->get_choice(1, 3), 0);
    EXPECT_EQ(work->get_choice(2, 3), 1);
  }

  {
    auto work = work_queue.steal_work();
    ASSERT_TRUE(work);
    EXPECT_EQ(work->get_choice(0, 3), 1);
    EXPECT_EQ(work->get_choice(1, 3), 0);
    EXPECT_EQ(work->get_choice(2, 3), 2);

    EXPECT_FALSE(work->done());
  }

  EXPECT_FALSE(work_queue.done());
  EXPECT_FALSE(work_queue.steal_work());

  EXPECT_EQ(work_queue.get_choice(3, 2), 0);

  work_queue.advance_cursor();
  EXPECT_EQ(work_queue.get_choice(3, 2), 1);
  work_queue.advance_cursor();

  EXPECT_TRUE(work_queue.done());
  EXPECT_FALSE(work_queue.steal_work());
}

} // namespace model

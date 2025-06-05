#include <gtest/gtest.h>

#include "model_checker/async.h"

namespace model {

TEST(Async, RunnableActionSetBasic) {
  WorkQueue work_queue;

  RunnableActionSet set(work_queue);

  set.add_action([](RunnableActionSet &set) -> Async {
    co_await set.bg();
    co_await set.bg();
  });

  set.run();

  ASSERT_EQ(work_queue.decision_count(), 2);
  EXPECT_EQ(work_queue.get_choice(0, 1), 0);
  EXPECT_EQ(work_queue.get_choice(1, 1), 0);

  work_queue.advance_cursor();

  EXPECT_TRUE(work_queue.done());
}

TEST(Async, RunnableActionSetMultipleActions) {
  WorkQueue work_queue;

  RunnableActionSet set(work_queue);

  set.add_action([](RunnableActionSet &set) -> Async {
    co_await set.bg();
    co_await set.bg();
  });

  set.add_action([](RunnableActionSet &set) -> Async {
    co_await set.bg();
    co_await set.bg();
  });

  set.run();

  ASSERT_EQ(work_queue.decision_count(), 4);
  EXPECT_EQ(work_queue.get_choice(0, 1), 0);
  EXPECT_EQ(work_queue.get_choice(1, 1), 0);
  EXPECT_EQ(work_queue.get_choice(2, 1), 0);
  EXPECT_EQ(work_queue.get_choice(3, 1), 0);

  work_queue.advance_cursor();

  EXPECT_FALSE(work_queue.done());
}

TEST(Async, RunnableActionSetFullTree) {
  WorkQueue work_queue;
  size_t loop_iters = 0;
  while (!work_queue.done()) {
    RunnableActionSet set(work_queue);

    set.add_action([](RunnableActionSet &set) -> Async {
      co_await set.bg();
      // 1-1
      co_await set.bg();
      // 1-2
    });

    set.add_action([](RunnableActionSet &set) -> Async {
      co_await set.bg();
      // 2-1
      co_await set.bg();
      // 2-2
    });

    ASSERT_EQ(set.run(), ActionResult::OK);

    loop_iters++;
    work_queue.advance_cursor();
  }
  // Manual verification shows the full tree has 6 leaves.
  //
  // 1-1 1-2 2-1 2-2
  // 1-1 2-1 1-2 2-2
  // 1-1 2-1 2-2 1-2
  // 2-1 1-1 1-2 2-2
  // 2-1 1-1 2-2 1-2
  // 2-1 2-2 1-1 1-2

  EXPECT_EQ(loop_iters, 6);
}

TEST(Async, RunnableActionSetAdditionIsCommutative) {
  WorkQueue work_queue;

  size_t loop_iters = 0;
  while (!work_queue.done()) {
    RunnableActionSet set(work_queue);
    int32_t value = 0;

    set.add_action(
        [](RunnableActionSet &set, int32_t &value) -> Async {
          co_await set.bg();
          value += 5;
          // 1-1
          co_await set.bg();
          value += 10;
          // 1-2
        },
        value);

    set.add_action(
        [](RunnableActionSet &set, int32_t &value) -> Async {
          co_await set.bg();
          value -= 3;
          // 2-1
          co_await set.bg();
          value -= 4;
          // 2-2
        },
        value);

    ASSERT_EQ(set.run(), ActionResult::OK);

    loop_iters++;
    work_queue.advance_cursor();

    ASSERT_EQ(value, 15 - 7);
  }
  EXPECT_EQ(loop_iters, 6);
}

TEST(Async, BlockProposalMimic) {
  WorkQueue work_queue;

  while (!work_queue.done()) {
    RunnableActionSet set(work_queue, 8);
    int32_t value = 10;

    set.add_action(
        [](RunnableActionSet &set, int32_t &value) -> Async {
          co_await set.bg();
          while (value < 20) {
            co_await set.bg();
          }
          value -= 20;
        },
        value);

    set.add_action(
        [](RunnableActionSet &set, int32_t &value) -> Async {
          co_await set.bg();
          value += 5;
          co_await set.bg();
          value += 12;
        },
        value);

    auto r = set.run();
    if (r == ActionResult::TIMEOUT) {
      ASSERT_TRUE(value == 10 || value == 15 || value == 27);

    } else {
      ASSERT_EQ(r, ActionResult::OK);
      ASSERT_EQ(value, 10 + 5 + 12 - 20);
    }

    work_queue.advance_cursor();
  }
}

} // namespace model
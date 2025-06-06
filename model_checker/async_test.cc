#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

#include "model_checker/async.h"
#include "model_checker/work_queue.h"

namespace model {

TEST(Async, RunnableActionSetBasic)
{
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

TEST(Async, RunnableActionSetMultipleActions)
{
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

TEST(Async, RunnableActionSetFullTree)
{
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

    ASSERT_EQ(set.run(), ActionResult::kOk);

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

TEST(Async, RunnableActionSetAdditionIsCommutative)
{
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

    ASSERT_EQ(set.run(), ActionResult::kOk);

    loop_iters++;
    work_queue.advance_cursor();

    ASSERT_EQ(value, 15 - 7);
  }
  EXPECT_EQ(loop_iters, 6);
}

TEST(Async, BlockProposalMimic)
{
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
    if (r == ActionResult::kTimeout) {
      ASSERT_TRUE(value == 10 || value == 15 || value == 27);
    }
    else {
      ASSERT_EQ(r, ActionResult::kOk);
      ASSERT_EQ(value, 10 + 5 + 12 - 20);
    }

    work_queue.advance_cursor();
  }
}

TEST(Async, ManualChoice)
{
  // Note: Once you find a bad path in your code, you can log it as per the
  // example below, and then rerun the test using just that path by modifying
  // the initial WorkQueue For example, swapping the below line to WorkQueue
  // work_queue({1, 1, 1});)` would start with just the path 1, 1, 1, and then
  // explore all choices below that prefix.
  WorkQueue work_queue;

  while (!work_queue.done()) {
    RunnableActionSet set(work_queue);

    int x = 0, y = 0, z = 0;

    set.add_action(
        [](RunnableActionSet &set, int &x, int &y, int &z) -> Async {
          co_await set.bg();
          auto choice = set.choice(2);
          if (choice) {
            x = 1;
          }
          else {
            x = 2;
          }

          co_await set.bg();
          if (set.choice(2)) {
            y = 10;
          }
          else {
            y = 20;
          }
        },
        x, y, z);

    set.add_action(
        [](RunnableActionSet &set, int &x, int &y, int &z) -> Async {
          co_await set.bg();
          z += x;
          co_await set.bg();
          z += y;
        },
        x, y, z);

    ASSERT_EQ(set.run(), ActionResult::kOk);

    auto check = [&]() -> testing::AssertionResult {
      if (z == 0 || z == 1 || z == 2 || z == 10 || z == 11 || z == 12 ||
          z == 20 || z == 21 || z == 22) {
        return testing::AssertionSuccess();
      }
      return testing::AssertionFailure()
             << "z=" << z
             << " path=" << show_path(work_queue.get_current_path());
    };

    EXPECT_TRUE(check());

    work_queue.advance_cursor();
  }
}

} // namespace model

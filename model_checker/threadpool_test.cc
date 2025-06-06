#include <gtest/gtest.h>

#include <memory>
#include <tuple>

#include "model_checker/async.h"
#include "model_checker/threadpool.h"
#include "model_checker/work_queue.h"

namespace model {

// Deliberately keeping test small to test edge cases of
// not enough work to steal and simple shutdown.
TEST(ThreadPool, Basic)
{
  ThreadPool<int, int> pool(4);
  std::shared_ptr<ExperimentBuilder<int, int>> experiment =
      std::make_shared<ExperimentBuilder<int, int>>(
          []() { return std::make_tuple(1, 2); },
          [](WorkQueue &work_queue, int &a, int &b) {
            auto actions = std::make_unique<RunnableActionSet>(work_queue);

            actions->add_action(
                [](RunnableActionSet &set, int &a, int &b) -> Async {
                  co_await set.bg();
                  a = b * 2;
                  co_await set.bg();
                  b = a * 2;
                },
                a, b);

            return actions;
          },
          [](ActionResult res, int &a, int &b) -> bool {
            if (res != ActionResult::kOk) {
              return false;
            }

            return a == 4 && b == 8;
          });

  pool.run(experiment);
}

TEST(ThreadPool, Stealing)
{
  ThreadPool<int, int> pool(4);
  std::shared_ptr<ExperimentBuilder<int, int>> experiment =
      std::make_shared<ExperimentBuilder<int, int>>(
          []() { return std::make_tuple(0, 0); },
          [](WorkQueue &work_queue, int &a, int &b) {
            auto actions = std::make_unique<RunnableActionSet>(work_queue);

            actions->add_action(
                [](RunnableActionSet &set, int &a, int &b) -> Async {
                  co_await set.bg();
                  a += 10;
                  co_await set.bg();
                  b += 5;
                },
                a, b);

            actions->add_action(
                [](RunnableActionSet &set, int &a, int &b) -> Async {
                  co_await set.bg();
                  a += 12;
                  co_await set.bg();
                  b += 7;
                },
                a, b);
            actions->add_action(
                [](RunnableActionSet &set, int &a, int &b) -> Async {
                  co_await set.bg();
                  a += 8;
                  co_await set.bg();
                  b += 3;
                },
                a, b);
            actions->add_action(
                [](RunnableActionSet &set, int &a, int &b) -> Async {
                  co_await set.bg();
                  a += 1;
                  co_await set.bg();
                  b += 1;
                },
                a, b);
            actions->add_action(
                [](RunnableActionSet &set, int &a, int &b) -> Async {
                  co_await set.bg();
                  a += 2;
                  co_await set.bg();
                  b += 2;
                },
                a, b);

            return actions;
          },
          [](ActionResult res, int &a, int &b) -> bool {
            if (res != ActionResult::kOk) {
              return false;
            }

            return a == 33 && b == 18;
          });

  pool.run(experiment);
}

TEST(ThreadPool, NoWaitPointsEdgeCase)
{
  ThreadPool<int, int> pool(4);
  std::shared_ptr<ExperimentBuilder<int, int>> experiment =
      std::make_shared<ExperimentBuilder<int, int>>(
          []() { return std::make_tuple(1, 2); },
          [](WorkQueue &work_queue, int &a, int &b) {
            auto actions = std::make_unique<RunnableActionSet>(work_queue);

            actions->add_action(
                [](RunnableActionSet &set, int &a, int &b) -> Async {
                  b = 1;
                  a = 2;
                  co_return;
                },
                a, b);

            return actions;
          },
          [](ActionResult res, int &a, int &b) -> bool {
            if (res != ActionResult::kOk) {
              return false;
            }

            return a == 2 && b == 1;
          });

  pool.run(experiment);
}

TEST(ThreadPool, NoActionsEdgeCase)
{
  ThreadPool<int, int> pool(4);
  std::shared_ptr<ExperimentBuilder<int, int>> experiment =
      std::make_shared<ExperimentBuilder<int, int>>(
          []() { return std::make_tuple(1, 2); },
          [](WorkQueue &work_queue, int & /*unused*/, int & /*unused*/) {
            return std::make_unique<RunnableActionSet>(work_queue);
          },
          [](ActionResult res, int &a, int &b) -> bool {
            if (res != ActionResult::kOk) {
              return false;
            }

            return a == 1 && b == 2;
          });

  pool.run(experiment);
}

TEST(ThreadPool, ExhaustiveSearchCheck)
{
  static bool found_solution = false;
  ThreadPool<int> pool(4);
  std::shared_ptr<ExperimentBuilder<int>> experiment =
      std::make_shared<ExperimentBuilder<int>>(
          []() { return std::make_tuple<int>(1); },
          [](WorkQueue &work_queue, int &value) {
            auto actions = std::make_unique<RunnableActionSet>(work_queue);
            actions->add_action(
                [](RunnableActionSet &set, int &value) -> Async {
                  co_await set.bg();
                  value += 1;
                  co_await set.bg();
                  value += 1;
                  co_await set.bg();
                  value += 1;
                  co_await set.bg();
                  value += 1;
                },
                value);
            actions->add_action(
                [](RunnableActionSet &set, int &value) -> Async {
                  co_await set.bg();
                  value *= 2;
                },
                value);
            actions->add_action(
                [](RunnableActionSet &set, int &value) -> Async {
                  co_await set.bg();
                  value *= 4;
                },
                value);
            actions->add_action(
                [](RunnableActionSet &set, int &value) -> Async {
                  co_await set.bg();
                  value *= 2;
                  co_await set.bg();
                  value *= 2;
                },
                value);
            return actions;
          },
          [](ActionResult res, int &value) -> bool {
            if (res != ActionResult::kOk) {
              return false;
            }

            // Start with 1.  Then alternate multiplying by 2 and adding 1, with
            // a *4 mixed in.
            if (value == 0b111011) {
              found_solution = true;
            }
            return true;
          });

  pool.run(experiment);

  ASSERT_TRUE(found_solution);
}

} // namespace model

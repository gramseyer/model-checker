#pragma once

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <latch>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <stop_token>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>
#include <version>

#include "model_checker/async.h"
#include "model_checker/work_queue.h"

namespace model {

// Experiment and ExperimentBuilder are my attempt at making it hard to mis-use
// this library with accidental captures in the lambdas.
template<typename... Args> class Experiment {
public:
  Experiment(
      std::function<std::tuple<Args...>()>& args_builder,
      std::function<std::unique_ptr<RunnableActionSet>(WorkQueue &, Args &...)>
          build,
      std::function<bool(ActionResult, Args &...)> check)
    : args_(args_builder()), build_(build), check_(check)
  {}

  std::unique_ptr<RunnableActionSet> build(WorkQueue &work_queue)
  {
    assert(state_ == ExperimentState::kInitialized);
    state_ = ExperimentState::kRunning;
    return [&]<size_t... I>(std::index_sequence<I...>) {
      return build_(work_queue, std::get<I>(args_)...);
    }(std::make_index_sequence<sizeof...(Args)>());
  }

  bool check(ActionResult res)
  {
    assert(state_ == ExperimentState::kRunning);
    state_ = ExperimentState::kChecked;
    return [&]<size_t... I>(std::index_sequence<I...>) {
      return check_(res, std::get<I>(args_)...);
    }(std::make_index_sequence<sizeof...(Args)>());
  }

  // disable copy and move
  Experiment(const Experiment &) = delete;
  Experiment &operator=(const Experiment &) = delete;
  Experiment(Experiment &&) = delete;
  Experiment &operator=(Experiment &&) = delete;

private:
  enum class ExperimentState {
    kInitialized = 0,
    kRunning = 1,
    kChecked = 2,
  };

  std::tuple<Args...> args_;
  std::function<std::unique_ptr<RunnableActionSet>(WorkQueue &, Args &...)>
      build_;
  std::function<bool(ActionResult, Args &...)> check_;
  ExperimentState state_ = ExperimentState::kInitialized;
};

// The args() function creates a new instance of experiment state.
// This function is the only one that's allowed to have stateful captures
// if it is e.g. passed in as a lambda.
template<typename... Args> class ExperimentBuilder {
public:
  ExperimentBuilder(std::function<std::tuple<Args...>()> args,
                    std::unique_ptr<RunnableActionSet> (*build)(WorkQueue &,
                                                                Args &...),
                    bool (*check)(ActionResult, Args &...))
    : build_(build), check_(check), args_(args)
  {}

  Experiment<Args...> build()
  {
    return Experiment<Args...>(args_, build_, check_);
  }

private:
  std::function<std::unique_ptr<RunnableActionSet>(WorkQueue &, Args &...)>
      build_;
  std::function<bool(ActionResult, Args &...)> check_;

  std::function<std::tuple<Args...>()> args_;
};

template<typename... Args> class ThreadPool {
public:
  ThreadPool(int n = std::thread::hardware_concurrency())
  {
    workers_.reserve(n);
    for (int i = 0; i < n; ++i) {
      workers_.emplace_back(
          [this, i](const std::stop_token &stoken) { worker_loop(stoken, i); });
    }
  }

  // disable move and copy.
  ThreadPool(ThreadPool &&p) = delete;
  ThreadPool &operator=(ThreadPool &&p) = delete;
  ThreadPool(const ThreadPool &p) = delete;
  ThreadPool &operator=(const ThreadPool &p) = delete;

  void run(std::shared_ptr<ExperimentBuilder<Args...>> experiment)
  {
    barrier_.emplace(workers_.size());
    finish_ = false;
    {
      std::scoped_lock g(mtx_);
      work_queue_manager_ = std::make_unique<WorkQueueManager>(workers_.size());
      experiment_ = experiment;

      cv_.notify_all();
    }

    barrier_->wait();
    work_queue_manager_ = nullptr;
    experiment_ = nullptr;
    finish_ = true;
    finish_.notify_all();
  }

  ~ThreadPool()
  {
    assert(work_queue_manager_ == nullptr);
    assert(experiment_ == nullptr);
  }

private:
  std::mutex mtx_;
  std::condition_variable_any cv_;
  // null if no active work
  std::unique_ptr<WorkQueueManager> work_queue_manager_;
  std::shared_ptr<ExperimentBuilder<Args...>> experiment_;
  std::promise<void> promise_;
  std::optional<std::latch> barrier_;
  std::atomic<bool> finish_ = false;

  std::vector<std::jthread> workers_;

  // worker_loop is the main loop run by each worker thread.
  void worker_loop(const std::stop_token &stoken, size_t worker_id)
  {
    while (true) {
      WorkQueueManager *work_queue_manager = nullptr;
      using experiment_type = decltype(experiment_)::element_type;
      experiment_type *experiment = nullptr;
      {
        std::unique_lock lk(mtx_);
        cv_.wait(lk, stoken, [this] { return work_queue_manager_ != nullptr; });
        if (stoken.stop_requested()) {
          return;
        }
        assert(work_queue_manager_ != nullptr);
        assert(experiment_ != nullptr);
        work_queue_manager = work_queue_manager_.get();
        experiment = experiment_.get();
      }

      while (true) {
        auto *work_queue = work_queue_manager->get_work_queue(worker_id);
        if (!work_queue) {
          barrier_->arrive_and_wait();
          // make sure that the loop doesn't run again
          finish_.wait(false);
          break;
        }

        assert(!work_queue->done());
        auto built_exp = experiment->build();
        auto action_set = built_exp.build(*work_queue);

        assert(action_set);
        auto res = action_set->run();

        auto check_res = built_exp.check(res);

        // TODO(geoff): maybe instead return a bool to top level result
        if (!check_res) {
          throw std::runtime_error("validation failed");
        }

        work_queue->advance_cursor();

        if (!work_queue->done()) {
          work_queue_manager->mark_self_as_stealable(worker_id);
        }
      }
    }
  }
};

} // namespace model

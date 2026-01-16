# Model Checker

[![C++ CI](https://github.com/gramseyer/model-checker/actions/workflows/cpp.yml/badge.svg)](https://github.com/gramseyer/model-checker/actions/workflows/cpp.yml)

A C++23 library for systematic testing of concurrent and asynchronous code.
This library exhaustively explores different execution interleavings of concurrent operations.

Each test consists of a set of coroutines operating on some shared state
(a set of logical "threads").  At programmer-specified points 
(`co_await set.bg()`), execution switches between logical "threads."
The library automatically iterates over all possible interleavings.

This library does _not_ operate on memory models beyond sequential consistency, and relies on manual annotation of synchronization points.

## Quick Start

### Basic Usage

```cpp
WorkQueue work_queue;
while (!work_queue.done()) {
    RunnableActionSet set(work_queue);
    int shared_value = 0;

    // Action 1: increment
    set.add_action([](RunnableActionSet &set, int &value) -> Async {
        co_await set.bg();  // Switch point - other actions may run
        value += 10;
        co_await set.bg();  // Another switch point
        value += 5;
    }, shared_value);

    // Action 2: decrement
    set.add_action([](RunnableActionSet &set, int &value) -> Async {
        co_await set.bg();
        value -= 3;
        co_await set.bg();
        value -= 2;
    }, shared_value);

    auto result = set.run();
    // Check invariants
    // This always equals 10 regardless of interleaving: (+10 +5 -3 -2 = 10)
    assert(shared_value == 10);

    work_queue.advance_cursor(); // Move to next interleaving
}
```

### Making Decisions with `choice()`

The `choice()` method allows you to make non-deterministic choices without pausing the coroutine. This is useful for testing different execution paths based on runtime decisions.

```cpp
set.add_action([](RunnableActionSet &set, int &value) -> Async {
    // choice() returns a value from 0 to (option_count - 1)
    uint8_t decision = set.choice(3);  // Returns 0, 1, or 2

    if (decision == 0) {
        value += 10;
    } else if (decision == 1) {
        value += 20;
    } else {
        value += 30;
    }

    co_await set.bg();  // Switch point
}, shared_value);
```

The model checker will explore all possible values returned by `choice()`, just like it explores all possible interleavings at `co_await set.bg()` points.

### Action Results

The `run()` method returns an `ActionResult` enum with the following values:

- `ActionResult::kOk` - All actions completed successfully
- `ActionResult::kTimeout` - The decision tree depth limit was reached (see Decision Limits below)

## Building

### Prerequisites

- C++23 compatible compiler

### Build Instructions

```bash
# Clone the repository
git clone <repository-url>
cd model-checker

# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
make -j$(nproc)

# Run tests
ctest
```

## Misc Features

### Decision Limits

Some workflows may iterate indefinitely.  Use a "timeout" to set a maximum depth of the decision tree to explore.

```cpp
RunnableActionSet set(work_queue, 100); // Max 100 decisions
auto result = set.run();
if (result == ActionResult::kTimeout) {
    // Handle timeout case
}
```

### Parallel Model Checking

```cpp
ThreadPool<int> pool(4); // 4 worker threads

auto experiment = std::make_shared<ExperimentBuilder<int>>(
    []() { return std::make_tuple(0); },  // Initial state builder
    [](WorkQueue &wq, int &value) {  // Action set builder
        auto actions = std::make_unique<RunnableActionSet>(wq);

        actions->add_action([](RunnableActionSet &set, int &val) -> Async {
            co_await set.bg();
            val += 10;
        }, value);

        actions->add_action([](RunnableActionSet &set, int &val) -> Async {
            co_await set.bg();
            val -= 5;
        }, value);

        return actions;
    },
    [](ActionResult result, int &value) -> bool {  // Invariant checker
        // This check runs after each interleaving
        return value == 5;  // Should always be true (0 + 10 - 5 = 5)
    }
);

pool.run(experiment);  // Returns std::nullopt if all checks pass
```

WARNING:

This API is a bit clunky (and it's possible that there's another way to set up the code that might save a bit of setup time).
It's incredibly easy to misuse lambda captures in a coroutine, and this API design is my attempt at making something thats very hard to misuse.

For example, it is CRUCIAL that the inputs to the action set lambda
are references, and not copied -- otherwise they'll get destroyed when the lambda finishes, and then your coroutines that do the actual work will be using freed memory.  The ExperimentBuilder API is designed not to  compile unless
the arguments passed to the lambda are references, and unless the lambda passed in is captureless.

ThreadPool implements a work-stealing threadpool to parallelize exploration of the search tree.  

The ExperimentBuilder takes as input an initial state, a lambda that sets up actions to run, and a lambda that performs state verification at the end.


## License

This project is licensed under the terms specified in the LICENSE file.

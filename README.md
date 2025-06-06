# Model Checker

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
        co_await set.bg();
        value += 10;
        co_await set.bg();
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
    assert(shared_value == 10); // Should always be true
    
    work_queue.advance_cursor(); // Move to next interleaving
}
```

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
    std::make_tuple(42),
    [](WorkQueue &wq, int &value) -> std::unique_ptr<RunnableActionSet> {
        // Build your action set
    },
    [](ActionResult result, int &value) -> bool {
        // Check invariants
        return value >= 0;
    }
);

pool.run(experiment);
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

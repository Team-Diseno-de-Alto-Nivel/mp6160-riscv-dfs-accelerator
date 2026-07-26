# How to add a new DFS algorithm

This guide documents the steps to add a new DFS problem to the software
pipeline (`src/program/`). Before starting, note a key design point: **there
is no `DfsAlgorithm` base class to subclass.** Each problem is a **free
function** written against the abstract seam (`Accelerator&`), and it's
**registered** as an entry (`AlgoEntry`) in `make_algorithms()`. If you're
coming from an object-oriented mindset, think of this as "function
registration," not inheritance.

For the full pipeline picture, see
[software-pipeline.md](software-pipeline.md); for the overall architecture,
see [architecture.md](architecture.md).

## Summary of the 3 steps

1. Write the algorithm function in `algorithms/<problem>/`.
2. Add its test cases in `cases/datasets.cpp`.
3. Register the entry in `make_algorithms()` (`algorithms/dfs_algorithm.cpp`)
   and in `CMakeLists.txt`.

---

## Step 1 — Write the algorithm function

Create a new folder under `algorithms/<problem>/` with a `.h` and a `.cpp`.

**`algorithms/<problem>/<problem>.h`**

```cpp
#pragma once

#include "dfs_types.h"
#include "harness/accelerator.h"

namespace dfs {

long <problem>(const Grid& grid, Accelerator& accel);

}  // namespace dfs
```

**`algorithms/<problem>/<problem>.cpp`** — the exact signature depends on what
your problem needs (some, like `word_search_ii`, take extra arguments such as
`p.words` or config flags). The body must operate **only** on:

- its input (`Grid`, plus any extra data from `Problem`),
- the `Accelerator&` seam (`is_visited`, `mark_visited`, `unmark_visited`,
  `neighbors`, `reset_visited`, `counters()`),
- a `CountedStack` for the exploration stack.

It must not touch any state or memory outside those two channels — that's
what lets the same code run unchanged in both `Mode::Off` (pure software) and
`Mode::On` (primitives offloaded to the accelerator).

Real example (`number_of_islands`, a linear flood-fill pattern):

```cpp
#include "algorithms/number_of_islands/number_of_islands.h"

#include "harness/instrumentation.h"

namespace dfs {

long number_of_islands(const Grid& grid, Accelerator& accel) {
    accel.reset_visited();
    Counters& counters = accel.counters();

    long islands = 0;
    for (int r = 0; r < grid.rows; ++r) {
        for (int c = 0; c < grid.cols; ++c) {
            const Coord src{r, c};
            if (grid.at(r, c) == 0 || accel.is_visited(src)) continue;

            ++islands;
            CountedStack stack(counters);
            stack.push(src);
            accel.mark_visited(src);
            while (!stack.empty()) {
                const Coord cur = stack.pop();
                ++counters.expanded_nodes;
                for (const Coord n : accel.neighbors(cur)) {
                    if (grid.at(n.row, n.col) != 0 && !accel.is_visited(n)) {
                        accel.mark_visited(n);
                        stack.push(n);
                    }
                }
            }
        }
    }
    return islands;
}

}  // namespace dfs
```

If your algorithm needs behavioral variants (e.g. with/without pruning,
with/without memoization — like `word_search_ii` or
`longest_increasing_path`), add a boolean parameter to the function instead of
duplicating the code; the variants are then registered as separate entries
that reuse the same `dataset_key` (see Step 3).

## Step 2 — Add cases under `cases/`

Cases are **datasets embedded in code** (no file I/O), in
`cases/datasets.cpp`. Each problem has its own `<problem>_cases()` function
that builds a list of `TestCase`.

`Problem` (defined in `cases/test_case.h`) groups the algorithm's input:

```cpp
struct Problem {
    Grid grid;
    Coord start;
    Connectivity connectivity = Connectivity::Four;
    std::vector<std::string> words;  // only used by word_search-style problems
};
```

Use the `make_grid` helper (or `make_letters` for character grids) and the
`expect(tc, expected_value)` helper to set the expected result:

```cpp
std::vector<TestCase> my_problem_cases() {
    std::vector<TestCase> cs;
    const std::vector<std::vector<int>> grid = {
        {1, 1, 0},
        {0, 1, 0},
        {0, 0, 1},
    };
    cs.push_back(expect({"my_problem_basic", "my_problem",
                         {make_grid(grid), {0, 0}, Connectivity::Four, {}}, {}}, /*expected=*/2));
    return cs;
}
```

**Design cases to force best/worst case**: vary connectivity (4/8), density,
size, and shape of the grid — the same criteria followed by the five existing
problems (see `main.pdf`, Experiment Plan section).

Finally, register the function in `CaseLoader::load()`, at the end of the
file:

```cpp
std::vector<TestCase> CaseLoader::load(const std::string& algorithm) const {
    std::vector<TestCase> all;
    if (algorithm.empty() || algorithm == "number_of_islands")
        append(all, number_of_islands_cases());
    // ...
    if (algorithm.empty() || algorithm == "my_problem")
        append(all, my_problem_cases());
    return all;
}
```

The string used here (`"my_problem"`) is the `dataset_key` that connects to
the algorithm registration in Step 3.

## Step 3 — Register the algorithm with the harness

In `algorithms/dfs_algorithm.cpp`:

1. Include your new algorithm's header.
2. Add an `AlgoEntry{name, dataset_key, run}` entry to the vector returned by
   `make_algorithms()`. `run` is a lambda that adapts your function's
   signature to `std::function<long(const Problem&, Accelerator&)>`.

```cpp
#include "algorithms/my_problem/my_problem.h"

std::vector<AlgoEntry> make_algorithms() {
    return {
        // ... existing entries ...
        {"my_problem", "my_problem",
         [](const Problem& p, Accelerator& a) { return my_problem(p.grid, a); }},
    };
}
```

If your algorithm has variants (pruning/no-pruning, memo/no-memo), add one
entry per variant reusing the same `dataset_key`, as `word_search_ii_nopruning`
does:

```cpp
{"my_problem_variant", "my_problem",
 [](const Problem& p, Accelerator& a) { return my_problem(p.grid, a, /*flag=*/false); }},
```

Finally, add the new `.cpp` to `src/program/CMakeLists.txt`:

```cmake
add_executable(program
    # ...
    algorithms/my_problem/my_problem.cpp
)
```

## Verification

When you build and run `program`, the harness will automatically pick up your
algorithm via `make_algorithms()`, load your cases via `CaseLoader`, and run
it twice per case (`Mode::Off` and `Mode::On`). The program returns a
non-zero exit code if any case fails — a clean run is your passing test suite.
The report (`report()`) will show your algorithm in the ON/OFF comparison
table alongside the others.

## Quick checklist

- [ ] `algorithms/<problem>/<problem>.{h,cpp}` — function written only
      against `Accelerator&` and `CountedStack`
- [ ] `cases/datasets.cpp` — `<problem>_cases()` function + registration in
      `CaseLoader::load()`
- [ ] `algorithms/dfs_algorithm.cpp` — include + entry in `make_algorithms()`
- [ ] `CMakeLists.txt` — new `.cpp` added to `add_executable(program ...)`
- [ ] Cases cover best/worst case (connectivity, density, size, shape)

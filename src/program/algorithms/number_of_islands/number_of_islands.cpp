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

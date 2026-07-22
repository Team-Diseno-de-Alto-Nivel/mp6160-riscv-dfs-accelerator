#include "algorithms/unique_paths_iii/unique_paths_iii.h"

#include <functional>

#include "harness/instrumentation.h"

namespace dfs {

long unique_paths_iii(const Grid& grid, Accelerator& accel) {
    accel.reset_visited();
    Counters& counters = accel.counters();

    int walkable = 0;
    Coord start{0, 0};
    for (int r = 0; r < grid.rows; ++r) {
        for (int c = 0; c < grid.cols; ++c) {
            if (grid.at(r, c) != -1) ++walkable;
            if (grid.at(r, c) == 1) start = {r, c};
        }
    }

    long paths = 0;
    std::function<void(Coord, int, std::size_t)> dfs = [&](Coord cur, int walked,
                                                           std::size_t depth) {
        ++counters.expanded_nodes;
        if (depth > counters.peak_stack_depth) counters.peak_stack_depth = depth;
        if (grid.at(cur.row, cur.col) == 2) {
            if (walked == walkable) ++paths;
            return;
        }
        for (const Coord n : accel.neighbors(cur)) {
            if (grid.at(n.row, n.col) == -1 || accel.is_visited(n)) continue;
            accel.mark_visited(n);
            dfs(n, walked + 1, depth + 1);
            accel.unmark_visited(n);
        }
    };

    accel.mark_visited(start);
    dfs(start, 1, 1);
    return paths;
}

}  // namespace dfs

#include "algorithms/longest_increasing_path/longest_increasing_path.h"

#include <algorithm>
#include <functional>
#include <vector>

#include "harness/instrumentation.h"

namespace dfs {

long longest_increasing_path(const Grid& matrix, Accelerator& accel, bool use_memo) {
    Counters& counters = accel.counters();
    std::vector<int> memo(static_cast<std::size_t>(matrix.rows) * matrix.cols, 0);

    std::function<int(Coord, std::size_t)> dfs = [&](Coord cur, std::size_t depth) -> int {
        ++counters.expanded_nodes;
        if (depth > counters.peak_stack_depth) counters.peak_stack_depth = depth;

        int& cached = memo[matrix.index(cur.row, cur.col)];
        if (use_memo && cached != 0) return cached;

        int best = 1;
        for (const Coord n : accel.neighbors(cur)) {
            if (matrix.at(n.row, n.col) > matrix.at(cur.row, cur.col))
                best = std::max(best, 1 + dfs(n, depth + 1));
        }
        if (use_memo) cached = best;
        return best;
    };

    long longest = 0;
    for (int r = 0; r < matrix.rows; ++r)
        for (int c = 0; c < matrix.cols; ++c)
            longest = std::max(longest, static_cast<long>(dfs({r, c}, 1)));

    return longest;
}

}  // namespace dfs

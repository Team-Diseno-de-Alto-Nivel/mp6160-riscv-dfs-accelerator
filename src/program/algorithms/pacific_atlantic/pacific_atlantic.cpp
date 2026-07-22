#include "algorithms/pacific_atlantic/pacific_atlantic.h"

#include <vector>

#include "harness/instrumentation.h"

namespace dfs {

long pacific_atlantic(const Grid& heights, Accelerator& accel) {
    Counters& counters = accel.counters();

    auto flood = [&](const std::vector<Coord>& sources) {
        accel.reset_visited();
        CountedStack stack(counters);
        for (const Coord s : sources) {
            if (!accel.is_visited(s)) {
                accel.mark_visited(s);
                stack.push(s);
            }
        }
        while (!stack.empty()) {
            const Coord cur = stack.pop();
            ++counters.expanded_nodes;
            for (const Coord n : accel.neighbors(cur)) {
                if (!accel.is_visited(n) &&
                    heights.at(n.row, n.col) >= heights.at(cur.row, cur.col)) {
                    accel.mark_visited(n);
                    stack.push(n);
                }
            }
        }
    };

    std::vector<Coord> pacific, atlantic;
    for (int r = 0; r < heights.rows; ++r) {
        for (int c = 0; c < heights.cols; ++c) {
            if (r == 0 || c == 0) pacific.push_back({r, c});
            if (r == heights.rows - 1 || c == heights.cols - 1) atlantic.push_back({r, c});
        }
    }

    flood(pacific);
    std::vector<char> in_pacific(heights.cells.size());
    for (int r = 0; r < heights.rows; ++r)
        for (int c = 0; c < heights.cols; ++c)
            in_pacific[heights.index(r, c)] = accel.is_visited({r, c}) ? 1 : 0;

    flood(atlantic);
    long count = 0;
    for (int r = 0; r < heights.rows; ++r)
        for (int c = 0; c < heights.cols; ++c)
            if (in_pacific[heights.index(r, c)] && accel.is_visited({r, c})) ++count;

    return count;
}

}  // namespace dfs

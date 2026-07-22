#pragma once
// Common value types shared by the RISC-V software baseline.

#include <cstdint>
#include <string>
#include <vector>

namespace dfs {

struct Coord {
    int row = 0;
    int col = 0;
};

// Row-major 2D grid; cell meaning is algorithm-specific.
struct Grid {
    int rows = 0;
    int cols = 0;
    std::vector<int> cells;

    int at(int r, int c) const;  // TODO(SW-2)
};

struct AlgoResult {
    long value = 0;
    bool valid = false;
};

}  // namespace dfs

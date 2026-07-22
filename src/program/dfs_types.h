#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dfs {

struct Coord {
    int row = 0;
    int col = 0;
};

inline bool operator==(Coord a, Coord b) { return a.row == b.row && a.col == b.col; }

enum class Connectivity { Four = 4, Eight = 8 };

struct Grid {
    int rows = 0;
    int cols = 0;
    std::vector<int> cells;

    bool in_bounds(int r, int c) const { return r >= 0 && r < rows && c >= 0 && c < cols; }
    int index(int r, int c) const { return r * cols + c; }
    int at(int r, int c) const { return cells[index(r, c)]; }
};

struct AlgoResult {
    long value = 0;
    bool valid = false;
};

}  // namespace dfs

#pragma once
// Common value types shared across the SystemC model.
//
// Named CellCoord (not Coord) deliberately: src/program/dfs_types.h defines
// its own dfs::Coord ({row, col}, not {x, y}) for the software side, and
// nothing in src/model included src/program headers until the INT-1
// integration bridge needed to reach into both — at that point a same-name,
// different-layout dfs::Coord in each half would collide (ODR violation) the
// instant one file includes both.

#include <cstdint>

namespace dfs {

struct CellCoord {
    std::uint32_t x = 0;
    std::uint32_t y = 0;
};

using CellValue = std::int32_t;

// Interpretation is app-specific: path length, region size, node count, ...
struct ResultData {
    std::uint32_t value = 0;
    std::uint32_t visited_cells = 0;
    std::uint32_t peak_stack_depth = 0;
    bool valid = false;
};

}  // namespace dfs

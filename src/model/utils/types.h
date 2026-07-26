#pragma once
// Common value types shared across the SystemC model.
//
// Named CellCoord, not Coord: src/program/dfs_types.h has its own dfs::Coord
// ({row, col}) for the software side. Same name, different layout, same
// namespace — including both headers in one file would be an ODR violation.

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

#pragma once
// Common value types shared across the SystemC model.

#include <cstdint>

namespace dfs {

struct Coord {
    std::uint32_t x = 0;
    std::uint32_t y = 0;
};

using CellValue = std::int32_t;

enum class Connectivity : std::uint8_t {
    Four = 4,   // (x-1,y), (x+1,y), (x,y-1), (x,y+1)
    Eight = 8,  // Four + diagonals
};

struct TraversalParams {
    std::uint32_t rows = 0;
    std::uint32_t cols = 0;
    Coord start;
    Connectivity connectivity = Connectivity::Four;
};

// Interpretation is app-specific: path length, region size, node count, ...
struct ResultData {
    std::uint32_t value = 0;
    std::uint32_t visited_cells = 0;
    std::uint32_t peak_stack_depth = 0;
    bool valid = false;
};

}  // namespace dfs

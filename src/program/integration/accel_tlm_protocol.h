#pragma once

#include "dfs_types.h"

namespace dfs::integration {

enum class PrimOp {
    LoadGrid,
    ResetVisited,
    IsVisited,
    MarkVisited,
    UnmarkVisited,
    Neighbors,
};

struct PrimMessage {
    PrimOp op = PrimOp::IsVisited;
    const Grid* grid = nullptr;
    Connectivity connectivity = Connectivity::Four;
    Coord cell{};

    bool visited_out = false;
    bool newly_marked_out = false;
    int neighbor_count_out = 0;
    Coord neighbors_out[8]{};
};

}  // namespace dfs::integration

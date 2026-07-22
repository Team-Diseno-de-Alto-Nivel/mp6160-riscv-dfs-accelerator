#pragma once
// Pacific Atlantic (SW-3): edge-anchored multi-source DFS, two intersecting sets.

#include "algorithms/dfs_algorithm.h"

namespace dfs {

class PacificAtlantic : public DfsAlgorithm {
  public:
    std::string name() const override { return "pacific_atlantic"; }
    AlgoResult solve(const TestCase& tc, Accelerator& accel) override;  // TODO(SW-3)
};

}  // namespace dfs

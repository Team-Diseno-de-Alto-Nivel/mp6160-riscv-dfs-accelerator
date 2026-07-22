#pragma once
// Number of Islands (SW-3): flood fill; vary 4/8 connectivity and island density.

#include "algorithms/dfs_algorithm.h"

namespace dfs {

class NumberOfIslands : public DfsAlgorithm {
  public:
    std::string name() const override { return "number_of_islands"; }
    AlgoResult solve(const TestCase& tc, Accelerator& accel) override;  // TODO(SW-3)
};

}  // namespace dfs

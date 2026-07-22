#pragma once
// Unique Paths III (SW-3): mark/unmark backtracking; vary obstacle density and size.

#include "algorithms/dfs_algorithm.h"

namespace dfs {

class UniquePathsIII : public DfsAlgorithm {
  public:
    std::string name() const override { return "unique_paths_iii"; }
    AlgoResult solve(const TestCase& tc, Accelerator& accel) override;  // TODO(SW-3)
};

}  // namespace dfs

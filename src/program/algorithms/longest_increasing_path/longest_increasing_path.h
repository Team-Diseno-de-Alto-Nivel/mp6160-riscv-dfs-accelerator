#pragma once
// Longest Increasing Path (SW-3): multi-source DFS; run with/without memoization.

#include "algorithms/dfs_algorithm.h"

namespace dfs {

class LongestIncreasingPath : public DfsAlgorithm {
  public:
    std::string name() const override { return "longest_increasing_path"; }
    AlgoResult solve(const TestCase& tc, Accelerator& accel) override;  // TODO(SW-3)
};

}  // namespace dfs

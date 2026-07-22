#pragma once
// Word Search II (SW-3): trie-guided backtracking; vary dictionary, toggle pruning.

#include "algorithms/dfs_algorithm.h"

namespace dfs {

class WordSearchII : public DfsAlgorithm {
  public:
    std::string name() const override { return "word_search_ii"; }
    AlgoResult solve(const TestCase& tc, Accelerator& accel) override;  // TODO(SW-3)
};

}  // namespace dfs

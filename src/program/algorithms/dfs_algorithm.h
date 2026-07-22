#pragma once
// DFS algorithm interface (SW-3): the contract the five LeetCode problems
// implement so the harness runs them uniformly with the accelerator ON/OFF.

#include <memory>
#include <string>
#include <vector>

#include "dfs_types.h"
#include "harness/accelerator.h"

namespace dfs {

struct TestCase;

class DfsAlgorithm {
  public:
    virtual ~DfsAlgorithm() = default;

    virtual std::string name() const = 0;
    virtual AlgoResult solve(const TestCase& tc, Accelerator& accel) = 0;  // TODO(SW-3)
};

std::vector<std::unique_ptr<DfsAlgorithm>> make_all_algorithms();  // TODO(SW-3)

}  // namespace dfs

#include "harness/harness.h"

#include "algorithms/dfs_algorithm.h"

namespace dfs {

std::vector<RunMetrics> Harness::run_all() {
    // TODO(SW-1): for each algorithm and case, run OFF then ON, collect metrics.
    return {};
}

RunMetrics Harness::run_one(DfsAlgorithm& algo, const TestCase& tc, bool accelerator_on) {
    // TODO(SW-1): build Accelerator in the requested mode, time solve(), validate.
    (void)algo;
    (void)tc;
    (void)accelerator_on;
    return {};
}

}  // namespace dfs

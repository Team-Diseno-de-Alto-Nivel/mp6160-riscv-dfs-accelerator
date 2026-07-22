#pragma once
// Harness (SW-1): runs each case against each algorithm twice (accelerator
// ON/OFF), validates, and records metrics.

#include <vector>

#include "cases/test_case.h"
#include "harness/metrics.h"

namespace dfs {

class DfsAlgorithm;

class Harness {
  public:
    std::vector<RunMetrics> run_all();  // TODO(SW-1)

  private:
    RunMetrics run_one(DfsAlgorithm& algo, const TestCase& tc, bool accelerator_on);  // TODO(SW-1)
};

}  // namespace dfs

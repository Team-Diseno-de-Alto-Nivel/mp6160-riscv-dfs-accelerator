#pragma once

#include <vector>

#include "cases/test_case.h"
#include "harness/metrics.h"

namespace dfs {

struct AlgoEntry;

class Harness {
  public:
    std::vector<RunMetrics> run_all();

  private:
    RunMetrics run_one(const AlgoEntry& algo, const TestCase& tc, bool accelerator_on);
};

}  // namespace dfs

#pragma once

#include <string>
#include <vector>

#include "cases/test_case.h"
#include "harness/metrics.h"

namespace dfs {

struct AlgoEntry;

const char* tier_name(Tier tier);
bool parse_tier(const std::string& text, Tier& out);

class Harness {
  public:
    explicit Harness(Tier max_tier = Tier::Legacy) : max_tier_(max_tier) {}

    std::vector<RunMetrics> run_all();

  private:
    RunMetrics run_one(const AlgoEntry& algo, const TestCase& tc, Tier tier,
                       bool accelerator_on);

    Tier max_tier_;
};

}  // namespace dfs

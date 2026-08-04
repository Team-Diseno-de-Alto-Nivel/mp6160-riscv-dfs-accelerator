#pragma once

#include <string>
#include <vector>

#include "cases/grid_spec.h"
#include "dfs_types.h"

namespace dfs {

struct Problem {
    Grid grid;
    Coord start;
    Connectivity connectivity = Connectivity::Four;
    std::vector<std::string> words;
};

struct TestCase {
    std::string name;
    std::string algorithm;
    Problem input;
    AlgoResult expected;
};

struct CaseSpec {
    std::string name;
    std::string algorithm;
    Tier tier = Tier::Legacy;
    Connectivity connectivity = Connectivity::Four;
    Coord start;
    std::vector<std::string> words;
    AlgoResult expected;
    bool generated = false;
    GridSpec spec;
    Grid grid;
};

TestCase materialize(const CaseSpec& spec);

std::vector<CaseSpec> synthetic_specs(const std::string& algorithm, Tier max_tier);

class CaseLoader {
  public:
    std::vector<TestCase> load(const std::string& algorithm = "") const;
    std::vector<CaseSpec> specs(const std::string& algorithm = "",
                                Tier max_tier = Tier::Legacy) const;
};

}  // namespace dfs

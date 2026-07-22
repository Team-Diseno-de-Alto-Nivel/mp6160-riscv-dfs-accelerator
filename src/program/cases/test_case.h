#pragma once

#include <string>
#include <vector>

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

class CaseLoader {
  public:
    std::vector<TestCase> load(const std::string& algorithm = "") const;
};

}  // namespace dfs

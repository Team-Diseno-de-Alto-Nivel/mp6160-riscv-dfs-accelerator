#pragma once
// Test-case format and loader (SW-2): grid + start + params + expected result.

#include <string>
#include <vector>

#include "dfs_types.h"

namespace dfs {

struct TestCase {
    std::string name;
    std::string algorithm;
    Grid grid;
    Coord start;
    std::vector<std::string> params;  // algorithm-specific (connectivity, dict, ...)
    AlgoResult expected;
};

class CaseLoader {
  public:
    // Load cases for one algorithm, or all if empty.
    std::vector<TestCase> load(const std::string& algorithm = "") const;  // TODO(SW-2)
};

}  // namespace dfs

#pragma once

#include <functional>
#include <string>
#include <vector>

#include "cases/test_case.h"
#include "harness/accelerator.h"

namespace dfs {

struct AlgoEntry {
    std::string name;
    std::string dataset_key;
    Tier max_tier;
    std::function<long(const Problem&, Accelerator&)> run;
};

std::vector<AlgoEntry> make_algorithms();

}  // namespace dfs

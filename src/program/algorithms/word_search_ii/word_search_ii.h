#pragma once

#include <string>
#include <vector>

#include "dfs_types.h"
#include "harness/accelerator.h"

namespace dfs {

long word_search_ii(const Grid& board, const std::vector<std::string>& words,
                    Accelerator& accel, bool use_pruning = true);

}  // namespace dfs

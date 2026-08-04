#pragma once

#include <string>
#include <vector>

#include "dfs_types.h"

namespace dfs {

long oracle_number_of_islands(const Grid& grid, Connectivity connectivity);
long oracle_longest_increasing_path(const Grid& grid, Connectivity connectivity);
long oracle_pacific_atlantic(const Grid& grid, Connectivity connectivity);
long oracle_word_search_ii(const Grid& grid, const std::vector<std::string>& words,
                           Connectivity connectivity);

}  // namespace dfs

#pragma once

#include "dfs_types.h"
#include "harness/accelerator.h"

namespace dfs {

long longest_increasing_path(const Grid& matrix, Accelerator& accel, bool use_memo = true);

}  // namespace dfs

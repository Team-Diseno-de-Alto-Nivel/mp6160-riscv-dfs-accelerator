#pragma once

#include <vector>

#include "harness/metrics.h"

namespace dfs {

bool report(const std::vector<RunMetrics>& metrics);

}  // namespace dfs

#pragma once
// Per-run metrics (SW-1) collected for every (case, mode) pair. Mirrors the
// experiment plan in the paper.

#include <cstdint>
#include <string>

namespace dfs {

struct RunMetrics {
    std::string algorithm;
    std::string case_name;
    bool accelerator_on = false;

    double sim_latency_ns = 0.0;  // sc_time
    std::uint64_t instruction_count = 0;
    double throughput_cells_per_s = 0.0;
    std::uint64_t visited_cells = 0;
    std::uint64_t expanded_nodes = 0;
    std::uint64_t peak_stack_depth = 0;
};

}  // namespace dfs

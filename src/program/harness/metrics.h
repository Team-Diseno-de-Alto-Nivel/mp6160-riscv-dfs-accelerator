#pragma once

#include <cstdint>
#include <string>

namespace dfs {

struct RunMetrics {
    std::string algorithm;
    std::string case_name;
    bool accelerator_on = false;

    double sim_latency_ns = 0.0;
    
    // Hardware timing metrics
    std::uint64_t hw_total_cycles = 0;
    std::uint64_t hw_pop_cycles = 0;
    std::uint64_t hw_visited_cycles = 0;
    std::uint64_t hw_mark_cycles = 0;
    std::uint64_t hw_neighbor_cycles = 0;
    std::uint64_t hw_push_cycles = 0;
    double hw_latency_ns = 0.0;

    std::uint64_t instruction_count = 0;
    double throughput_cells_per_s = 0.0;
    std::uint64_t visited_cells = 0;
    std::uint64_t expanded_nodes = 0;
    std::uint64_t peak_stack_depth = 0;

    long result_value = 0;
    bool passed = false;
};

}  // namespace dfs

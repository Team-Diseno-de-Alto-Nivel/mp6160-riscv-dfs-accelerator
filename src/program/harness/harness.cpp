#include "harness/harness.h"

#include "algorithms/dfs_algorithm.h"
#include "harness/accelerator.h"
#include "harness/instrumentation.h"

namespace dfs {

RunMetrics Harness::run_one(const AlgoEntry& algo, const TestCase& tc, bool accelerator_on) {
    Counters counters;
    const Mode mode = accelerator_on ? Mode::On : Mode::Off;
    auto accel = make_accelerator(mode, counters);
    accel->set_connectivity(tc.input.connectivity);
    accel->load_grid(tc.input.grid);

    const long value = algo.run(tc.input, *accel);

    const CostModel cost;
    RunMetrics m;
    auto t = accelerator.timing();
    m.hw_cycles = t.total_cycles;
    m.pop_cycles = t.pop_cycles;
    m.visit_cycles = t.visit_cycles;

    m.algorithm = algo.name;
    m.case_name = tc.name;
    m.accelerator_on = accelerator_on;
    m.sim_latency_ns = cost.latency_ns(counters);
    auto t = accelerator.timing();

    m.hw_cycles =
        t.pop_cycles +
        t.visited_cycles +
        t.mark_cycles +
        t.neighbor_cycles +
        t.push_cycles;

    m.pop_cycles=t.pop_cycles;

    m.instruction_count = counters.ops;
    m.visited_cells = counters.visited_cells;
    m.expanded_nodes = counters.expanded_nodes;
    m.peak_stack_depth = counters.peak_stack_depth;
    m.throughput_cells_per_s =
        m.sim_latency_ns > 0.0
            ? static_cast<double>(counters.expanded_nodes) / (m.sim_latency_ns * 1e-9)
            : 0.0;
    m.result_value = value;
    m.passed = tc.expected.valid && value == tc.expected.value;
    return m;
}

std::vector<RunMetrics> Harness::run_all() {
    std::vector<RunMetrics> out;
    CaseLoader loader;
    for (const AlgoEntry& algo : make_algorithms()) {
        const auto cases = loader.load(algo.dataset_key);
        for (const auto& tc : cases) {
            out.push_back(run_one(algo, tc, false));
            out.push_back(run_one(algo, tc, true));
        }
    }
    return out;
}

}  // namespace dfs

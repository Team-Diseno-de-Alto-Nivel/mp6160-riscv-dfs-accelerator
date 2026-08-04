#include "harness/harness.h"

#include "algorithms/dfs_algorithm.h"
#include "harness/accelerator.h"
#include "harness/instrumentation.h"

namespace dfs {

const char* tier_name(Tier tier) {
    switch (tier) {
        case Tier::Legacy: return "legacy";
        case Tier::Small: return "small";
        case Tier::Medium: return "medium";
        case Tier::Large: return "large";
    }
    return "legacy";
}

bool parse_tier(const std::string& text, Tier& out) {
    if (text == "legacy") { out = Tier::Legacy; return true; }
    if (text == "small") { out = Tier::Small; return true; }
    if (text == "medium") { out = Tier::Medium; return true; }
    if (text == "large") { out = Tier::Large; return true; }
    return false;
}

RunMetrics Harness::run_one(const AlgoEntry& algo, const TestCase& tc, Tier tier,
                            bool accelerator_on) {
    Counters counters;
    const Mode mode = accelerator_on ? Mode::On : Mode::Off;
    auto accel = make_accelerator(mode, counters);
    accel->set_connectivity(tc.input.connectivity);
    accel->load_grid(tc.input.grid);

    const long value = algo.run(tc.input, *accel);

    const CostModel cost;
    RunMetrics m;
    m.algorithm = algo.name;
    m.case_name = tc.name;
    m.tier = tier_name(tier);
    m.grid_cells = static_cast<std::uint64_t>(tc.input.grid.cells.size());
    m.accelerator_on = accelerator_on;
    m.sim_latency_ns = cost.latency_ns(counters);
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
        const Tier limit = algo.max_tier < max_tier_ ? algo.max_tier : max_tier_;
        for (const CaseSpec& spec : loader.specs(algo.dataset_key, limit)) {
            if (spec.tier > limit) continue;
            const TestCase tc = materialize(spec);
            out.push_back(run_one(algo, tc, spec.tier, false));
            out.push_back(run_one(algo, tc, spec.tier, true));
        }
    }
    return out;
}

}  // namespace dfs

#include "harness/report.h"

#include <cstdio>

namespace dfs {
namespace {

const RunMetrics* find_counterpart(const std::vector<RunMetrics>& all, const RunMetrics& m,
                                   bool accelerator_on) {
    for (const RunMetrics& o : all)
        if (o.algorithm == m.algorithm && o.case_name == m.case_name &&
            o.accelerator_on == accelerator_on)
            return &o;
    return nullptr;
}

}  // namespace

bool report(const std::vector<RunMetrics>& metrics) {
    bool all_passed = true;

    printf("\n=== Per-run metrics ===\n");
    printf("%-30s %-22s %10s %-4s %-4s %10s %14s %12s %10s\n", "ALGORITHM", "CASE",
           "CELLS", "ACC", "OK", "RESULT", "LAT(ns)", "OPS", "PEAK");
    for (const RunMetrics& m : metrics) {
        if (!m.passed) all_passed = false;
        printf("%-30s %-22s %10llu %-4s %-4s %10ld %14.0f %12llu %10llu\n",
               m.algorithm.c_str(), m.case_name.c_str(),
               static_cast<unsigned long long>(m.grid_cells),
               m.accelerator_on ? "ON" : "OFF", m.passed ? "ok" : "FAIL", m.result_value,
               m.sim_latency_ns, static_cast<unsigned long long>(m.instruction_count),
               static_cast<unsigned long long>(m.peak_stack_depth));
    }

    printf("\n=== Accelerator ON vs OFF (speedup = OFF latency / ON latency) ===\n");
    printf("%-30s %-22s %10s %14s %14s %8s\n", "ALGORITHM", "CASE", "CELLS", "OFF(ns)",
           "ON(ns)", "SPEEDUP");
    for (const RunMetrics& m : metrics) {
        if (m.accelerator_on) continue;
        const RunMetrics* on = find_counterpart(metrics, m, true);
        if (!on) continue;
        const double speedup = on->sim_latency_ns > 0.0 ? m.sim_latency_ns / on->sim_latency_ns : 0.0;
        printf("%-30s %-22s %10llu %14.0f %14.0f %7.2fx\n", m.algorithm.c_str(),
               m.case_name.c_str(), static_cast<unsigned long long>(m.grid_cells),
               m.sim_latency_ns, on->sim_latency_ns, speedup);
    }

    printf("\n=== CSV ===\n");
    printf("algorithm,case,accelerator_on,passed,result,latency_ns,instruction_count,"
           "expanded_nodes,visited_cells,peak_stack_depth,throughput_cells_per_s,"
           "grid_cells,tier\n");
    for (const RunMetrics& m : metrics) {
        printf("%s,%s,%d,%d,%ld,%.0f,%llu,%llu,%llu,%llu,%.2f,%llu,%s\n",
               m.algorithm.c_str(), m.case_name.c_str(), m.accelerator_on ? 1 : 0,
               m.passed ? 1 : 0, m.result_value, m.sim_latency_ns,
               static_cast<unsigned long long>(m.instruction_count),
               static_cast<unsigned long long>(m.expanded_nodes),
               static_cast<unsigned long long>(m.visited_cells),
               static_cast<unsigned long long>(m.peak_stack_depth),
               m.throughput_cells_per_s,
               static_cast<unsigned long long>(m.grid_cells), m.tier.c_str());
    }

    printf("\n%s (%zu runs)\n", all_passed ? "ALL CASES PASSED" : "SOME CASES FAILED",
           metrics.size());
    return all_passed;
}

}  // namespace dfs

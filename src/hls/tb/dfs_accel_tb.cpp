#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "algorithms/dfs_algorithm.h"
#include "cases/test_case.h"
#include "harness/accelerator.h"
#include "harness/instrumentation.h"

#include "dfs_accel.h"
#include "kernel_io.h"

namespace {

long run_software(const dfs::AlgoEntry &algo, const dfs::TestCase &tc, dfs::Counters &counters) {
    auto accel = dfs::make_accelerator(dfs::Mode::Off, counters);
    accel->set_connectivity(tc.input.connectivity);
    accel->load_grid(tc.input.grid);
    return algo.run(tc.input, *accel);
}

}  // namespace

int main() {
    dfs::CaseLoader loader;
    int total = 0;
    int failures = 0;
    int counter_mismatches = 0;

    std::printf("%-32s %-18s %8s %8s %8s %10s %10s %8s %8s  %s\n", "algorithm", "case",
                "expect", "sw", "hls", "nodes_sw", "nodes_hls", "peak_sw", "peak_hls",
                "status");

    for (const dfs::AlgoEntry &algo : dfs::make_algorithms()) {
        dfs_hls_io::KernelConfig cfg;
        if (!dfs_hls_io::config_for(algo.name, cfg)) {
            std::printf("unmapped algorithm: %s\n", algo.name.c_str());
            ++failures;
            continue;
        }

        for (const dfs::TestCase &tc : loader.load(algo.dataset_key)) {
            ++total;

            std::vector<dfs_hls::cell_t> grid;
            std::vector<std::uint8_t> words;
            if (!dfs_hls_io::pack_grid(tc.input.grid, grid) ||
                !dfs_hls_io::pack_words(tc.input.words, words)) {
                std::printf("%-32s %-18s  packing failed\n", algo.name.c_str(),
                            tc.name.c_str());
                ++failures;
                continue;
            }

            const std::uint32_t flags = dfs_hls_io::flags_for(cfg, tc.input.connectivity);

            std::uint32_t result[dfs_hls::kResultWords] = {0, 0, 0, 0};
            dfs_accel(grid.data(), words.data(), result,
                      static_cast<std::uint32_t>(tc.input.grid.rows),
                      static_cast<std::uint32_t>(tc.input.grid.cols), cfg.mode, flags,
                      static_cast<std::uint32_t>(tc.input.words.size()));

            dfs::Counters counters;
            const long sw = run_software(algo, tc, counters);
            const long hls = static_cast<long>(result[dfs_hls::kSlotValue]);

            const bool value_ok = hls == tc.expected.value && hls == sw;
            const bool nodes_ok = result[dfs_hls::kSlotExpandedNodes] == counters.expanded_nodes;
            const bool peak_ok =
                result[dfs_hls::kSlotPeakStackDepth] == counters.peak_stack_depth;
            const bool visited_ok =
                result[dfs_hls::kSlotVisitedCells] == counters.visited_cells;
            const bool counters_ok = nodes_ok && peak_ok && visited_ok;

            if (!value_ok) ++failures;
            if (value_ok && !counters_ok) ++counter_mismatches;

            const char *status = !value_ok ? "FAIL" : (counters_ok ? "ok" : "ok*");

            if (value_ok && !visited_ok)
                std::printf("%-32s %-18s  visited_cells sw=%llu hls=%u\n", algo.name.c_str(),
                            tc.name.c_str(),
                            static_cast<unsigned long long>(counters.visited_cells),
                            result[dfs_hls::kSlotVisitedCells]);

            std::printf("%-32s %-18s %8ld %8ld %8ld %10llu %10u %8llu %8u  %s\n",
                        algo.name.c_str(), tc.name.c_str(), tc.expected.value, sw, hls,
                        static_cast<unsigned long long>(counters.expanded_nodes),
                        result[dfs_hls::kSlotExpandedNodes],
                        static_cast<unsigned long long>(counters.peak_stack_depth),
                        result[dfs_hls::kSlotPeakStackDepth], status);
        }
    }

    std::printf("\n%d/%d cases match the golden result\n", total - failures, total);
    if (counter_mismatches > 0)
        std::printf("%d case(s) marked ok* differ in traversal counters\n",
                    counter_mismatches);

    return failures == 0 ? 0 : 1;
}

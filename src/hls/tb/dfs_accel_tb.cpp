#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "algorithms/dfs_algorithm.h"
#include "cases/test_case.h"
#include "harness/accelerator.h"
#include "harness/instrumentation.h"

#include "dfs_accel.h"

namespace {

struct KernelConfig {
    std::uint32_t mode;
    std::uint32_t flags;
};

bool config_for(const std::string &algorithm, KernelConfig &out) {
    if (algorithm == "number_of_islands") {
        out = {dfs_hls::kNumberOfIslands, 0};
    } else if (algorithm == "unique_paths_iii") {
        out = {dfs_hls::kUniquePathsIii, 0};
    } else if (algorithm == "word_search_ii") {
        out = {dfs_hls::kWordSearchIi, dfs_hls::kFlagPrune};
    } else if (algorithm == "word_search_ii_nopruning") {
        out = {dfs_hls::kWordSearchIi, 0};
    } else if (algorithm == "longest_increasing_path") {
        out = {dfs_hls::kLongestIncreasingPath, dfs_hls::kFlagMemo};
    } else if (algorithm == "longest_increasing_path_nomemo") {
        out = {dfs_hls::kLongestIncreasingPath, 0};
    } else if (algorithm == "pacific_atlantic") {
        out = {dfs_hls::kPacificAtlantic, 0};
    } else {
        return false;
    }
    return true;
}

bool pack_grid(const dfs::Grid &grid, std::vector<dfs_hls::cell_t> &out) {
    out.assign(grid.cells.size(), 0);
    for (std::size_t i = 0; i < grid.cells.size(); ++i) {
        const int v = grid.cells[i];
        if (v < -128 || v > 127) return false;
        out[i] = static_cast<dfs_hls::cell_t>(v);
    }
    return true;
}

bool pack_words(const std::vector<std::string> &words, std::vector<std::uint8_t> &out) {
    out.assign(dfs_hls::kMaxWords * dfs_hls::kMaxWordLen, 0);
    if (words.size() > dfs_hls::kMaxWords) return false;
    for (std::size_t w = 0; w < words.size(); ++w) {
        if (words[w].size() > dfs_hls::kMaxWordLen) return false;
        for (std::size_t k = 0; k < words[w].size(); ++k)
            out[w * dfs_hls::kMaxWordLen + k] = static_cast<std::uint8_t>(words[w][k]);
    }
    return true;
}

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
        KernelConfig cfg;
        if (!config_for(algo.name, cfg)) {
            std::printf("unmapped algorithm: %s\n", algo.name.c_str());
            ++failures;
            continue;
        }

        for (const dfs::TestCase &tc : loader.load(algo.dataset_key)) {
            ++total;

            std::vector<dfs_hls::cell_t> grid;
            std::vector<std::uint8_t> words;
            if (!pack_grid(tc.input.grid, grid) || !pack_words(tc.input.words, words)) {
                std::printf("%-32s %-18s  packing failed\n", algo.name.c_str(),
                            tc.name.c_str());
                ++failures;
                continue;
            }

            std::uint32_t flags = cfg.flags;
            if (tc.input.connectivity == dfs::Connectivity::Eight)
                flags |= dfs_hls::kFlagEightConnected;

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

// Exports golden cases to JSON for on-board validation from PYNQ (issue
// #67). Reuses the exact packing/config logic already proven by the
// cosim testbench (kernel_io.h, shared with tb/dfs_accel_tb.cpp) instead of
// re-deriving it in Python, so the grid/words bytes and the mode/flags for
// each case are guaranteed to match what dfs_accel() was actually cosim'd
// against -- a hand-written Python packer could silently diverge and give
// a false correctness signal on real hardware.
//
// Plain host build, no Vitis/Vivado needed: `make onboard-export-cases`.
// Output: src/onboard/cases.json (versioned -- unlike the bitstream, this file
// has to travel to the KV260, and it's small/text, so it's committed).
//
// Optional [tier] argument (default legacy): also export the generated
// datasets up to that tier (#109's synthetic.cpp), intersected with each
// algorithm's own max_tier the same way Harness::run_all() does (harness.cpp)
// -- skipping that intersection let a 64x64 no-memo longest-increasing-path
// case slip through once and hang (unbounded exponential search on a long
// gradient path). small (64x64) is the largest tier the HLS kernel can hold
// (kMaxRows/kMaxCols = 64, dfs_accel.h); medium/large exceed it and aren't
// supported here. `make onboard-export-cases-small` -> cases_small.json,
// used by #92's scale-sensitivity follow-up (see README.md).

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

void write_json_string(std::FILE *f, const std::string &s) {
    std::fputc('"', f);
    for (char c : s) {
        if (c == '"' || c == '\\') std::fputc('\\', f);
        std::fputc(c, f);
    }
    std::fputc('"', f);
}

template <typename T>
void write_int_array(std::FILE *f, const std::vector<T> &values) {
    std::fputc('[', f);
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) std::fputc(',', f);
        std::fprintf(f, "%ld", static_cast<long>(values[i]));
    }
    std::fputc(']', f);
}

}  // namespace

int main(int argc, char **argv) {
    if (argc < 2 || argc > 3) {
        std::fprintf(stderr, "usage: %s <output.json> [tier=legacy|small]\n", argv[0]);
        return 1;
    }

    // small (64x64) is the largest tier the HLS kernel can hold (kMaxRows/
    // kMaxCols = 64, see dfs_accel.h); medium/large exceed it, so exporting
    // them here would silently hand the board a fixture it can't run.
    dfs::Tier tier = dfs::Tier::Legacy;
    if (argc == 3) {
        const std::string t = argv[2];
        if (t == "legacy") {
            tier = dfs::Tier::Legacy;
        } else if (t == "small") {
            tier = dfs::Tier::Small;
        } else {
            std::fprintf(stderr, "export_cases: unsupported tier '%s' (use legacy|small)\n",
                         t.c_str());
            return 1;
        }
    }

    std::FILE *f = std::fopen(argv[1], "w");
    if (!f) {
        std::fprintf(stderr, "export_cases: could not open '%s' for writing\n", argv[1]);
        return 1;
    }

    dfs::CaseLoader loader;
    int total = 0;
    int failures = 0;

    std::fputs("[\n", f);
    bool first_case = true;

    for (const dfs::AlgoEntry &algo : dfs::make_algorithms()) {
        dfs_hls_io::KernelConfig cfg;
        if (!dfs_hls_io::config_for(algo.name, cfg)) {
            std::fprintf(stderr, "export_cases: unmapped algorithm: %s\n", algo.name.c_str());
            ++failures;
            continue;
        }

        // Intersect with the algorithm's own cap (exponential variants like
        // longest_increasing_path_nomemo are pinned to Legacy) -- same rule
        // Harness::run_all() applies (harness.cpp). Skipping this let a
        // 64x64 no-memo LIP case slip through and hang (unbounded exponential
        // search on a long gradient path).
        const dfs::Tier limit = algo.max_tier < tier ? algo.max_tier : tier;
        for (const dfs::CaseSpec &spec : loader.specs(algo.dataset_key, limit)) {
            if (spec.tier > limit) continue;
            const dfs::TestCase tc = dfs::materialize(spec);
            ++total;

            std::vector<dfs_hls::cell_t> grid;
            std::vector<std::uint8_t> words;
            if (!dfs_hls_io::pack_grid(tc.input.grid, grid) ||
                !dfs_hls_io::pack_words(tc.input.words, words)) {
                std::fprintf(stderr, "export_cases: packing failed for %s/%s\n",
                             algo.name.c_str(), tc.name.c_str());
                ++failures;
                continue;
            }

            const std::uint32_t flags = dfs_hls_io::flags_for(cfg, tc.input.connectivity);

            dfs::Counters counters;
            const long sw = run_software(algo, tc, counters);
            if (sw != tc.expected.value) {
                std::fprintf(stderr,
                             "export_cases: %s/%s software result (%ld) disagrees with the "
                             "golden expected value (%ld) -- refusing to export a bad fixture\n",
                             algo.name.c_str(), tc.name.c_str(), sw, tc.expected.value);
                ++failures;
                continue;
            }

            // grid holds signed cell_t (int8_t); std::vector<dfs_hls::cell_t>
            // isn't a plain int, so copy through int for write_int_array.
            std::vector<int> grid_ints(grid.begin(), grid.end());
            std::vector<int> word_ints(words.begin(), words.end());

            if (!first_case) std::fputs(",\n", f);
            first_case = false;

            std::fputs("  {\n", f);
            std::fputs("    \"algorithm\": ", f);
            write_json_string(f, algo.name);
            std::fputs(",\n    \"case\": ", f);
            write_json_string(f, tc.name);
            std::fprintf(f, ",\n    \"mode\": %u", cfg.mode);
            std::fprintf(f, ",\n    \"flags\": %u", flags);
            std::fprintf(f, ",\n    \"rows\": %d", tc.input.grid.rows);
            std::fprintf(f, ",\n    \"cols\": %d", tc.input.grid.cols);
            std::fprintf(f, ",\n    \"num_words\": %zu", tc.input.words.size());
            std::fputs(",\n    \"grid\": ", f);
            write_int_array(f, grid_ints);
            std::fputs(",\n    \"words\": ", f);
            write_int_array(f, word_ints);
            std::fprintf(f, ",\n    \"expected_value\": %ld", tc.expected.value);
            std::fprintf(f, ",\n    \"expected_expanded_nodes\": %llu",
                         static_cast<unsigned long long>(counters.expanded_nodes));
            std::fprintf(f, ",\n    \"expected_visited_cells\": %llu",
                         static_cast<unsigned long long>(counters.visited_cells));
            std::fprintf(f, ",\n    \"expected_peak_stack_depth\": %llu",
                         static_cast<unsigned long long>(counters.peak_stack_depth));
            std::fputs("\n  }", f);
        }
    }

    std::fputs("\n]\n", f);
    std::fclose(f);

    std::fprintf(stderr, "export_cases: wrote %d/%d cases to %s\n", total - failures, total,
                 argv[1]);
    return failures == 0 ? 0 : 1;
}

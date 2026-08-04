#pragma once

// Host-side packing/config helpers shared by every consumer of dfs_accel()
// outside of Vitis HLS itself: the cosim testbench (tb/dfs_accel_tb.cpp,
// #63) and the PYNQ case exporter (tools/export_cases.cpp, #67). Extracted
// so both always agree on the mode/flags mapping and the exact byte layout
// dfs_accel() expects -- a hand-rolled second copy in either place could
// silently drift and produce a kernel call that looks right but isn't.

#include <cstdint>
#include <string>
#include <vector>

#include "dfs_accel.h"
#include "dfs_types.h"

namespace dfs_hls_io {

struct KernelConfig {
    std::uint32_t mode;
    std::uint32_t flags;
};

inline bool config_for(const std::string &algorithm, KernelConfig &out) {
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

inline bool pack_grid(const dfs::Grid &grid, std::vector<dfs_hls::cell_t> &out) {
    out.assign(grid.cells.size(), 0);
    for (std::size_t i = 0; i < grid.cells.size(); ++i) {
        const int v = grid.cells[i];
        if (v < -128 || v > 127) return false;
        out[i] = static_cast<dfs_hls::cell_t>(v);
    }
    return true;
}

inline bool pack_words(const std::vector<std::string> &words, std::vector<std::uint8_t> &out) {
    out.assign(dfs_hls::kMaxWords * dfs_hls::kMaxWordLen, 0);
    if (words.size() > dfs_hls::kMaxWords) return false;
    for (std::size_t w = 0; w < words.size(); ++w) {
        if (words[w].size() > dfs_hls::kMaxWordLen) return false;
        for (std::size_t k = 0; k < words[w].size(); ++k)
            out[w * dfs_hls::kMaxWordLen + k] = static_cast<std::uint8_t>(words[w][k]);
    }
    return true;
}

// flags for a case = the algorithm's base config flags (memo/prune) plus
// kFlagEightConnected if the case itself asks for 8-connectivity -- same
// composition dfs_accel_tb.cpp did inline before this was factored out.
inline std::uint32_t flags_for(const KernelConfig &cfg, dfs::Connectivity connectivity) {
    std::uint32_t flags = cfg.flags;
    if (connectivity == dfs::Connectivity::Eight) flags |= dfs_hls::kFlagEightConnected;
    return flags;
}

}  // namespace dfs_hls_io

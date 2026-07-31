#pragma once

#include <cstdint>

namespace dfs_hls {

constexpr std::uint32_t kMaxRows = 64;
constexpr std::uint32_t kMaxCols = 64;
constexpr std::uint32_t kMaxCells = kMaxRows * kMaxCols;
constexpr std::uint32_t kMaxStack = kMaxCells;
constexpr std::uint32_t kMaxWords = 16;
constexpr std::uint32_t kMaxWordLen = 16;
constexpr std::uint32_t kMaxTrieNodes = kMaxWords * kMaxWordLen + 1;
constexpr std::uint32_t kResultWords = 4;
constexpr std::uint32_t kFootprintBudgetBytes = 128u * 1024u;

using cell_t = std::int8_t;

enum Mode : std::uint32_t {
    kNumberOfIslands = 0,
    kUniquePathsIii = 1,
    kWordSearchIi = 2,
    kLongestIncreasingPath = 3,
    kPacificAtlantic = 4,
};

enum Flag : std::uint32_t {
    kFlagEightConnected = 1u << 0,
    kFlagMemo = 1u << 1,
    kFlagPrune = 1u << 2,
};

enum ResultSlot : std::uint32_t {
    kSlotValue = 0,
    kSlotExpandedNodes = 1,
    kSlotVisitedCells = 2,
    kSlotPeakStackDepth = 3,
};

}  // namespace dfs_hls

void dfs_accel(const dfs_hls::cell_t *grid_in,
               const std::uint8_t *words_in,
               std::uint32_t *result_out,
               std::uint32_t rows,
               std::uint32_t cols,
               std::uint32_t mode,
               std::uint32_t flags,
               std::uint32_t num_words);

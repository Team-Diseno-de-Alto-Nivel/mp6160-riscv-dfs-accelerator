#pragma once
// Compile-time configuration of the DFS accelerator model.

#include <cstdint>

namespace dfs::config {

inline constexpr std::uint32_t kMaxRows = 256;
inline constexpr std::uint32_t kMaxCols = 256;
inline constexpr std::uint32_t kMaxCells = kMaxRows * kMaxCols;

inline constexpr std::uint32_t kStackDepth = kMaxCells;  // Stack Manager capacity
inline constexpr std::uint32_t kBusWidthBytes = 4;       // TLM data-bus width

}  // namespace dfs::config

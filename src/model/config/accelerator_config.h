#pragma once
// Compile-time configuration of the DFS accelerator model.

#include <cstdint>

namespace dfs::config {

inline constexpr std::uint32_t kMaxRows = 256;
inline constexpr std::uint32_t kMaxCols = 256;
inline constexpr std::uint32_t kMaxCells = kMaxRows * kMaxCols;

inline constexpr std::uint32_t kStackDepth = kMaxCells;  // Stack Manager capacity
inline constexpr std::uint32_t kBusWidthBytes = 4;       // TLM data-bus width

//Collection of cycle counts for each operation in the DFS accelerator model
constexpr uint32_t POP_CYCLES = 1;
constexpr uint32_t VISITED_READ_CYCLES = 2;
constexpr uint32_t MARK_VISITED_CYCLES = 1;
constexpr uint32_t NEIGHBOR_GEN_CYCLES = 1;
constexpr uint32_t PUSH_CYCLES = 1;
constexpr sc_time CLOCK_PERIOD(10, SC_NS);

}  // namespace dfs::config

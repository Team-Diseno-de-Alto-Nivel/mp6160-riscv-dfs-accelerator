#pragma once
// Accelerator memory map as seen from the host over TLM (HWC-2).

#include <cstdint>

namespace dfs::memmap {

// Control / configuration registers.
inline constexpr std::uint64_t kRegControl = 0x0000;
inline constexpr std::uint64_t kRegStatus  = 0x0004;
inline constexpr std::uint64_t kRegRows    = 0x0008;
inline constexpr std::uint64_t kRegCols    = 0x000C;
inline constexpr std::uint64_t kRegStartX  = 0x0010;
inline constexpr std::uint64_t kRegStartY  = 0x0014;
inline constexpr std::uint64_t kRegParams  = 0x0018;

// Result registers.
inline constexpr std::uint64_t kRegResult    = 0x0020;
inline constexpr std::uint64_t kRegVisited   = 0x0024;
inline constexpr std::uint64_t kRegPeakStack = 0x0028;

// Grid payload window.
inline constexpr std::uint64_t kGridBase = 0x1000;

// Control-register bits.
inline constexpr std::uint32_t kControlStart  = 0x1;
inline constexpr std::uint32_t kControlEnable = 0x2;  // ON/OFF toggle

// Status-register bits.
inline constexpr std::uint32_t kStatusBusy     = 0x1;
inline constexpr std::uint32_t kStatusDone     = 0x2;
// A push was dropped this run (stack hit kStackDepth) — result under-counts.
// Sticky until the next run starts.
inline constexpr std::uint32_t kStatusOverflow = 0x4;

}  // namespace dfs::memmap

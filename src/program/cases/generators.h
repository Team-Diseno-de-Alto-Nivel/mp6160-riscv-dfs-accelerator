#pragma once

#include <cstdint>

#include "cases/grid_spec.h"
#include "dfs_types.h"

namespace dfs {

class Rng {
  public:
    explicit Rng(std::uint64_t seed) {
        std::uint64_t z = seed + 0x9e3779b97f4a7c15ull;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
        z ^= z >> 31;
        state_ = z ? z : 0x9e3779b97f4a7c15ull;
    }

    std::uint64_t next() {
        state_ ^= state_ >> 12;
        state_ ^= state_ << 25;
        state_ ^= state_ >> 27;
        return state_ * 0x2545f4914f6cdd1dull;
    }

    std::uint32_t below(std::uint32_t bound) {
        return bound ? static_cast<std::uint32_t>((next() >> 32) % bound) : 0;
    }

    bool chance(int percent) { return static_cast<int>(below(100)) < percent; }

  private:
    std::uint64_t state_;
};

Grid generate_grid(const GridSpec& spec);

}  // namespace dfs

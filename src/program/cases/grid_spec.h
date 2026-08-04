#pragma once

#include <cstdint>

namespace dfs {

enum class Tier { Legacy = 0, Small = 1, Medium = 2, Large = 3 };

enum class Topology {
    UniformRandom,
    Checkerboard,
    AllLand,
    AllWater,
    Serpentine,
    DiagonalStripes,
    Gradient,
    RandomHeights,
    Letters,
};

struct GridSpec {
    Topology topology = Topology::UniformRandom;
    int rows = 0;
    int cols = 0;
    int density = 50;
    int levels = 16;
    int alphabet = 4;
    std::uint64_t seed = 1;
};

}  // namespace dfs

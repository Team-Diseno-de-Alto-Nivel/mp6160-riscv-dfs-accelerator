#include "cases/generators.h"

namespace dfs {

Grid generate_grid(const GridSpec& spec) {
    Grid g;
    g.rows = spec.rows;
    g.cols = spec.cols;
    if (spec.rows <= 0 || spec.cols <= 0) return g;

    g.cells.assign(static_cast<std::size_t>(spec.rows) * spec.cols, 0);
    Rng rng(spec.seed);

    for (int r = 0; r < spec.rows; ++r) {
        for (int c = 0; c < spec.cols; ++c) {
            int value = 0;
            switch (spec.topology) {
                case Topology::UniformRandom:
                    value = rng.chance(spec.density) ? 1 : 0;
                    break;
                case Topology::Checkerboard:
                    value = ((r + c) % 2 == 0) ? 1 : 0;
                    break;
                case Topology::AllLand:
                    value = 1;
                    break;
                case Topology::AllWater:
                    value = 0;
                    break;
                case Topology::Serpentine:
                    if (r % 2 == 0) {
                        value = 1;
                    } else {
                        const int connector = ((r / 2) % 2 == 0) ? spec.cols - 1 : 0;
                        value = (c == connector) ? 1 : 0;
                    }
                    break;
                case Topology::DiagonalStripes:
                    value = ((r + c) % 4 < 2) ? 1 : 0;
                    break;
                case Topology::Gradient:
                    value = r + c;
                    break;
                case Topology::RandomHeights:
                    value = static_cast<int>(rng.below(static_cast<std::uint32_t>(spec.levels)));
                    break;
                case Topology::Letters:
                    value = 'a' + static_cast<int>(rng.below(static_cast<std::uint32_t>(spec.alphabet)));
                    break;
            }
            g.cells[g.index(r, c)] = value;
        }
    }
    return g;
}

}  // namespace dfs

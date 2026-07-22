#pragma once
// Accelerator seam (SW-1): the same algorithm runs with Mode::Off (all software)
// or Mode::On (DFS primitives offloaded to the modelled accelerator), so the
// harness can compare both.

#include "dfs_types.h"

namespace dfs {

enum class Mode { Off, On };

class Accelerator {
  public:
    explicit Accelerator(Mode mode);
    virtual ~Accelerator() = default;

    Mode mode() const { return mode_; }

    virtual void load_grid(const Grid& grid) = 0;
    virtual void reset_visited() = 0;
    virtual bool is_visited(Coord c) const = 0;
    virtual void mark_visited(Coord c) = 0;
    virtual std::vector<Coord> neighbors(Coord c) const = 0;

  protected:
    Mode mode_;
};

}  // namespace dfs

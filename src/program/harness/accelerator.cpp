#include "harness/accelerator.h"

#include <algorithm>

namespace dfs {
namespace {

constexpr int kDr[8] = {-1, 1, 0, 0, -1, -1, 1, 1};
constexpr int kDc[8] = {0, 0, -1, 1, -1, 1, -1, 1};

class BaseAccelerator : public Accelerator {
  public:
    using Accelerator::Accelerator;

    void load_grid(const Grid& grid) override {
        grid_ = &grid;
        visited_.assign(static_cast<std::size_t>(grid.rows) * grid.cols, false);
    }

    void reset_visited() override {
        std::fill(visited_.begin(), visited_.end(), false);
    }

    bool is_visited(Coord c) const override {
        ++counters_.ops;
        return visited_[grid_->index(c.row, c.col)];
    }

    void mark_visited(Coord c) override {
        ++counters_.ops;
        const int i = grid_->index(c.row, c.col);
        if (!visited_[i]) {
            visited_[i] = true;
            ++counters_.visited_cells;
        }
    }

    void unmark_visited(Coord c) override {
        ++counters_.ops;
        visited_[grid_->index(c.row, c.col)] = false;
    }

    std::vector<Coord> neighbors(Coord c) const override {
        const int count = (connectivity_ == Connectivity::Eight) ? 8 : 4;
        charge_neighbor_cost(count);
        std::vector<Coord> out;
        for (int i = 0; i < count; ++i) {
            const int r = c.row + kDr[i];
            const int col = c.col + kDc[i];
            if (grid_->in_bounds(r, col)) out.push_back({r, col});
        }
        return out;
    }

  protected:
    virtual void charge_neighbor_cost(int candidates) const = 0;

  private:
    std::vector<bool> visited_;
};

class SoftwareAccelerator final : public BaseAccelerator {
  public:
    using BaseAccelerator::BaseAccelerator;

  protected:
    void charge_neighbor_cost(int candidates) const override {
        counters_.ops += static_cast<std::uint64_t>(candidates);
    }
};

class ReferenceAccelerator final : public BaseAccelerator {
  public:
    using BaseAccelerator::BaseAccelerator;

  protected:
    void charge_neighbor_cost(int) const override { ++counters_.ops; }
};

}  // namespace

std::unique_ptr<Accelerator> make_accelerator(Mode mode, Counters& counters) {
    if (mode == Mode::On) {
        return std::make_unique<ReferenceAccelerator>(mode, counters);
    }
    return std::make_unique<SoftwareAccelerator>(mode, counters);
}

}  // namespace dfs

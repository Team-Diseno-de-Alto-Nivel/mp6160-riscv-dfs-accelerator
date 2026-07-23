#pragma once

#include <cstddef>
#include <cstdint>

#include "config/memory_map.h"
#include "dfs_types.h"

namespace dfs::driver {

class Bus {
  public:
    virtual ~Bus() = default;
    virtual void write32(std::uint64_t offset, std::uint32_t value) = 0;
    virtual std::uint32_t read32(std::uint64_t offset) const = 0;
};

class RawBus final : public Bus {
  public:
    explicit RawBus(std::uintptr_t base)
        : base_(reinterpret_cast<volatile std::uint32_t*>(base)) {}

    void write32(std::uint64_t offset, std::uint32_t value) override {
        base_[offset / sizeof(std::uint32_t)] = value;
    }

    std::uint32_t read32(std::uint64_t offset) const override {
        return base_[offset / sizeof(std::uint32_t)];
    }

  private:
    volatile std::uint32_t* base_;
};

class AcceleratorDriver {
  public:
    explicit AcceleratorDriver(Bus& bus) : bus_(bus) {}

    void set_enable(bool on) {
        std::uint32_t ctrl = bus_.read32(memmap::kRegControl);
        ctrl = on ? (ctrl | memmap::kControlEnable)
                  : (ctrl & ~memmap::kControlEnable);
        bus_.write32(memmap::kRegControl, ctrl);
    }

    void load_grid(const Grid& grid, Coord origin, std::uint32_t params = 0) {
        bus_.write32(memmap::kRegRows, static_cast<std::uint32_t>(grid.rows));
        bus_.write32(memmap::kRegCols, static_cast<std::uint32_t>(grid.cols));
        bus_.write32(memmap::kRegStartX, static_cast<std::uint32_t>(origin.col));
        bus_.write32(memmap::kRegStartY, static_cast<std::uint32_t>(origin.row));
        bus_.write32(memmap::kRegParams, params);
        for (std::size_t i = 0; i < grid.cells.size(); ++i) {
            bus_.write32(memmap::kGridBase + i * sizeof(std::uint32_t),
                         static_cast<std::uint32_t>(grid.cells[i]));
        }
    }

    void start() {
        const std::uint32_t ctrl = bus_.read32(memmap::kRegControl);
        bus_.write32(memmap::kRegControl, ctrl | memmap::kControlStart);
    }

    bool busy() const {
        return (bus_.read32(memmap::kRegStatus) & memmap::kStatusBusy) != 0;
    }

    bool done() const {
        return (bus_.read32(memmap::kRegStatus) & memmap::kStatusDone) != 0;
    }

    void wait_done() const {
        while (!done()) {
        }
    }

    std::uint32_t result() const { return bus_.read32(memmap::kRegResult); }
    std::uint32_t visited() const { return bus_.read32(memmap::kRegVisited); }
    std::uint32_t peak_stack() const { return bus_.read32(memmap::kRegPeakStack); }

    AlgoResult run(const Grid& grid, Coord origin, std::uint32_t params = 0) {
        load_grid(grid, origin, params);
        set_enable(true);
        start();
        wait_done();
        return {static_cast<long>(result()), true};
    }

  private:
    Bus& bus_;
};

}  // namespace dfs::driver

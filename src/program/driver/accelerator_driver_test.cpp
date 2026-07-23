#include "driver/accelerator_driver.h"

#include <cstdio>
#include <unordered_map>

#include "config/memory_map.h"
#include "dfs_types.h"

namespace {

using dfs::AlgoResult;
using dfs::Coord;
using dfs::Grid;
using dfs::driver::AcceleratorDriver;
using dfs::driver::Bus;
namespace memmap = dfs::memmap;

class FakeBus final : public Bus {
  public:
    void write32(std::uint64_t offset, std::uint32_t value) override {
        regs_[offset] = value;
    }

    std::uint32_t read32(std::uint64_t offset) const override {
        if (offset == memmap::kRegStatus) return memmap::kStatusDone;
        if (offset == memmap::kRegResult) return result_;
        const auto it = regs_.find(offset);
        return it == regs_.end() ? 0u : it->second;
    }

    void set_result(std::uint32_t v) { result_ = v; }

    std::uint32_t at(std::uint64_t offset) const {
        const auto it = regs_.find(offset);
        return it == regs_.end() ? 0u : it->second;
    }

  private:
    std::unordered_map<std::uint64_t, std::uint32_t> regs_;
    std::uint32_t result_ = 0;
};

int failures = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

}  // namespace

int main() {
    FakeBus bus;
    bus.set_result(42);
    AcceleratorDriver drv(bus);

    Grid grid;
    grid.rows = 2;
    grid.cols = 3;
    grid.cells = {1, 0, 1, 0, 1, 0};

    const AlgoResult r = drv.run(grid, Coord{1, 2}, 0x5);

    check(bus.at(memmap::kRegRows) == 2, "rows written");
    check(bus.at(memmap::kRegCols) == 3, "cols written");
    check(bus.at(memmap::kRegStartX) == 2, "start x holds column");
    check(bus.at(memmap::kRegStartY) == 1, "start y holds row");
    check(bus.at(memmap::kRegParams) == 0x5, "params written");
    check(bus.at(memmap::kGridBase + 0) == 1, "grid cell 0 written");
    check(bus.at(memmap::kGridBase + 2 * sizeof(std::uint32_t)) == 1,
          "grid cell 2 written at word stride");
    check((bus.at(memmap::kRegControl) & memmap::kControlEnable) != 0,
          "enable bit set");
    check((bus.at(memmap::kRegControl) & memmap::kControlStart) != 0,
          "start bit set");
    check(r.valid && r.value == 42, "result read back");

    if (failures == 0) {
        std::printf("accelerator_driver_test: all checks passed\n");
    }
    return failures == 0 ? 0 : 1;
}

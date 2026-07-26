#pragma once
// Grid Memory (HWC-1): read-only input grid, loaded by the host.
//
// commit()/preview() split for the same reason as StackManager/
// VisitedMemory: raddr is driven off the same wire as VisitedMemory.addr,
// so rdata needs to line up with vis_is_visited one cycle after the address.
//
// load_cell() is a backdoor write used by DfsAccelerator::write_reg(): the
// host loads the grid with one TLM write per cell and no wait() in between,
// and sc_signal only keeps the last pending write before the kernel catches
// up — so the we/waddr/wdata port below would silently lose every cell but
// the last. It's kept for a hypothetical properly-paced write path, just
// not what the real loading path uses.

#include <cstdint>

#include <systemc.h>

#include "config/accelerator_config.h"
#include "utils/types.h"

namespace dfs {

SC_MODULE(GridMemory) {
    sc_in<bool> clk;

    // Write port (host data-loading).
    sc_in<bool> we;
    sc_in<sc_uint<32>> waddr;
    sc_in<sc_int<32>> wdata;

    // Read port (traversal).
    sc_in<sc_uint<32>> raddr;
    sc_out<sc_int<32>> rdata;

    SC_CTOR(GridMemory) {
        SC_METHOD(commit);
        sensitive << clk.pos();
        dont_initialize();

        SC_METHOD(preview);
        sensitive << we << waddr << wdata << raddr;
    }

    // Backdoor bulk-load write — see the file comment.
    void load_cell(std::uint32_t index, CellValue value) {
        if (index < config::kMaxCells) cells_[index] = value;
    }

    // Actual registered write, one cycle after we is asserted.
    void commit() {
        const std::uint32_t w = waddr.read();
        if (we.read() && w < config::kMaxCells) {
            cells_[w] = static_cast<CellValue>(wdata.read());
        }
    }

    // Preview of this cycle's request, see file comment.
    void preview() {
        const std::uint32_t r = raddr.read();
        if (we.read() && waddr.read() == r && r < config::kMaxCells) {
            rdata.write(wdata.read());
        } else {
            rdata.write(r < config::kMaxCells ? cells_[r] : 0);
        }
    }

  private:
    CellValue cells_[config::kMaxCells] = {};
};

}  // namespace dfs

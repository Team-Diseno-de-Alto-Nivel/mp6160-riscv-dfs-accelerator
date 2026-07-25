#pragma once
// Grid Memory (HWC-1): read-only input grid, loaded by the host.
//
// Split into commit()/preview() for the same reason as StackManager/
// VisitedMemory (see the timing note atop stack_manager.h): a purely
// clocked design would only reflect a newly-asserted raddr on rdata two
// cycles later. raddr is driven by DfsAccelerator off the same wire as
// VisitedMemory.addr (both memories are indexed identically), so
// DfsController's content check (see the peer-contract note atop
// dfs_controller.h) sees rdata exactly one cycle after asserting the
// address, same as vis_is_visited.
//
// load_cell() is a backdoor, non-signal write used by
// DfsAccelerator::write_reg() to actually load the grid (see its call site
// for why): the host issues one TLM write per cell in a tight loop with no
// wait() in between (AcceleratorDriver::load_grid()), and sc_signal only
// keeps the *last* pending value written to it before the kernel next
// processes deltas — so every write but the final cell would otherwise be
// silently lost before this module's own clocked process ever saw them,
// regardless of whether that process is combinational or clocked. The we/
// waddr/wdata port below is kept for a hypothetical cycle-accurate write
// path (e.g. one word per clock, properly spaced with wait()) but is not
// what the host loading path actually uses.

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

    // Backdoor read, same rationale as load_cell(): used by the fine-grained
    // primitive TLM bridge (src/program/integration) to read a cell's value
    // directly rather than through raddr/rdata's clocked handshake — see
    // visited_memory.h's peek()/poke() comment for the fuller reasoning.
    CellValue peek(std::uint32_t index) const {
        return index < config::kMaxCells ? cells_[index] : 0;
    }

    // Synchronous: the actual, persistent array update, one cycle after we
    // is asserted (a normal registered write) — the cycle-accurate path.
    void commit() {
        const std::uint32_t w = waddr.read();
        if (we.read() && w < config::kMaxCells) {
            cells_[w] = static_cast<CellValue>(wdata.read());
        }
    }

    // Combinational: previews the effect of *this* cycle's we/waddr/wdata
    // request on rdata before commit() applies it at the next edge — see
    // the file comment.
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

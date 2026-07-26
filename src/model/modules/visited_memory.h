#pragma once
// Visited Memory (HWC-1): per-cell visited state, cleared at run start. Some
// algorithms need more than one instance (e.g. Pacific Atlantic).
//
// Split into commit()/preview() same reason as StackManager: a purely
// clocked design would reflect a new addr two cycles late, but Pop/Visit
// needs vis_is_visited one cycle after asserting vis_addr. clear beats a
// same-cycle we (both driven by the same Start pulse in dfs_accelerator.h).
//
// peek()/poke()/clear_all() are a backdoor for the primitive TLM bridge
// (src/program/integration) — the four algorithms that skip DfsController
// entirely. That path is one call per software primitive with no clock
// stepping, so going through we/addr/clk.pos() doesn't make sense there.

#include <algorithm>
#include <cstdint>

#include <systemc.h>

#include "config/accelerator_config.h"

namespace dfs {

SC_MODULE(VisitedMemory) {
    sc_in<bool> clk;
    sc_in<bool> clear;

    sc_in<bool> we;
    sc_in<sc_uint<32>> addr;
    sc_in<bool> wdata;   // mark visited
    sc_out<bool> rdata;  // is-visited

    SC_CTOR(VisitedMemory) {
        SC_METHOD(commit);
        sensitive << clk.pos();
        dont_initialize();

        SC_METHOD(preview);
        sensitive << clear << we << addr << wdata;
    }

    // Actual registered write, one cycle after the request.
    void commit() {
        const std::uint32_t a = addr.read();
        if (clear.read()) {
            std::fill(std::begin(visited_), std::end(visited_), false);
        } else if (we.read() && a < config::kMaxCells) {
            visited_[a] = wdata.read();
        }
    }

    // Preview of this cycle's request, see file comment.
    void preview() {
        const std::uint32_t a = addr.read();
        if (clear.read()) {
            rdata.write(false);
        } else if (we.read() && a < config::kMaxCells) {
            rdata.write(wdata.read());
        } else {
            rdata.write(a < config::kMaxCells && visited_[a]);
        }
    }

    // Backdoor, see file comment.
    bool peek(std::uint32_t index) const {
        return index < config::kMaxCells && visited_[index];
    }
    void poke(std::uint32_t index, bool value) {
        if (index < config::kMaxCells) visited_[index] = value;
    }
    void clear_all() { std::fill(std::begin(visited_), std::end(visited_), false); }

  private:
    bool visited_[config::kMaxCells] = {};
};

}  // namespace dfs

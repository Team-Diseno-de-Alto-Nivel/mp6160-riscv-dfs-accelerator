#pragma once
// Visited Memory (HWC-1): per-cell visited state, cleared at run start. Some
// algorithms need more than one instance (e.g. Pacific Atlantic).
//
// Split into commit()/preview() for the same reason as StackManager (see the
// timing note atop stack_manager.h): a purely clocked design would only
// reflect a newly-asserted addr on rdata two cycles later (one hop for addr
// to reach this module's clocked process, one more for the resulting rdata
// write to reach the reader), but DfsController's Pop/Visit split reads
// vis_is_visited unconditionally exactly one cycle after asserting vis_addr
// (see the peer-contract note atop dfs_controller.h) — it needs the T+1
// visibility preview() provides. clear takes priority over a same-cycle we,
// matching how dfs_accelerator.h drives both from the same self-clearing
// Start pulse (a fresh run never intends to also commit a stale write in the
// same cycle).

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

    // Synchronous: the actual, persistent array update, one cycle after
    // clear/we is asserted (a normal registered write).
    void commit() {
        const std::uint32_t a = addr.read();
        if (clear.read()) {
            std::fill(std::begin(visited_), std::end(visited_), false);
        } else if (we.read() && a < config::kMaxCells) {
            visited_[a] = wdata.read();
        }
    }

    // Combinational: previews the effect of *this* cycle's clear/we/addr
    // request before commit() applies it at the next edge — see the file
    // comment.
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

  private:
    bool visited_[config::kMaxCells] = {};
};

}  // namespace dfs

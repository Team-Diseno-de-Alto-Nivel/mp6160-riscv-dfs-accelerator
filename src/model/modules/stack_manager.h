#pragma once
// Stack Manager (HWB-2): LIFO of unexplored nodes; tracks peak depth.
//
// top_x/top_y/empty/full/peak_depth are driven by a *combinational* preview
// of this cycle's push_en/pop_en request, not by the registered storage_/sp_
// alone. Reason: a cross-module signal write only becomes visible to a peer
// one cycle after it is made, so if these outputs only reflected the
// already-committed state, a push at cycle T would only appear as the new
// top at cycle T+2 (T+1 for the push to reach commit(), T+2 for that commit
// to reach the reader). DfsController's Pop state reads top_x/top_y/empty
// unconditionally the cycle right after a push (it does not poll/retry), so
// it needs the T+1 visibility this preview provides — see the peer-contract
// note atop dfs_controller.h. The actual sp_/storage_ mutation still only
// commits synchronously in commit(), one cycle after the request, exactly
// like a normal register.
//
// clear resets sp_/peak_ for a fresh run (wired from the same Start pulse as
// VisitedMemory.clear in dfs_accelerator.h) — without it, peak_depth would
// report the highest depth ever seen since reset instead of this run's peak,
// making run-by-run metrics (docs/guides/metrics-schema.md) meaningless
// after the first run against a given accelerator instance.

#include <algorithm>
#include <cstdint>

#include <systemc.h>

#include "config/accelerator_config.h"
#include "utils/types.h"

namespace dfs {

SC_MODULE(StackManager) {
    sc_in<bool> clk;
    sc_in<bool> rst_n;
    sc_in<bool> clear;
    sc_in<bool> push_en;
    sc_in<bool> pop_en;

    sc_in<sc_uint<32>> in_x;
    sc_in<sc_uint<32>> in_y;
    sc_out<sc_uint<32>> top_x;
    sc_out<sc_uint<32>> top_y;

    sc_out<bool> empty;
    sc_out<bool> full;
    sc_out<sc_uint<32>> peak_depth;

    SC_CTOR(StackManager) {
        SC_METHOD(commit);
        sensitive << clk.pos();
        dont_initialize();

        SC_METHOD(preview);
        sensitive << clear << push_en << pop_en << in_x << in_y << rst_n;
    }

    // Synchronous: the actual, persistent stack pointer/storage update, one
    // cycle after push_en/pop_en is asserted (a normal registered write).
    void commit() {
        if (!rst_n.read() || clear.read()) {
            sp_ = 0;
            peak_ = 0;
            return;
        }
        if (push_en.read()) {
            if (sp_ < config::kStackDepth) {
                storage_[sp_] = CellCoord{static_cast<std::uint32_t>(in_x.read()),
                                       static_cast<std::uint32_t>(in_y.read())};
                ++sp_;
                if (sp_ > peak_) peak_ = sp_;
            }
            // else: silently dropped, see the HWA-2 scope note on overflow.
        } else if (pop_en.read()) {
            if (sp_ > 0) --sp_;
        }
    }

    // Combinational: previews the effect of *this* cycle's clear/push_en/
    // pop_en before commit() applies it at the next edge — see the file
    // comment.
    void preview() {
        if (!rst_n.read() || clear.read()) {
            empty.write(true);
            full.write(false);
            peak_depth.write(0);
            top_x.write(0);
            top_y.write(0);
            return;
        }

        const bool do_push = push_en.read() && sp_ < config::kStackDepth;
        const bool do_pop = !push_en.read() && pop_en.read() && sp_ > 0;
        const std::uint32_t sp = do_push ? sp_ + 1 : (do_pop ? sp_ - 1 : sp_);

        empty.write(sp == 0);
        full.write(sp >= config::kStackDepth);
        peak_depth.write(std::max(peak_, sp));

        if (sp == 0) {
            top_x.write(0);
            top_y.write(0);
        } else if (do_push) {
            top_x.write(in_x.read());
            top_y.write(in_y.read());
        } else {
            top_x.write(storage_[sp - 1].x);
            top_y.write(storage_[sp - 1].y);
        }
    }

  private:
    CellCoord storage_[config::kStackDepth];
    std::uint32_t sp_ = 0;
    std::uint32_t peak_ = 0;
};

}  // namespace dfs

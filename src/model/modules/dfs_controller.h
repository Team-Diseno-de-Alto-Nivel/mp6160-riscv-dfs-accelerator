#pragma once
// DFS Controller (HWA-2): FSM driving the traversal without CPU intervention.
//
// Multi-source scan (real "number of islands", not single-source
// reachability): ScanNext walks the grid row-major and pushes each
// unvisited/passable cell it finds, same as a discovered neighbour would be
// pushed. start_x/start_y are unused as a result — still readable back, just
// ignored by the FSM.
//
// island_count_ only bumps for the *first* cell of a region. No per-entry
// stack tagging needed for that: ScanNext only pushes when the stack is
// empty, so that push is always the very next thing popped. One bool
// (is_new_island_start_) tracks it across that single Pop/Visit hop.
//
// Timing notes for the peer modules (StackManager/NeighborGenerator are
// implemented to this; VisitedMemory isn't yet):
//   - StackManager previews push/pop combinationally so top_x/top_y/empty
//     reflect this cycle's request next cycle, not two cycles later.
//   - NeighborGenerator: valid_in is request/ack, one pulse per candidate
//     (including the first), not a one-shot start. PushNeighbor latches
//     nbr_x/y off of valid_out immediately rather than re-reading a cycle
//     later, since NeighborGenerator would've already moved on by then.
//     ngen_wait_ exists because the cycle GenNeighbors is entered,
//     ngen_valid_out/done still hold the *previous* request's answer for
//     one cycle — it just eats that stale read.
//   - VisitedMemory/GridMemory: addr asserted this cycle, data back next
//     cycle. Grid and visited share the same address wire (vis_addr), so
//     grid_value lines up with vis_is_visited automatically.
//
// Passable = grid_value != 0 (land/water). Only number_of_islands uses this
// path; the other four algorithms go through the primitive path instead.
//
// Every push (ScanNext, PushNeighbor) checks stk_full first and sets
// overflow_ instead of pushing when the stack's already at capacity — the
// run keeps going on whatever made it onto the stack, just under-counts.

#include <cstdint>

#include <systemc.h>

#include "utils/types.h"
namespace dfs {

SC_MODULE(DfsController) {
    sc_in<bool> clk;
    sc_in<bool> rst_n;
    sc_in<bool> start;
    sc_out<bool> busy;
    sc_out<bool> done;

    // start_x/start_y unused, see file comment.
    sc_in<sc_uint<32>> start_x;
    sc_in<sc_uint<32>> start_y;
    sc_in<sc_uint<32>> rows;
    sc_in<sc_uint<32>> cols;  // row-major stride, used to address memories

    // Stack Manager (HWB-2).
    sc_out<bool> stk_push;
    sc_out<bool> stk_pop;
    sc_out<sc_uint<32>> stk_in_x;
    sc_out<sc_uint<32>> stk_in_y;
    sc_in<bool> stk_empty;
    sc_in<bool> stk_full;
    sc_in<sc_uint<32>> stk_top_x;
    sc_in<sc_uint<32>> stk_top_y;

    // Neighbor Generator (HWB-1).
    sc_out<bool> ngen_valid_in;
    sc_out<sc_uint<32>> cur_x;  // node under expansion, latched from the stack pop
    sc_out<sc_uint<32>> cur_y;
    sc_in<bool> ngen_valid_out;
    sc_in<bool> ngen_done;
    sc_in<sc_uint<32>> ngen_x;
    sc_in<sc_uint<32>> ngen_y;

    // Visited Memory (HWC-1).
    sc_out<bool> vis_we;
    sc_out<sc_uint<32>> vis_addr;
    sc_in<bool> vis_is_visited;

    // Grid Memory (HWC-1): read-only content check, addressed via vis_addr.
    sc_in<sc_int<32>> grid_value;

    // Result Interface (HWA-3).
    sc_out<sc_uint<32>> visited_count;  // total cells marked, across all islands
    sc_out<sc_uint<32>> island_count;   // number of distinct islands found
    sc_out<bool> overflow;              // kStatusOverflow, see memory_map.h

    enum class State {
        Idle,
        LoadStart,
        Pop,
        Visit,
        GenNeighbors,
        PushNeighbor,
        ScanNext,
        Done,
    };

    struct TimingStats {
        uint64_t total_cycles = 0;
        uint64_t pop_cycles = 0;
        uint64_t visited_cycles = 0;
        uint64_t mark_cycles = 0;
        uint64_t neighbor_cycles = 0;
        uint64_t push_cycles = 0;
    };

    SC_CTOR(DfsController) {
        SC_METHOD(run_fsm);
        sensitive << clk.pos();
        dont_initialize();
    }
    
    const TimingStats& timing() const {
    return timing_;
    }

    void run_fsm() {
        if (!rst_n.read()) {
            state_ = State::Idle;
            busy.write(false);
            done.write(false);
            stk_push.write(false);
            stk_pop.write(false);
            stk_in_x.write(0);
            stk_in_y.write(0);
            ngen_valid_in.write(false);
            cur_x.write(0);
            cur_y.write(0);
            vis_we.write(false);
            vis_addr.write(0);
            visited_count.write(0);
            island_count.write(0);
            overflow.write(false);
            cur_node_x_ = 0;
            cur_node_y_ = 0;
            nbr_latch_x_ = 0;
            nbr_latch_y_ = 0;
            scan_x_ = 0;
            scan_y_ = 0;
            ngen_wait_ = false;
            is_new_island_start_ = false;
            visited_count_ = 0;
            island_count_ = 0;
            overflow_ = false;
            timing_ = {};
            return;
        }

        // Single-cycle pulses: deasserted unless a branch below re-asserts them.
        stk_push.write(false);
        stk_pop.write(false);
        ngen_valid_in.write(false);
        vis_we.write(false);

        switch (state_) {
            case State::Idle:
                busy.write(false);
                done.write(false);
                if (start.read()) {
                    visited_count_ = 0;
                    island_count_ = 0;
                    overflow_ = false;
                    state_ = State::LoadStart;
                }
                break;

            case State::LoadStart:
                busy.write(true);
                scan_x_ = 0;
                scan_y_ = 0;
                ++timing_.total_cycles;
                state_ = State::ScanNext;
                break;

            case State::ScanNext:
                busy.write(true);
                if (scan_y_ >= rows.read()) {
                    // Swept the whole grid: nothing left to start from.
                    state_ = State::Done;
                    break;
                }
                if (stk_full.read()) {
                    overflow_ = true;
                } else {
                    stk_in_x.write(scan_x_);
                    stk_in_y.write(scan_y_);
                    stk_push.write(true);
                    is_new_island_start_ = true;
                }
                // Advance the pointer for next time, row-major.
                if (scan_x_ + 1 >= cols.read()) {
                    scan_x_ = 0;
                    ++scan_y_;
                } else {
                    ++scan_x_;
                }
                state_ = State::Pop;
                break;

            case State::Pop:
                busy.write(true);
                if (stk_empty.read()) {
                    state_ = State::ScanNext;
                    break;
                }
                cur_node_x_ = stk_top_x.read();
                cur_node_y_ = stk_top_y.read();
                vis_addr.write(cell_index(cur_node_x_, cur_node_y_));
                stk_pop.write(true);
                ++timing_.pop_cycles;
                ++timing_.total_cycles;
                state_ = State::Visit;
                break;

            case State::Visit:
                busy.write(true);
                ++timing_.visit_cycles;
                ++timing_.total_cycles;
                if (vis_is_visited.read() || grid_value.read() == 0) {
                    // already seen or impassable, skip it
                    state_ = State::Pop;
                    break;
                }
                vis_we.write(true);
                ++timing_.mark_cycles;
                ++timing_.total_cycles;
                vis_addr.write(cell_index(cur_node_x_, cur_node_y_));
                ++visited_count_;
                if (is_new_island_start_) {
                    ++island_count_;
                    is_new_island_start_ = false;
                }
                cur_x.write(cur_node_x_);
                cur_y.write(cur_node_y_);
                ngen_valid_in.write(true);
                ngen_wait_ = true;
                state_ = State::GenNeighbors;
                break;

            case State::GenNeighbors:
                busy.write(true);
                if (ngen_wait_) {
                    // ngen_valid_out/done still hold the previous request's
                    // answer for one cycle — ignore it, see file comment.
                    ngen_wait_ = false;
                } else if (ngen_valid_out.read()) {
                    // Latch now: NeighborGenerator only guarantees nbr_x/
                    // nbr_y are valid the cycle valid_out is observed, and
                    // PushNeighbor's own re-request for the next candidate
                    // would otherwise race the read.
                    ++timing_.neighbor_cycles;
                    ++timing_.total_cycles;
                    // latch now, PushNeighbor's re-request would race a
                    // delayed read of nbr_x/y
                    nbr_latch_x_ = ngen_x.read();
                    nbr_latch_y_ = ngen_y.read();
                    state_ = State::PushNeighbor;
                } else if (ngen_done.read()) {
                    state_ = State::Pop;
                }
                break;

            case State::PushNeighbor:
                busy.write(true);
                if (stk_full.read()) {
                    overflow_ = true;  // dropped this neighbour, keep going
                } else {
                    stk_in_x.write(nbr_latch_x_);
                    stk_in_y.write(nbr_latch_y_);
                    ++timing_.push_cycles;
                    ++timing_.total_cycles;
                    stk_push.write(true);
                }
                ngen_valid_in.write(true);  // request the next candidate
                ngen_wait_ = true;
                state_ = State::GenNeighbors;
                break;

            case State::Done:
                busy.write(false);
                if (start.read()) {
                    // clear done right away — Idle is skipped on this path,
                    // so nothing else would ever clear it
                    done.write(false);
                    visited_count_ = 0;
                    island_count_ = 0;
                    overflow_ = false;
                    state_ = State::LoadStart;
                } else {
                    done.write(true);
                }
                break;
        }

        visited_count.write(visited_count_);
        island_count.write(island_count_);
        overflow.write(overflow_);
    }

  private:
    sc_uint<32> cell_index(const sc_uint<32>& x, const sc_uint<32>& y) const {
        return y * cols.read() + x;
    }

    State state_ = State::Idle;
    sc_uint<32> cur_node_x_ = 0;
    sc_uint<32> cur_node_y_ = 0;
    sc_uint<32> nbr_latch_x_ = 0;
    sc_uint<32> nbr_latch_y_ = 0;
    sc_uint<32> scan_x_ = 0;
    sc_uint<32> scan_y_ = 0;
    bool ngen_wait_ = false;
    bool is_new_island_start_ = false;
    std::uint32_t visited_count_ = 0;
    std::uint32_t island_count_ = 0;
    bool overflow_ = false;
    TimingStats timing_;
};

}  // namespace dfs

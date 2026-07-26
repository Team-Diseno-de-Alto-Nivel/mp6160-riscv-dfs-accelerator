#pragma once
// DFS Controller (HWA-2): FSM driving the traversal without CPU intervention.
//
// Multi-source scan: LoadStart resets a row-major scan pointer to (0,0), and
// ScanNext advances it, pushing each candidate cell onto the stack (like a
// discovered neighbour) rather than a single host-given (start_x, start_y).
// Whenever the stack drains (Pop sees it empty), control returns to ScanNext
// to look for the next unvisited, passable cell instead of going straight to
// Done — so the whole grid gets covered, matching real "number of islands"
// semantics (this is the register/whole-traversal path's only real
// consumer: AcceleratorDriver -> src/program/integration/integration_main.cpp
// run_register_path()). start_x/start_y are consequently unused by this FSM
// — kept in the register map and readable back, just never consulted here.
//
// Distinguishing "this push starts a new island" from "this push extends the
// current one" doesn't need any per-entry stack metadata: ScanNext only ever
// pushes when the stack is *empty* (that's the only time it runs), so the
// item it pushes is guaranteed to be the very next one popped, with nothing
// else able to interleave. is_new_island_start_ latches that fact across
// exactly one Pop/Visit round-trip: set when ScanNext pushes, consumed (and
// only then does island_count_ increment) the moment Visit finds that cell
// genuinely unvisited and passable. If Visit instead skips it (already
// visited/impassable), the flag simply stays set for whatever ScanNext tries
// next — Pop is guaranteed empty again immediately afterward, so nothing
// else could have consumed or invalidated it in between.
//
// Peer contracts (StackManager/HWB-2 and NeighborGenerator/HWB-1 are
// implemented against these; VisitedMemory/HWC-1 is not, so that part is
// still an assumption):
//   - StackManager: push_en/pop_en are single-cycle pulses. Because a signal
//     write only becomes visible to a peer module one cycle later, top_x/
//     top_y/empty preview the *result* of this cycle's push_en/pop_en
//     combinationally, so the very next cycle already sees a just-pushed
//     item as the top (see stack_manager.h) instead of needing a second
//     round trip.
//   - NeighborGenerator: valid_in is a request/ack pulse, not just a
//     one-shot start — one pulse (with cur_x/cur_y held steady) is required
//     per neighbour, including the first. It answers with exactly one
//     valid_out pulse (nbr_x/nbr_y valid that cycle) per request, and raises
//     done on a request once the candidates are exhausted. It does *not*
//     auto-advance on its own: without a fresh valid_in pulse it would keep
//     re-driving the same candidate (and a same-cycle cross-module write is
//     never visible to the reader that triggered it), which is why
//     PushNeighbor immediately latches ngen_x/ngen_y off of valid_out and
//     re-pulses valid_in for the next candidate rather than re-reading
//     ngen_x/ngen_y a cycle later — NeighborGenerator would already have
//     moved on by then if it auto-advanced. A subtler consequence: the cycle
//     GenNeighbors is entered, ngen_valid_out/ngen_done still hold whatever
//     was left over from the *previous* request (this one hasn't reached
//     NeighborGenerator yet, let alone been answered) — a done left over
//     from the prior node's exhaustion reads as true here even though this
//     node's scan hasn't started. ngen_wait_ swallows exactly that one stale
//     read after every valid_in pulse (from Visit or PushNeighbor) before
//     trusting either signal.
//   - VisitedMemory: addr asserted one cycle is reflected on rdata/we by the
//     following cycle (at least one clock of latency).
//   - GridMemory: grid_value is read through the *same* address wire as
//     vis_addr (dfs_accelerator.h ties GridMemory.raddr and
//     VisitedMemory.addr to one signal — both memories are indexed
//     identically, cell_index(x,y)), so it becomes valid on the same
//     one-cycle schedule as vis_is_visited: address asserted in Pop,
//     value read in Visit.
//
// A cell counts as passable when grid_value != 0 — matches
// number_of_islands-style land/water grids, the only real consumer of this
// path; the other four algorithms go through the fine-grained primitive
// path instead (see TlmAccelerator), which doesn't touch this datapath.
//
// Before every push (ScanNext, PushNeighbor) the FSM checks stk_full and, if
// the stack is already at capacity, drops that push and latches overflow_
// instead of asserting stk_push — StackManager would silently drop it
// anyway (see stack_manager.h), but checking here means the host gets an
// explicit kStatusOverflow bit (memory_map.h) instead of a silently
// under-counted result. The traversal still runs to completion on whatever
// made it onto the stack; it does not stop or retry the dropped push.

#include <cstdint>

#include <systemc.h>

#include "utils/types.h"
#include "program/harness/instrumentation.h"

namespace dfs {

SC_MODULE(DfsController) {
    sc_in<bool> clk;
    sc_in<bool> rst_n;
    sc_in<bool> start;
    sc_out<bool> busy;
    sc_out<bool> done;

    // Host-configured traversal parameters. start_x/start_y are unused —
    // see the file comment (multi-source scan replaces a single start
    // point) — kept only so the register map still reads back what was
    // written.
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
    // Set if a push was ever dropped this run because the stack was full —
    // see memory_map.h's kStatusOverflow.
    sc_out<bool> overflow;

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

    SC_CTOR(DfsController) {
        SC_METHOD(run_fsm);
        sensitive << clk.pos();
        dont_initialize();
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
                ++timing_.pop_cycles;
                wait(1,SC_NS);
                busy.write(true);
                if (stk_empty.read()) {
                    state_ = State::ScanNext;
                    break;
                }
                cur_node_x_ = stk_top_x.read();
                cur_node_y_ = stk_top_y.read();
                vis_addr.write(cell_index(cur_node_x_, cur_node_y_));
                stk_pop.write(true);
                state_ = State::Visit;
                break;

            case State::Visit:
                busy.write(true);
                ++timing_.visited_cycles;
                wait(2,SC_NS);
                if (vis_is_visited.read() || grid_value.read() == 0) {
                    // Already seen, or an impassable cell (e.g. water): drop
                    // it, try the next one. Leave is_new_island_start_ as-is
                    // — ScanNext's next candidate inherits it.
                    state_ = State::Pop;
                    break;
                }
                vis_we.write(true);
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
                ++timing_.neighbor_cycles;              
                wait(1,SC_NS);

                if (ngen_wait_) {
                    // The valid_in pulse that got us here (from Visit or
                    // PushNeighbor) isn't visible to NeighborGenerator until
                    // next cycle, and its answer isn't visible back here
                    // until the cycle after that — so ngen_valid_out/
                    // ngen_done right now are still whatever was left over
                    // from a *previous*, already-fully-consumed request
                    // (e.g. still `done` from the last node's exhaustion).
                    // Ignore this one read rather than act on stale data.
                    ngen_wait_ = false;
                } else if (ngen_valid_out.read()) {
                    // Latch now: NeighborGenerator only guarantees nbr_x/
                    // nbr_y are valid the cycle valid_out is observed, and
                    // PushNeighbor's own re-request for the next candidate
                    // would otherwise race the read.
                    nbr_latch_x_ = ngen_x.read();
                    nbr_latch_y_ = ngen_y.read();
                    state_ = State::PushNeighbor;
                } else if (ngen_done.read()) {
                    state_ = State::Pop;
                }
                break;

            case State::PushNeighbor:
                ++timing_.push_cycles;
                wait(1,SC_NS);
                busy.write(true);
                if (stk_full.read()) {
                    overflow_ = true;  // dropped this neighbour, keep going
                } else {
                    stk_in_x.write(nbr_latch_x_);
                    stk_in_y.write(nbr_latch_y_);
                    stk_push.write(true);
                }
                ngen_valid_in.write(true);  // request the next candidate
                ngen_wait_ = true;
                state_ = State::GenNeighbors;
                break;

            case State::Done:
                busy.write(false);
                if (start.read()) {
                    // A new run begins: drop Done immediately so the host's
                    // status poll can't mistake this for the previous run's
                    // completion (it would never clear otherwise, since Idle
                    // is skipped on this direct Done -> LoadStart path).
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
    Counters timing_;
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
};

}  // namespace dfs

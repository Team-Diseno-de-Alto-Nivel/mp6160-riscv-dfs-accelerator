#pragma once
// DFS Controller (HWA-2): FSM driving the traversal without CPU intervention —
// pop node -> mark visited -> generate neighbours -> push unvisited -> repeat.
//
// Assumed peer contracts (StackManager/HWB-2, NeighborGenerator/HWB-1,
// VisitedMemory/HWC-1 are not implemented by this issue, so the FSM below is
// written against the following expected handshakes):
//   - StackManager: push_en/pop_en are single-cycle pulses; top_x/top_y show
//     the current top *before* a same-cycle pop takes effect.
//   - NeighborGenerator: a single-cycle valid_in pulse (with cur_x/cur_y held
//     steady) starts generation; it then pulses valid_out once per neighbour
//     (nbr_x/nbr_y valid that cycle) and finally raises done, held until the
//     next valid_in pulse.
//   - VisitedMemory: addr asserted one cycle is reflected on rdata/we by the
//     following cycle (at least one clock of latency).
//
// Known scope limits: this FSM performs a content-agnostic reachability
// traversal — it does not consult GridMemory, so it marks/expands every
// in-bounds neighbour regardless of cell value. Grid-aware filtering needs a
// read datapath from GridMemory into the controller that does not exist yet.
// It also does not consult StackManager.full, so a dense grid can in
// principle push more entries than kStackDepth before HWB-2 adds back-
// pressure.

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

    // Host-configured traversal parameters.
    sc_in<sc_uint<32>> start_x;
    sc_in<sc_uint<32>> start_y;
    sc_in<sc_uint<32>> cols;  // row-major stride, used to address Visited Memory

    // Stack Manager (HWB-2).
    sc_out<bool> stk_push;
    sc_out<bool> stk_pop;
    sc_out<sc_uint<32>> stk_in_x;
    sc_out<sc_uint<32>> stk_in_y;
    sc_in<bool> stk_empty;
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

    // Result Interface (HWA-3): running count of cells marked visited so far.
    sc_out<sc_uint<32>> visited_count;

    enum class State {
        Idle,
        LoadStart,
        Pop,
        Visit,
        GenNeighbors,
        PushNeighbor,
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
            cur_node_x_ = 0;
            cur_node_y_ = 0;
            visited_count_ = 0;
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
                    state_ = State::LoadStart;
                }
                break;

            case State::LoadStart:
                busy.write(true);
                stk_in_x.write(start_x.read());
                stk_in_y.write(start_y.read());
                stk_push.write(true);
                state_ = State::Pop;
                break;

            case State::Pop:
                busy.write(true);
                if (stk_empty.read()) {
                    state_ = State::Done;
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
                if (vis_is_visited.read()) {
                    state_ = State::Pop;  // already seen: drop it, try the next one
                    break;
                }
                vis_we.write(true);
                vis_addr.write(cell_index(cur_node_x_, cur_node_y_));
                ++visited_count_;
                cur_x.write(cur_node_x_);
                cur_y.write(cur_node_y_);
                ngen_valid_in.write(true);
                state_ = State::GenNeighbors;
                break;

            case State::GenNeighbors:
                busy.write(true);
                if (ngen_valid_out.read()) {
                    state_ = State::PushNeighbor;
                } else if (ngen_done.read()) {
                    state_ = State::Pop;
                }
                break;

            case State::PushNeighbor:
                busy.write(true);
                stk_in_x.write(ngen_x.read());
                stk_in_y.write(ngen_y.read());
                stk_push.write(true);
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
                    state_ = State::LoadStart;
                } else {
                    done.write(true);
                }
                break;
        }

        visited_count.write(visited_count_);
    }

  private:
    sc_uint<32> cell_index(const sc_uint<32>& x, const sc_uint<32>& y) const {
        return y * cols.read() + x;
    }

    State state_ = State::Idle;
    sc_uint<32> cur_node_x_ = 0;
    sc_uint<32> cur_node_y_ = 0;
    std::uint32_t visited_count_ = 0;
};

}  // namespace dfs

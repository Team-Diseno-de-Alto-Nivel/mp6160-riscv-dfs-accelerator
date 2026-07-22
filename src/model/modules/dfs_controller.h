#pragma once
// DFS Controller (HWA-2): FSM driving the traversal without CPU intervention —
// pop node -> mark visited -> generate neighbours -> push unvisited -> repeat.

#include <systemc.h>

#include "utils/types.h"

namespace dfs {

SC_MODULE(DfsController) {
    sc_in<bool> clk;
    sc_in<bool> rst_n;
    sc_in<bool> start;
    sc_out<bool> busy;
    sc_out<bool> done;

    // Stack Manager (HWB-2).
    sc_out<bool> stk_push;
    sc_out<bool> stk_pop;
    sc_in<bool> stk_empty;
    sc_in<sc_uint<32>> stk_top_x;
    sc_in<sc_uint<32>> stk_top_y;

    // Neighbor Generator (HWB-1).
    sc_out<bool> ngen_valid_in;
    sc_in<bool> ngen_valid_out;
    sc_in<bool> ngen_done;
    sc_in<sc_uint<32>> ngen_x;
    sc_in<sc_uint<32>> ngen_y;

    // Visited Memory (HWC-1).
    sc_out<bool> vis_we;
    sc_out<sc_uint<32>> vis_addr;
    sc_in<bool> vis_is_visited;

    enum class State {
        Idle,
        LoadStart,
        Pop,
        Visit,
        GenNeighbors,
        PushNeighbor,
        Done,
    };

    SC_CTOR(DfsController) {}  // TODO(HWA-2): register run_fsm()

    void run_fsm();  // TODO(HWA-2)

  private:
    State state_ = State::Idle;
};

}  // namespace dfs

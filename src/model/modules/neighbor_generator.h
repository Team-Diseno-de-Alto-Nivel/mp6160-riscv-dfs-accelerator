#pragma once
// Neighbor Generator (HWB-1): emits the 4/8 neighbours of a cell with boundary
// handling.

#include <systemc.h>

#include "utils/types.h"

namespace dfs {

SC_MODULE(NeighborGenerator) {
    sc_in<bool> clk;
    sc_in<bool> rst_n;
    sc_in<bool> valid_in;
    sc_in<sc_uint<32>> cur_x;
    sc_in<sc_uint<32>> cur_y;
    sc_in<sc_uint<32>> rows;
    sc_in<sc_uint<32>> cols;

    sc_out<bool> valid_out;
    sc_out<bool> done;
    sc_out<sc_uint<32>> nbr_x;
    sc_out<sc_uint<32>> nbr_y;

    SC_CTOR(NeighborGenerator) {}  // TODO(HWB-1): register generate()

    void generate();  // TODO(HWB-1)
};

}  // namespace dfs

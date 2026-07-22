#pragma once
// Stack Manager (HWB-2): LIFO of unexplored nodes; tracks peak depth.

#include <systemc.h>

#include "config/accelerator_config.h"
#include "utils/types.h"

namespace dfs {

SC_MODULE(StackManager) {
    sc_in<bool> clk;
    sc_in<bool> rst_n;
    sc_in<bool> push_en;
    sc_in<bool> pop_en;

    sc_in<sc_uint<32>> in_x;
    sc_in<sc_uint<32>> in_y;
    sc_out<sc_uint<32>> top_x;
    sc_out<sc_uint<32>> top_y;

    sc_out<bool> empty;
    sc_out<bool> full;
    sc_out<sc_uint<32>> peak_depth;

    SC_CTOR(StackManager) {}  // TODO(HWB-2): register update()

    void update();  // TODO(HWB-2)

  private:
    Coord storage_[config::kStackDepth];
    std::uint32_t sp_ = 0;
    std::uint32_t peak_ = 0;
};

}  // namespace dfs

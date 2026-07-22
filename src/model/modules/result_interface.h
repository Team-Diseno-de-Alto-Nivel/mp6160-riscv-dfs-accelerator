#pragma once
// Result Interface (HWA-3): latches the traversal result for the host.

#include <systemc.h>

#include "utils/types.h"

namespace dfs {

SC_MODULE(ResultInterface) {
    sc_in<bool> clk;
    sc_in<bool> rst_n;
    sc_in<bool> done;
    sc_in<sc_uint<32>> result_value;
    sc_in<sc_uint<32>> visited_cells;
    sc_in<sc_uint<32>> peak_stack_depth;

    sc_out<bool> result_valid;

    SC_CTOR(ResultInterface) {}  // TODO(HWA-3): register latch()

    void latch();                 // TODO(HWA-3)
    ResultData snapshot() const;  // TODO(HWA-3)

  private:
    ResultData result_{};
};

}  // namespace dfs

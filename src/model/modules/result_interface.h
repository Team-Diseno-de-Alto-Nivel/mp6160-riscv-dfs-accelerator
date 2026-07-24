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

    SC_CTOR(ResultInterface) {
        SC_METHOD(latch);
        sensitive << clk.pos();
        dont_initialize();
    }

    void latch() {
        if (!rst_n.read()) {
            result_ = ResultData{};
            result_valid.write(false);
            return;
        }

        // While the controller holds Done, its outputs are steady, so it is
        // safe to keep re-latching them every cycle; once a new run starts
        // (done drops), the last-known result is kept for the host to read
        // even though result_valid is deasserted immediately.
        const bool d = done.read();
        if (d) {
            result_.value = result_value.read();
            result_.visited_cells = visited_cells.read();
            result_.peak_stack_depth = peak_stack_depth.read();
        }
        result_.valid = d;
        result_valid.write(d);
    }

    ResultData snapshot() const { return result_; }

  private:
    ResultData result_{};
};

}  // namespace dfs

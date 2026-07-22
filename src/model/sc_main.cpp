// Simulation entry point: instantiates the accelerator top-level.
// Stimulus, binding and sc_start() are left to INT-1.

#include <systemc.h>
#include <iostream>

#include "modules/dfs_accelerator.h"

int sc_main(int argc, char* argv[])
{
    if (argc > 1) {
        std::cout << "Program binary: " << argv[1] << std::endl;
    } else {
        std::cout << "No program binary provided." << std::endl;
    }

    sc_clock clk("clk", 10, SC_NS);
    sc_signal<bool> rst_n;

    dfs::DfsAccelerator accelerator("accelerator");
    accelerator.clk(clk);
    accelerator.rst_n(rst_n);

    // TODO(INT-1): bind a TLM initiator to accelerator.socket, then sc_start().

    return 0;
}

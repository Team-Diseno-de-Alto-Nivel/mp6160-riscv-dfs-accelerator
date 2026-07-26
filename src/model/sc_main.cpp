// Simulation entry point (INT-1): instantiates the accelerator, drives a
// small demonstration traversal over its TLM register/MMIO surface, and
// reports the result.
//
// Standalone smoke test for `make model` / `make run` — the RISC-V software
// (src/program) and the co-simulation cross-check (src/program/integration,
// wired to this same DfsAccelerator via dfs_accelerator_bridge.h) have their
// own entry points.

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>

#include <cstdint>
#include <iostream>

#include "config/accelerator_config.h"
#include "config/memory_map.h"
#include "modules/dfs_accelerator.h"

namespace dfs {

SC_MODULE(Stimulus) {
    tlm_utils::simple_initiator_socket<Stimulus> socket;
    sc_out<bool> rst_n;

    const char* program_binary = nullptr;

    SC_CTOR(Stimulus) : socket("socket") { SC_THREAD(run); }

    void write_reg(std::uint64_t addr, std::uint32_t value) {
        tlm::tlm_generic_payload trans;
        sc_time delay = SC_ZERO_TIME;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        trans.set_data_length(config::kBusWidthBytes);
        socket->b_transport(trans, delay);
        if (trans.get_response_status() != tlm::TLM_OK_RESPONSE) {
            std::cerr << "write to 0x" << std::hex << addr << std::dec << " failed"
                      << std::endl;
        }
    }

    std::uint32_t read_reg(std::uint64_t addr) {
        std::uint32_t value = 0;
        tlm::tlm_generic_payload trans;
        sc_time delay = SC_ZERO_TIME;
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        trans.set_data_length(config::kBusWidthBytes);
        socket->b_transport(trans, delay);
        if (trans.get_response_status() != tlm::TLM_OK_RESPONSE) {
            std::cerr << "read from 0x" << std::hex << addr << std::dec << " failed"
                      << std::endl;
        }
        return value;
    }

    void run() {
        if (program_binary != nullptr) {
            std::cout << "Program binary: " << program_binary << std::endl;
        } else {
            std::cout << "No program binary provided." << std::endl;
        }

        rst_n.write(false);
        wait(20, SC_NS);
        rst_n.write(true);
        wait(10, SC_NS);

        // Small demonstration grid: a 3x3 block, all open, 4-connectivity,
        // starting at the top-left corner.
        const std::uint32_t rows = 3;
        const std::uint32_t cols = 3;
        write_reg(memmap::kRegRows, rows);
        write_reg(memmap::kRegCols, cols);
        write_reg(memmap::kRegStartX, 0);
        write_reg(memmap::kRegStartY, 0);
        write_reg(memmap::kRegParams, 4);
        for (std::uint32_t i = 0; i < rows * cols; ++i) {
            write_reg(memmap::kGridBase + i * config::kBusWidthBytes, 1);
        }

        write_reg(memmap::kRegControl, memmap::kControlEnable | memmap::kControlStart);

        bool done = false;
        for (int i = 0; i < 2000 && !done; ++i) {
            wait(10, SC_NS);
            done = (read_reg(memmap::kRegStatus) & memmap::kStatusDone) != 0;
        }

        if (done) {
            std::cout << "Traversal done: result=" << read_reg(memmap::kRegResult)
                      << " visited=" << read_reg(memmap::kRegVisited)
                      << " peak_stack=" << read_reg(memmap::kRegPeakStack) << std::endl;
        } else {
            std::cout << "Traversal did not complete in time" << std::endl;
        }

        sc_stop();
    }
};

}  // namespace dfs

int sc_main(int argc, char* argv[]) {
    sc_clock clk("clk", 10, SC_NS);
    sc_signal<bool> rst_n;

    dfs::DfsAccelerator accelerator("accelerator");
    accelerator.clk(clk);
    accelerator.rst_n(rst_n);

    dfs::Stimulus stimulus("stimulus");
    stimulus.rst_n(rst_n);
    stimulus.socket.bind(accelerator.socket);
    stimulus.program_binary = (argc > 1) ? argv[1] : nullptr;

    sc_start();

    // Print hardware timing values collected by the accelerator's controller.
    const auto& stats = accelerator.timing();
    std::cout << "Hardware timing (cycles): total=" << stats.total_cycles
              << " pop=" << stats.pop_cycles
              << " visited=" << stats.visited_cycles
              << " mark=" << stats.mark_cycles
              << " neighbor=" << stats.neighbor_cycles
              << " push=" << stats.push_cycles << std::endl;

    // Approximate latency in nanoseconds using the simulation clock period (10 ns).
    std::uint64_t latency_ns = stats.total_cycles * 10ULL;
    std::cout << "Approx latency: " << latency_ns << " ns" << std::endl;

    return 0;
}

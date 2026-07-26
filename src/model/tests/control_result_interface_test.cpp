// Unit testbench for the Control-register decode (memory map, ON/OFF,
// Start self-clear) and Result Interface (kRegResult/Visited/PeakStack,
// kRegStatus) paths of DfsAccelerator — part 1/3 of the hardware unit
// testbenches tracked in #55. Standalone SystemC/TLM test: drives
// DfsAccelerator's register socket directly, no RISC-V toolchain needed.

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>

#include <cstdint>
#include <cstdio>

#include "config/accelerator_config.h"
#include "config/memory_map.h"
#include "modules/dfs_accelerator.h"

namespace dfs {
namespace {

int failures = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

SC_MODULE(Stimulus) {
    tlm_utils::simple_initiator_socket<Stimulus> socket;
    sc_out<bool> rst_n;

    SC_CTOR(Stimulus) : socket("socket") { SC_THREAD(run); }

    tlm::tlm_response_status write_reg(std::uint64_t addr, std::uint32_t value) {
        tlm::tlm_generic_payload trans;
        sc_time delay = SC_ZERO_TIME;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        trans.set_data_length(config::kBusWidthBytes);
        socket->b_transport(trans, delay);
        return trans.get_response_status();
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
        check(trans.get_response_status() == tlm::TLM_OK_RESPONSE, "read_reg TLM_OK_RESPONSE");
        return value;
    }

    void run() {
        rst_n.write(false);
        wait(20, SC_NS);
        rst_n.write(true);
        wait(10, SC_NS);

        // --- Memory map: config registers round-trip ---
        check(write_reg(memmap::kRegRows, 7) == tlm::TLM_OK_RESPONSE, "write rows ok");
        write_reg(memmap::kRegCols, 5);
        write_reg(memmap::kRegStartX, 2);
        write_reg(memmap::kRegStartY, 3);
        write_reg(memmap::kRegParams, 8);
        // rows_sig_/cols_sig_/etc. are sc_signals: a write() only becomes
        // visible to read() after the current delta cycle's update phase.
        wait(SC_ZERO_TIME);
        check(read_reg(memmap::kRegRows) == 7, "rows round-trip");
        check(read_reg(memmap::kRegCols) == 5, "cols round-trip");
        check(read_reg(memmap::kRegStartX) == 2, "start_x round-trip");
        check(read_reg(memmap::kRegStartY) == 3, "start_y round-trip");
        check(read_reg(memmap::kRegParams) == 8, "params round-trip");

        // --- Memory map: unmapped address rejected ---
        check(write_reg(0x0800, 0xDEAD) == tlm::TLM_ADDRESS_ERROR_RESPONSE,
              "unmapped register write rejected");

        // --- Control register: ON/OFF toggle (Enable bit, no Start) ---
        write_reg(memmap::kRegControl, memmap::kControlEnable);
        std::uint32_t ctrl = read_reg(memmap::kRegControl);
        check((ctrl & memmap::kControlEnable) != 0, "enable bit set");
        check((ctrl & memmap::kControlStart) == 0, "start bit clear when not requested");

        write_reg(memmap::kRegControl, 0);
        ctrl = read_reg(memmap::kRegControl);
        check((ctrl & memmap::kControlEnable) == 0, "enable bit cleared");

        // --- Result Interface: run a known traversal end to end ---
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

        // Start self-clears shortly after being requested (see the
        // kRegControl comment in dfs_accelerator.h's read_reg()). It's
        // consumed by drive_start_pulse() on clk.pos(), so poll a couple of
        // cycles instead of assuming a fixed delay — how many wait(10ns)
        // calls it takes depends on how our current sim time happens to
        // land relative to the clock's phase, not on the design itself.
        ctrl = read_reg(memmap::kRegControl);
        check((ctrl & memmap::kControlStart) != 0,
              "start bit still set the same cycle it was requested");
        check((read_reg(memmap::kRegStatus) & memmap::kStatusDone) == 0,
              "status not done right after start");

        bool start_cleared = false;
        for (int i = 0; i < 5 && !start_cleared; ++i) {
            wait(10, SC_NS);
            ctrl = read_reg(memmap::kRegControl);
            start_cleared = (ctrl & memmap::kControlStart) == 0;
        }
        check(start_cleared, "start bit self-clears within a few cycles");

        bool done = false;
        for (int i = 0; i < 2000 && !done; ++i) {
            wait(10, SC_NS);
            done = (read_reg(memmap::kRegStatus) & memmap::kStatusDone) != 0;
        }
        check(done, "traversal completes within timeout");
        check(read_reg(memmap::kRegResult) == 1, "result register");
        check(read_reg(memmap::kRegVisited) == 9, "visited register");
        check(read_reg(memmap::kRegPeakStack) == 13, "peak stack register");
        check((read_reg(memmap::kRegStatus) & memmap::kStatusOverflow) == 0,
              "no overflow reported");

        sc_stop();
    }
};

}  // namespace
}  // namespace dfs

int sc_main(int, char*[]) {
    sc_clock clk("clk", 10, SC_NS);
    sc_signal<bool> rst_n;

    dfs::DfsAccelerator accelerator("accelerator");
    accelerator.clk(clk);
    accelerator.rst_n(rst_n);

    dfs::Stimulus stimulus("stimulus");
    stimulus.rst_n(rst_n);
    stimulus.socket.bind(accelerator.socket);

    sc_start();

    if (dfs::failures == 0) {
        std::printf("control_result_interface_test: all checks passed\n");
    }
    return dfs::failures == 0 ? 0 : 1;
}

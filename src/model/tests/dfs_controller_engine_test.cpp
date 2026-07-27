// Unit testbench for the DFS Controller's traversal sequencing (state
// machine) wired to the real Processing Engine — part 2/3 of the hardware
// unit testbenches tracked in #55. Bypasses DfsAccelerator's TLM/register
// decode (already covered by control_result_interface_test.cpp) and wires
// DfsController directly to DfsProcessingEngine's sub-modules, the same way
// dfs_accelerator.h itself does.
//
// Cases mirror src/program/cases/datasets.cpp's number_of_islands_cases()
// (same grids/connectivity, same expected island counts) plus the expected
// visited-cell counts worked out by hand from those same grids, since
// datasets.cpp only records the algorithm's final result, not visited_cells.

#include <systemc.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "modules/dfs_controller.h"
#include "modules/dfs_processing_engine.h"
#include "utils/types.h"

namespace dfs {
namespace {

int failures = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

struct GridCase {
    const char* name;
    std::uint32_t rows;
    std::uint32_t cols;
    std::vector<CellValue> cells;  // row-major
    std::uint32_t connectivity;
    std::uint32_t expected_islands;
    std::uint32_t expected_visited;
};

SC_MODULE(Harness) {
    sc_in<bool> clk;
    sc_out<bool> rst_n;
    sc_out<bool> start;
    sc_out<sc_uint<32>> rows;
    sc_out<sc_uint<32>> cols;
    sc_out<sc_uint<32>> start_x;
    sc_out<sc_uint<32>> start_y;
    sc_out<sc_uint<32>> connectivity;
    sc_in<bool> busy;
    sc_in<bool> done;
    sc_in<sc_uint<32>> visited_count;
    sc_in<sc_uint<32>> island_count;
    sc_in<bool> overflow;

    DfsProcessingEngine& engine;

    SC_HAS_PROCESS(Harness);
    Harness(sc_module_name name, DfsProcessingEngine& eng) : engine(eng) { SC_THREAD(run); }

    void load_case(const GridCase& gc) {
        for (std::size_t i = 0; i < gc.cells.size(); ++i) {
            engine.grid_mem.load_cell(static_cast<std::uint32_t>(i), gc.cells[i]);
        }
        rows.write(gc.rows);
        cols.write(gc.cols);
        start_x.write(0);
        start_y.write(0);
        connectivity.write(gc.connectivity);
    }

    void run_case(const GridCase& gc) {
        load_case(gc);

        // One-cycle start pulse (also clears Stack/Visited Memory — both are
        // wired to this same signal, exactly like start_pulse_sig_ in
        // dfs_accelerator.h). Held across a full period so it's sampled by
        // the controller regardless of clock phase.
        start.write(true);
        wait(10, SC_NS);
        start.write(false);

        const std::string prefix = std::string(gc.name) + ": ";
        bool d = false;
        for (int i = 0; i < 5000 && !d; ++i) {
            wait(10, SC_NS);
            d = done.read();
        }
        check(d, (prefix + "traversal completes within timeout").c_str());
        if (!d) return;

        check(island_count.read() == gc.expected_islands, (prefix + "island_count").c_str());
        check(visited_count.read() == gc.expected_visited, (prefix + "visited_count").c_str());
        check(!overflow.read(), (prefix + "no overflow").c_str());
        check(!busy.read(), (prefix + "busy clears once done").c_str());
    }

    void run() {
        rst_n.write(false);
        start.write(false);
        wait(20, SC_NS);
        rst_n.write(true);
        wait(10, SC_NS);

        // Same grids/connectivity as number_of_islands_cases() in
        // src/program/cases/datasets.cpp — island counts match what's
        // already validated there; visited counts worked out by hand.
        run_case({"noi_classic_4c", 4, 5,
                  {1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1},
                  4, 3, 7});
        run_case({"noi_classic_8c", 4, 5,
                  {1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1},
                  8, 1, 7});
        run_case({"noi_checker_4c", 4, 4,
                  {1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1},
                  4, 8, 8});
        run_case({"noi_checker_8c", 4, 4,
                  {1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1},
                  8, 1, 8});
        run_case({"noi_all_water", 2, 2, {0, 0, 0, 0}, 4, 0, 0});
        run_case({"noi_all_land_4c", 3, 3, {1, 1, 1, 1, 1, 1, 1, 1, 1}, 4, 1, 9});

        sc_stop();
    }
};

}  // namespace
}  // namespace dfs

int sc_main(int, char*[]) {
    sc_clock clk("clk", 10, SC_NS);
    sc_signal<bool> rst_n;
    sc_signal<bool> start_sig;
    sc_signal<sc_uint<32>> start_x_sig, start_y_sig, rows_sig, cols_sig, params_sig;
    sc_signal<bool> busy_sig, done_sig, overflow_sig;
    sc_signal<sc_uint<32>> visited_count_sig, island_count_sig;

    sc_signal<bool> stk_push_sig, stk_pop_sig, stk_empty_sig, stk_full_sig;
    sc_signal<sc_uint<32>> stk_in_x_sig, stk_in_y_sig, stk_top_x_sig, stk_top_y_sig,
        stk_peak_depth_sig;

    sc_signal<bool> ngen_valid_in_sig, ngen_valid_out_sig, ngen_done_sig;
    sc_signal<sc_uint<32>> ngen_x_sig, ngen_y_sig, ngen_cur_x_sig, ngen_cur_y_sig;

    sc_signal<bool> vis_we_sig, vis_is_visited_sig, vis_mark_sig;
    sc_signal<sc_uint<32>> vis_addr_sig;

    // Grid write path: unused here (grid content is loaded via the backdoor
    // load_cell(), same as DfsAccelerator's own LoadGrid path), just needs
    // to stay bound and deasserted.
    sc_signal<bool> grid_we_sig;
    sc_signal<sc_uint<32>> grid_waddr_sig;
    sc_signal<sc_int<32>> grid_wdata_sig, grid_rdata_sig;

    dfs::DfsController controller("controller");
    dfs::DfsProcessingEngine engine("engine");
    dfs::Harness harness("harness", engine);

    controller.clk(clk);
    controller.rst_n(rst_n);
    engine.clk(clk);
    engine.rst_n(rst_n);

    harness.clk(clk);
    harness.rst_n(rst_n);
    harness.start(start_sig);
    harness.rows(rows_sig);
    harness.cols(cols_sig);
    harness.start_x(start_x_sig);
    harness.start_y(start_y_sig);
    harness.connectivity(params_sig);
    harness.busy(busy_sig);
    harness.done(done_sig);
    harness.visited_count(visited_count_sig);
    harness.island_count(island_count_sig);
    harness.overflow(overflow_sig);

    controller.start(start_sig);
    controller.busy(busy_sig);
    controller.done(done_sig);
    controller.start_x(start_x_sig);
    controller.start_y(start_y_sig);
    controller.rows(rows_sig);
    controller.cols(cols_sig);
    controller.visited_count(visited_count_sig);
    controller.island_count(island_count_sig);
    controller.overflow(overflow_sig);

    controller.stk_push(stk_push_sig);
    controller.stk_pop(stk_pop_sig);
    controller.stk_in_x(stk_in_x_sig);
    controller.stk_in_y(stk_in_y_sig);
    controller.stk_empty(stk_empty_sig);
    controller.stk_full(stk_full_sig);
    controller.stk_top_x(stk_top_x_sig);
    controller.stk_top_y(stk_top_y_sig);
    engine.stack_mgr.push_en(stk_push_sig);
    engine.stack_mgr.pop_en(stk_pop_sig);
    engine.stack_mgr.in_x(stk_in_x_sig);
    engine.stack_mgr.in_y(stk_in_y_sig);
    engine.stack_mgr.top_x(stk_top_x_sig);
    engine.stack_mgr.top_y(stk_top_y_sig);
    engine.stack_mgr.empty(stk_empty_sig);
    engine.stack_mgr.full(stk_full_sig);
    engine.stack_mgr.peak_depth(stk_peak_depth_sig);
    engine.stack_mgr.clear(start_sig);

    controller.ngen_valid_in(ngen_valid_in_sig);
    controller.cur_x(ngen_cur_x_sig);
    controller.cur_y(ngen_cur_y_sig);
    controller.ngen_valid_out(ngen_valid_out_sig);
    controller.ngen_done(ngen_done_sig);
    controller.ngen_x(ngen_x_sig);
    controller.ngen_y(ngen_y_sig);
    engine.neighbor_gen.valid_in(ngen_valid_in_sig);
    engine.neighbor_gen.cur_x(ngen_cur_x_sig);
    engine.neighbor_gen.cur_y(ngen_cur_y_sig);
    engine.neighbor_gen.rows(rows_sig);
    engine.neighbor_gen.cols(cols_sig);
    engine.neighbor_gen.connectivity(params_sig);
    engine.neighbor_gen.valid_out(ngen_valid_out_sig);
    engine.neighbor_gen.done(ngen_done_sig);
    engine.neighbor_gen.nbr_x(ngen_x_sig);
    engine.neighbor_gen.nbr_y(ngen_y_sig);

    controller.vis_we(vis_we_sig);
    controller.vis_addr(vis_addr_sig);
    controller.vis_is_visited(vis_is_visited_sig);
    engine.visited_mem.we(vis_we_sig);
    engine.visited_mem.addr(vis_addr_sig);
    engine.visited_mem.rdata(vis_is_visited_sig);
    engine.visited_mem.wdata(vis_mark_sig);
    engine.visited_mem.clear(start_sig);

    controller.grid_value(grid_rdata_sig);
    engine.grid_mem.we(grid_we_sig);
    engine.grid_mem.waddr(grid_waddr_sig);
    engine.grid_mem.wdata(grid_wdata_sig);
    engine.grid_mem.raddr(vis_addr_sig);
    engine.grid_mem.rdata(grid_rdata_sig);

    vis_mark_sig.write(true);
    grid_we_sig.write(false);

    sc_start();

    if (dfs::failures == 0) {
        std::printf("dfs_controller_engine_test: all checks passed\n");
    }
    return dfs::failures == 0 ? 0 : 1;
}

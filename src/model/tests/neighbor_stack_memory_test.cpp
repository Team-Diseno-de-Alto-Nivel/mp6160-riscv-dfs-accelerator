// Unit testbench for Neighbor Generator, Stack Manager, and the Grid/Visited
// memories in isolation — part 3/3 of the hardware unit testbenches tracked
// in #55. Each module is instantiated standalone (no DfsController/engine
// wiring) and driven directly, focused on behavior documented in each
// module's own file header: NeighborGenerator's per-candidate request/ack
// and connectivity latching, StackManager's combinational same-cycle preview
// and peak-depth tracking (plus real capacity/overflow behavior), and the
// Grid/Visited memories' preview/commit split and backdoor accessors.

#include <systemc.h>

#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>

#include "config/accelerator_config.h"
#include "modules/grid_memory.h"
#include "modules/neighbor_generator.h"
#include "modules/stack_manager.h"
#include "modules/visited_memory.h"

namespace dfs {
namespace {

int failures = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

SC_MODULE(Harness) {
    sc_in<bool> clk;
    sc_out<bool> rst_n;

    // Neighbor Generator
    sc_out<bool> ngen_valid_in;
    sc_out<sc_uint<32>> ngen_cur_x, ngen_cur_y, ngen_rows, ngen_cols, ngen_conn;
    sc_in<bool> ngen_valid_out, ngen_done;
    sc_in<sc_uint<32>> ngen_nbr_x, ngen_nbr_y;

    // Stack Manager
    sc_out<bool> stk_clear, stk_push, stk_pop;
    sc_out<sc_uint<32>> stk_in_x, stk_in_y;
    sc_in<bool> stk_empty, stk_full;
    sc_in<sc_uint<32>> stk_top_x, stk_top_y, stk_peak;

    // Grid Memory
    sc_out<bool> grid_we;
    sc_out<sc_uint<32>> grid_waddr, grid_raddr;
    sc_out<sc_int<32>> grid_wdata;
    sc_in<sc_int<32>> grid_rdata;
    GridMemory& grid_mem;

    // Visited Memory
    sc_out<bool> vis_clear, vis_we;
    sc_out<sc_uint<32>> vis_addr;
    sc_out<bool> vis_wdata;
    sc_in<bool> vis_rdata;
    VisitedMemory& visited_mem;

    SC_HAS_PROCESS(Harness);
    Harness(sc_module_name name, GridMemory& gm, VisitedMemory& vm)
        : grid_mem(gm), visited_mem(vm) {
        SC_THREAD(run);
    }

    // Two zero-time waits: one delta for a combinational preview() process
    // to react to a port change we just wrote, one more for *its* resulting
    // write to become visible to a subsequent read() — a single delta only
    // covers the first hop.
    void settle() {
        wait(SC_ZERO_TIME);
        wait(SC_ZERO_TIME);
    }

    void run() {
        rst_n.write(false);
        ngen_valid_in.write(false);
        stk_clear.write(false);
        stk_push.write(false);
        stk_pop.write(false);
        grid_we.write(false);
        vis_clear.write(false);
        vis_we.write(false);
        vis_wdata.write(true);
        wait(20, SC_NS);
        rst_n.write(true);
        wait(10, SC_NS);

        test_neighbor_generator();
        test_stack_manager();
        test_stack_overflow();
        test_grid_memory();
        test_visited_memory();

        sc_stop();
    }

    // --- Neighbor Generator -------------------------------------------------
    struct NgenResult {
        std::vector<std::pair<std::uint32_t, std::uint32_t>> coords;
        bool done_seen = false;
    };

    NgenResult run_neighbor_scan(std::uint32_t cx, std::uint32_t cy, std::uint32_t rows,
                                  std::uint32_t cols, std::uint32_t connectivity) {
        ngen_cur_x.write(cx);
        ngen_cur_y.write(cy);
        ngen_rows.write(rows);
        ngen_cols.write(cols);
        ngen_conn.write(connectivity);

        NgenResult r;
        for (int i = 0; i < 16 && !r.done_seen; ++i) {
            ngen_valid_in.write(true);
            wait(10, SC_NS);
            ngen_valid_in.write(false);
            if (ngen_done.read()) {
                r.done_seen = true;
            } else if (ngen_valid_out.read()) {
                r.coords.push_back({static_cast<std::uint32_t>(ngen_nbr_x.read()),
                                     static_cast<std::uint32_t>(ngen_nbr_y.read())});
            }
        }
        return r;
    }

    void test_neighbor_generator() {
        // Interior cell, 4-connectivity: all 4 orthogonal neighbours valid.
        auto r = run_neighbor_scan(2, 2, 5, 5, 4);
        check(r.done_seen, "ngen: interior 4c reaches done");
        check(r.coords.size() == 4, "ngen: interior 4c yields 4 candidates");
        if (r.coords.size() == 4) {
            check(r.coords[0] == std::make_pair(1u, 2u), "ngen: interior 4c dir0 (x-1,y)");
            check(r.coords[1] == std::make_pair(3u, 2u), "ngen: interior 4c dir1 (x+1,y)");
            check(r.coords[2] == std::make_pair(2u, 1u), "ngen: interior 4c dir2 (x,y-1)");
            check(r.coords[3] == std::make_pair(2u, 3u), "ngen: interior 4c dir3 (x,y+1)");
        }

        // Corner cell (0,0), 4-connectivity: only 2 neighbours are in bounds.
        r = run_neighbor_scan(0, 0, 5, 5, 4);
        check(r.done_seen, "ngen: corner 4c reaches done");
        check(r.coords.size() == 2, "ngen: corner 4c yields 2 candidates (out-of-bounds skipped)");
        if (r.coords.size() == 2) {
            check(r.coords[0] == std::make_pair(1u, 0u), "ngen: corner 4c dir1 (x+1,y)");
            check(r.coords[1] == std::make_pair(0u, 1u), "ngen: corner 4c dir3 (x,y+1)");
        }

        // Interior cell, 8-connectivity: all 8 neighbours (orthogonal + diagonal) valid.
        r = run_neighbor_scan(2, 2, 5, 5, 8);
        check(r.done_seen, "ngen: interior 8c reaches done");
        check(r.coords.size() == 8, "ngen: interior 8c yields 8 candidates");
    }

    // --- Stack Manager -------------------------------------------------------
    // Held for two full clock periods (not one) before deasserting: a single
    // period can land exactly on the boundary of commit()'s clk.pos() edge
    // depending on accumulated phase, silently missing it (same class of
    // issue as the Start self-clear timing in control_result_interface_test.cpp).
    void stk_do_push(std::uint32_t x, std::uint32_t y) {
        stk_in_x.write(x);
        stk_in_y.write(y);
        stk_push.write(true);
        wait(20, SC_NS);
        stk_push.write(false);
        settle();
    }

    void stk_do_pop() {
        stk_pop.write(true);
        wait(20, SC_NS);
        stk_pop.write(false);
        settle();
    }

    void stk_do_clear() {
        stk_clear.write(true);
        wait(20, SC_NS);
        stk_clear.write(false);
        settle();
    }

    std::uint32_t stk_peak_read() { return static_cast<std::uint32_t>(stk_peak.read()); }

    void test_stack_manager() {
        stk_do_clear();
        check(stk_empty.read(), "stack: empty after clear");

        // Combinational same-cycle preview: top/empty reflect the push
        // *before* the actual commit one cycle later (see stack_manager.h).
        stk_in_x.write(5);
        stk_in_y.write(7);
        stk_push.write(true);
        settle();
        check(!stk_empty.read(), "stack: preview shows non-empty same cycle as push");
        check(stk_top_x.read() == 5 && stk_top_y.read() == 7,
              "stack: preview shows pushed coord same cycle as push");
        wait(20, SC_NS);
        stk_push.write(false);
        settle();

        stk_do_push(10, 20);
        stk_do_push(30, 40);
        check(stk_top_x.read() == 30 && stk_top_y.read() == 40, "stack: LIFO top after 3 pushes");
        check(stk_peak_read() == 3, "stack: peak_depth reaches 3");

        stk_do_pop();
        check(stk_top_x.read() == 10 && stk_top_y.read() == 20, "stack: LIFO order after 1 pop");
        stk_do_pop();
        check(stk_top_x.read() == 5 && stk_top_y.read() == 7, "stack: LIFO order after 2 pops");
        check(stk_peak_read() == 3, "stack: peak_depth persists after pops");
        stk_do_pop();
        check(stk_empty.read(), "stack: empty after popping everything");
        check(stk_peak_read() == 3, "stack: peak_depth still remembers max after emptying");

        stk_do_clear();
        check(stk_peak_read() == 0, "stack: clear resets peak_depth");
    }

    void test_stack_overflow() {
        stk_do_clear();

        for (std::uint32_t i = 0; i < config::kStackDepth; ++i) {
            stk_do_push(i, i);
        }
        check(stk_full.read(), "stack: full once at capacity");
        check(stk_peak_read() == config::kStackDepth, "stack: peak_depth caps at capacity");

        // One push past capacity: silently dropped (see the commit() comment
        // in stack_manager.h) — top and peak stay exactly as they were.
        const std::uint32_t last_x = static_cast<std::uint32_t>(stk_top_x.read());
        const std::uint32_t last_y = static_cast<std::uint32_t>(stk_top_y.read());
        stk_do_push(9999, 9999);
        check(stk_top_x.read() == last_x && stk_top_y.read() == last_y,
              "stack: push past capacity is silently dropped");
        check(stk_peak_read() == config::kStackDepth,
              "stack: peak_depth unchanged by dropped push");

        stk_do_clear();
    }

    // --- Grid Memory ----------------------------------------------------------
    void test_grid_memory() {
        // Backdoor load_cell(): immediate, no clock needed.
        grid_mem.load_cell(0, 7);
        grid_mem.load_cell(1, -3);
        // Check address 1 first: raddr defaults to 0, so writing 0 again
        // right away wouldn't be a value *change* and wouldn't re-trigger
        // preview() at all — checking 1 before 0 keeps every raddr write a
        // genuine change.
        grid_raddr.write(1);
        settle();
        check(grid_rdata.read() == -3, "grid: load_cell(1) readable via raddr, negative value");
        grid_raddr.write(0);
        settle();
        check(grid_rdata.read() == 7, "grid: load_cell(0) readable via raddr");

        // Bounds guard: out-of-range index is a silent no-op. No raddr
        // rewrite needed — it's already sitting on address 0.
        grid_mem.load_cell(config::kMaxCells, 999);
        check(grid_rdata.read() == 7, "grid: load_cell out-of-bounds index is a no-op");

        // Write/read forwarding: same-cycle write+read of the same address
        // sees the new data before it commits (see preview()).
        grid_waddr.write(2);
        grid_wdata.write(42);
        grid_we.write(true);
        grid_raddr.write(2);
        settle();
        check(grid_rdata.read() == 42, "grid: write/read forwarding same cycle");

        wait(20, SC_NS);  // commit (see the phase-margin note on stk_do_push)
        grid_we.write(false);
        settle();
        check(grid_rdata.read() == 42, "grid: committed write persists after we deasserted");
    }

    // --- Visited Memory ---------------------------------------------------------
    void test_visited_memory() {
        // Backdoor path (peek/poke/clear_all): independent of the we/addr/
        // rdata port path below — rdata only reacts to *its own* port
        // sensitivity list, not to backdoor mutations, so these two access
        // modes are checked separately rather than cross-referenced.
        visited_mem.clear_all();
        check(!visited_mem.peek(3), "visited: clear_all leaves cell unvisited (backdoor)");
        visited_mem.poke(3, true);
        check(visited_mem.peek(3), "visited: poke backdoor reflects immediately (backdoor)");
        visited_mem.poke(config::kMaxCells, true);
        check(!visited_mem.peek(config::kMaxCells), "visited: poke/peek out-of-bounds is a no-op");

        // Port path (we/addr/wdata -> rdata).
        vis_addr.write(9);
        settle();
        check(!vis_rdata.read(), "visited: unmarked cell reads not-visited via port");

        vis_we.write(true);
        vis_wdata.write(true);
        settle();
        check(vis_rdata.read(), "visited: write/read forwarding same cycle (port)");

        wait(20, SC_NS);  // commit (see the phase-margin note on stk_do_push)
        vis_we.write(false);
        settle();
        check(vis_rdata.read(), "visited: committed mark persists after we deasserted (port)");

        // clear: combinational false immediately, and actually wipes the
        // underlying array — visible through both access paths afterward.
        vis_clear.write(true);
        settle();
        check(!vis_rdata.read(), "visited: clear forces rdata false same cycle (port)");
        wait(20, SC_NS);  // commit the clear
        vis_clear.write(false);
        settle();
        check(!visited_mem.peek(9), "visited: clear actually wipes previously marked cell (backdoor)");
    }
};

}  // namespace
}  // namespace dfs

int sc_main(int, char*[]) {
    sc_clock clk("clk", 10, SC_NS);
    sc_signal<bool> rst_n;

    sc_signal<bool> ngen_valid_in_sig, ngen_valid_out_sig, ngen_done_sig;
    sc_signal<sc_uint<32>> ngen_cur_x_sig, ngen_cur_y_sig, ngen_rows_sig, ngen_cols_sig,
        ngen_conn_sig, ngen_nbr_x_sig, ngen_nbr_y_sig;

    sc_signal<bool> stk_clear_sig, stk_push_sig, stk_pop_sig, stk_empty_sig, stk_full_sig;
    sc_signal<sc_uint<32>> stk_in_x_sig, stk_in_y_sig, stk_top_x_sig, stk_top_y_sig, stk_peak_sig;

    sc_signal<bool> grid_we_sig;
    sc_signal<sc_uint<32>> grid_waddr_sig, grid_raddr_sig;
    sc_signal<sc_int<32>> grid_wdata_sig, grid_rdata_sig;

    sc_signal<bool> vis_clear_sig, vis_we_sig, vis_wdata_sig, vis_rdata_sig;
    sc_signal<sc_uint<32>> vis_addr_sig;

    dfs::NeighborGenerator ngen("ngen");
    dfs::StackManager smgr("smgr");
    dfs::GridMemory gmem("gmem");
    dfs::VisitedMemory vmem("vmem");
    dfs::Harness harness("harness", gmem, vmem);

    harness.clk(clk);
    harness.rst_n(rst_n);

    ngen.clk(clk);
    ngen.rst_n(rst_n);
    harness.ngen_valid_in(ngen_valid_in_sig);
    harness.ngen_cur_x(ngen_cur_x_sig);
    harness.ngen_cur_y(ngen_cur_y_sig);
    harness.ngen_rows(ngen_rows_sig);
    harness.ngen_cols(ngen_cols_sig);
    harness.ngen_conn(ngen_conn_sig);
    harness.ngen_valid_out(ngen_valid_out_sig);
    harness.ngen_done(ngen_done_sig);
    harness.ngen_nbr_x(ngen_nbr_x_sig);
    harness.ngen_nbr_y(ngen_nbr_y_sig);
    ngen.valid_in(ngen_valid_in_sig);
    ngen.cur_x(ngen_cur_x_sig);
    ngen.cur_y(ngen_cur_y_sig);
    ngen.rows(ngen_rows_sig);
    ngen.cols(ngen_cols_sig);
    ngen.connectivity(ngen_conn_sig);
    ngen.valid_out(ngen_valid_out_sig);
    ngen.done(ngen_done_sig);
    ngen.nbr_x(ngen_nbr_x_sig);
    ngen.nbr_y(ngen_nbr_y_sig);

    smgr.clk(clk);
    smgr.rst_n(rst_n);
    harness.stk_clear(stk_clear_sig);
    harness.stk_push(stk_push_sig);
    harness.stk_pop(stk_pop_sig);
    harness.stk_in_x(stk_in_x_sig);
    harness.stk_in_y(stk_in_y_sig);
    harness.stk_empty(stk_empty_sig);
    harness.stk_full(stk_full_sig);
    harness.stk_top_x(stk_top_x_sig);
    harness.stk_top_y(stk_top_y_sig);
    harness.stk_peak(stk_peak_sig);
    smgr.clear(stk_clear_sig);
    smgr.push_en(stk_push_sig);
    smgr.pop_en(stk_pop_sig);
    smgr.in_x(stk_in_x_sig);
    smgr.in_y(stk_in_y_sig);
    smgr.empty(stk_empty_sig);
    smgr.full(stk_full_sig);
    smgr.top_x(stk_top_x_sig);
    smgr.top_y(stk_top_y_sig);
    smgr.peak_depth(stk_peak_sig);

    gmem.clk(clk);
    harness.grid_we(grid_we_sig);
    harness.grid_waddr(grid_waddr_sig);
    harness.grid_raddr(grid_raddr_sig);
    harness.grid_wdata(grid_wdata_sig);
    harness.grid_rdata(grid_rdata_sig);
    gmem.we(grid_we_sig);
    gmem.waddr(grid_waddr_sig);
    gmem.wdata(grid_wdata_sig);
    gmem.raddr(grid_raddr_sig);
    gmem.rdata(grid_rdata_sig);

    vmem.clk(clk);
    harness.vis_clear(vis_clear_sig);
    harness.vis_we(vis_we_sig);
    harness.vis_addr(vis_addr_sig);
    harness.vis_wdata(vis_wdata_sig);
    harness.vis_rdata(vis_rdata_sig);
    vmem.clear(vis_clear_sig);
    vmem.we(vis_we_sig);
    vmem.addr(vis_addr_sig);
    vmem.wdata(vis_wdata_sig);
    vmem.rdata(vis_rdata_sig);

    sc_start();

    if (dfs::failures == 0) {
        std::printf("neighbor_stack_memory_test: all checks passed\n");
    }
    return dfs::failures == 0 ? 0 : 1;
}

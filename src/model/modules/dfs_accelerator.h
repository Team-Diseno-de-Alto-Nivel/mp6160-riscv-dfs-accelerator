#pragma once
// DFS Accelerator top-level (HWA-1): single TLM 2.0 target socket, decodes host
// transactions against the memory map, and wires the internal modules together.

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_target_socket.h>

#include <cstdint>

#include "config/accelerator_config.h"
#include "config/memory_map.h"
#include "modules/dfs_controller.h"
#include "modules/grid_memory.h"
#include "modules/neighbor_generator.h"
#include "modules/result_interface.h"
#include "modules/stack_manager.h"
#include "modules/visited_memory.h"
#include "utils/types.h"

namespace dfs {

SC_MODULE(DfsAccelerator) {
    tlm_utils::simple_target_socket<DfsAccelerator> socket;

    sc_in<bool> clk;
    sc_in<bool> rst_n;

    DfsController controller;
    NeighborGenerator neighbor_gen;
    StackManager stack_mgr;
    GridMemory grid_mem;
    VisitedMemory visited_mem;
    ResultInterface result_if;

    SC_CTOR(DfsAccelerator)
        : socket("socket"),
          controller("controller"),
          neighbor_gen("neighbor_gen"),
          stack_mgr("stack_mgr"),
          grid_mem("grid_mem"),
          visited_mem("visited_mem"),
          result_if("result_if") {
        socket.register_b_transport(this, &DfsAccelerator::b_transport);

        SC_METHOD(drive_start_pulse);
        sensitive << clk.pos();
        dont_initialize();

        // Visited Memory only ever gets asked to mark (never unmark) a cell.
        vis_mark_sig_.write(true);

        // Clock / reset fan-out.
        controller.clk(clk);
        controller.rst_n(rst_n);
        neighbor_gen.clk(clk);
        neighbor_gen.rst_n(rst_n);
        stack_mgr.clk(clk);
        stack_mgr.rst_n(rst_n);
        stack_mgr.clear(start_pulse_sig_);
        grid_mem.clk(clk);
        visited_mem.clk(clk);
        result_if.clk(clk);
        result_if.rst_n(rst_n);

        // Host control handshake.
        controller.start(start_pulse_sig_);
        controller.busy(busy_sig_);
        controller.done(done_sig_);

        // Controller <-> Stack Manager.
        controller.stk_push(stk_push_sig_);
        controller.stk_pop(stk_pop_sig_);
        controller.stk_empty(stk_empty_sig_);
        controller.stk_full(stk_full_sig_);
        controller.stk_top_x(stk_top_x_sig_);
        controller.stk_top_y(stk_top_y_sig_);
        stack_mgr.push_en(stk_push_sig_);
        stack_mgr.pop_en(stk_pop_sig_);
        stack_mgr.empty(stk_empty_sig_);
        stack_mgr.top_x(stk_top_x_sig_);
        stack_mgr.top_y(stk_top_y_sig_);
        stack_mgr.full(stk_full_sig_);
        stack_mgr.peak_depth(stk_peak_depth_sig_);
        // Push data: scan candidate or discovered neighbour, see run_fsm().
        controller.stk_in_x(stk_in_x_sig_);
        controller.stk_in_y(stk_in_y_sig_);
        stack_mgr.in_x(stk_in_x_sig_);
        stack_mgr.in_y(stk_in_y_sig_);

        // Controller <-> Neighbor Generator.
        controller.ngen_valid_in(ngen_valid_in_sig_);
        controller.ngen_valid_out(ngen_valid_out_sig_);
        controller.ngen_done(ngen_done_sig_);
        controller.ngen_x(ngen_x_sig_);
        controller.ngen_y(ngen_y_sig_);
        neighbor_gen.valid_in(ngen_valid_in_sig_);
        neighbor_gen.valid_out(ngen_valid_out_sig_);
        neighbor_gen.done(ngen_done_sig_);
        neighbor_gen.nbr_x(ngen_x_sig_);
        neighbor_gen.nbr_y(ngen_y_sig_);
        neighbor_gen.rows(rows_sig_);
        neighbor_gen.cols(cols_sig_);
        neighbor_gen.connectivity(params_sig_);
        // Node under expansion, latched by the controller on pop.
        controller.cur_x(ngen_cur_x_sig_);
        controller.cur_y(ngen_cur_y_sig_);
        neighbor_gen.cur_x(ngen_cur_x_sig_);
        neighbor_gen.cur_y(ngen_cur_y_sig_);

        // Controller <-> Visited Memory.
        controller.vis_we(vis_we_sig_);
        controller.vis_addr(vis_addr_sig_);
        controller.vis_is_visited(vis_is_visited_sig_);
        visited_mem.we(vis_we_sig_);
        visited_mem.addr(vis_addr_sig_);
        visited_mem.rdata(vis_is_visited_sig_);
        visited_mem.wdata(vis_mark_sig_);  // controller only marks, never unmarks
        // A host Start also begins a fresh traversal, so it clears Visited Memory.
        visited_mem.clear(start_pulse_sig_);

        // start_x/y unused by the FSM, see dfs_controller.h.
        controller.start_x(start_x_sig_);
        controller.start_y(start_y_sig_);
        controller.rows(rows_sig_);
        controller.cols(cols_sig_);
        controller.visited_count(visited_count_sig_);
        controller.island_count(island_count_sig_);
        controller.overflow(overflow_sig_);

        // Grid read port shares Visited Memory's address wire — both are
        // indexed the same way, so no separate address port needed.
        grid_mem.we(grid_we_sig_);
        grid_mem.waddr(grid_waddr_sig_);
        grid_mem.wdata(grid_wdata_sig_);
        grid_mem.raddr(vis_addr_sig_);
        grid_mem.rdata(grid_rdata_sig_);
        controller.grid_value(grid_rdata_sig_);

        // "value" is the island count; visited_cells is the total across
        // all islands.
        result_if.done(done_sig_);
        result_if.result_value(island_count_sig_);
        result_if.visited_cells(visited_count_sig_);
        result_if.peak_stack_depth(stk_peak_depth_sig_);
        result_if.result_valid(result_valid_sig_);
    }
    
const DfsController::TimingStats& timing() const {
    return controller.timing();
        }
    
    void b_transport(tlm::tlm_generic_payload& trans, sc_time& delay) {
        delay += sc_time(10, SC_NS);

        unsigned char* ptr = trans.get_data_ptr();
        if (ptr == nullptr || trans.get_data_length() != config::kBusWidthBytes) {
            trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
            return;
        }

        auto* data = reinterpret_cast<std::uint32_t*>(ptr);
        const std::uint64_t addr = trans.get_address();
        bool ok = false;

        switch (trans.get_command()) {
            case tlm::TLM_WRITE_COMMAND:
                ok = write_reg(addr, *data);
                break;
            case tlm::TLM_READ_COMMAND:
                ok = read_reg(addr, *data);
                break;
            default:
                trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
                return;
        }

        trans.set_response_status(ok ? tlm::TLM_OK_RESPONSE : tlm::TLM_ADDRESS_ERROR_RESPONSE);
    }
        
  private:
    // AcceleratorDriver::start() never clears the Start bit, just
    // read-modify-writes it back — so a second start() writes the same
    // value and an edge-detected signal would never re-fire. start_requested_
    // is a plain bool instead, unconditionally set true by every Control
    // write with Start set and consumed once per clock by
    // drive_start_pulse(). Has to be a plain member rather than an sc_signal
    // written directly: write_reg() runs in the caller's process (b_transport),
    // not ours, and sc_signal requires a single writer.
    bool start_requested_ = false;
    // Like start_requested_ but stays set until DfsController reports busy —
    // see the kRegStatus masking below.
    bool run_pending_ = false;

    void drive_start_pulse() {
        start_pulse_sig_.write(start_requested_);
        start_requested_ = false;
        if (busy_sig_.read()) run_pending_ = false;
    }

    bool write_reg(std::uint64_t addr, std::uint32_t value) {
        grid_we_sig_.write(false);
        switch (addr) {
            case memmap::kRegControl:
                enabled_ = (value & memmap::kControlEnable) != 0;
                if (value & memmap::kControlStart) {
                    start_requested_ = true;
                    run_pending_ = true;
                }
                return true;
            case memmap::kRegRows: rows_sig_.write(value); return true;
            case memmap::kRegCols: cols_sig_.write(value); return true;
            case memmap::kRegStartX: start_x_sig_.write(value); return true;
            case memmap::kRegStartY: start_y_sig_.write(value); return true;
            case memmap::kRegParams: params_sig_.write(value); return true;
            default: break;
        }

        if (addr < memmap::kGridBase) return false;  // unmapped register
        const std::uint64_t idx = (addr - memmap::kGridBase) / config::kBusWidthBytes;
        if (idx >= config::kMaxCells) return false;
        grid_waddr_sig_.write(static_cast<std::uint32_t>(idx));
        grid_wdata_sig_.write(static_cast<std::int32_t>(value));
        grid_we_sig_.write(true);
        // Real load happens through this backdoor, not the signal path
        // above — see load_cell() in grid_memory.h for why.
        grid_mem.load_cell(static_cast<std::uint32_t>(idx), static_cast<CellValue>(value));
        return true;
    }

    bool read_reg(std::uint64_t addr, std::uint32_t& value) const {
        switch (addr) {
            case memmap::kRegControl:
                // Start reads back as ~0 most of the time: it self-clears
                // the next cycle via drive_start_pulse().
                value = (enabled_ ? memmap::kControlEnable : 0) |
                        (start_requested_ ? memmap::kControlStart : 0);
                return true;
            case memmap::kRegStatus:
                // Done tracks result_valid (not the controller's raw done)
                // so a host can't read kRegResult before latch() captures
                // it. Also masked by run_pending_ until DfsController is
                // actually busy, so a host re-running back-to-back can't
                // see the *previous* run's still-latched Done. Overflow
                // needs no such masking — it's cleared before that window.
                value = (busy_sig_.read() ? memmap::kStatusBusy : 0) |
                        ((result_valid_sig_.read() && !run_pending_) ? memmap::kStatusDone : 0) |
                        (overflow_sig_.read() ? memmap::kStatusOverflow : 0);
                return true;
            case memmap::kRegRows: value = rows_sig_.read(); return true;
            case memmap::kRegCols: value = cols_sig_.read(); return true;
            case memmap::kRegStartX: value = start_x_sig_.read(); return true;
            case memmap::kRegStartY: value = start_y_sig_.read(); return true;
            case memmap::kRegParams: value = params_sig_.read(); return true;
            case memmap::kRegResult:
            case memmap::kRegVisited:
            case memmap::kRegPeakStack: {
                const ResultData snap = result_if.snapshot();
                value = (addr == memmap::kRegResult)    ? snap.value
                        : (addr == memmap::kRegVisited) ? snap.visited_cells
                                                         : snap.peak_stack_depth;
                return true;
            }
            default:
                return false;  // unmapped register
        }
    }

    bool enabled_ = false;  // ON/OFF toggle (HWA-3)

    // ── Host-visible config registers ───────────────────────────────────────
    sc_signal<sc_uint<32>> rows_sig_{"rows_sig"};
    sc_signal<sc_uint<32>> cols_sig_{"cols_sig"};
    sc_signal<sc_uint<32>> start_x_sig_{"start_x_sig"};
    sc_signal<sc_uint<32>> start_y_sig_{"start_y_sig"};
    sc_signal<sc_uint<32>> params_sig_{"params_sig"};  // connectivity: 4 or 8

    // ── Host control handshake ──────────────────────────────────────────────
    sc_signal<bool> start_pulse_sig_{"start_pulse_sig"};  // one-cycle pulse, see drive_start_pulse()
    sc_signal<bool> busy_sig_{"busy_sig"};
    sc_signal<bool> done_sig_{"done_sig"};
    sc_signal<bool> overflow_sig_{"overflow_sig"};

    // ── Controller <-> Stack Manager ─────────────────────────────────────────
    sc_signal<bool> stk_push_sig_{"stk_push_sig"};
    sc_signal<bool> stk_pop_sig_{"stk_pop_sig"};
    sc_signal<bool> stk_empty_sig_{"stk_empty_sig"};
    sc_signal<sc_uint<32>> stk_top_x_sig_{"stk_top_x_sig"};
    sc_signal<sc_uint<32>> stk_top_y_sig_{"stk_top_y_sig"};
    sc_signal<bool> stk_full_sig_{"stk_full_sig"};
    sc_signal<sc_uint<32>> stk_peak_depth_sig_{"stk_peak_depth_sig"};
    sc_signal<sc_uint<32>> stk_in_x_sig_{"stk_in_x_sig"};
    sc_signal<sc_uint<32>> stk_in_y_sig_{"stk_in_y_sig"};

    // ── Controller <-> Neighbor Generator ────────────────────────────────────
    sc_signal<bool> ngen_valid_in_sig_{"ngen_valid_in_sig"};
    sc_signal<bool> ngen_valid_out_sig_{"ngen_valid_out_sig"};
    sc_signal<bool> ngen_done_sig_{"ngen_done_sig"};
    sc_signal<sc_uint<32>> ngen_x_sig_{"ngen_x_sig"};
    sc_signal<sc_uint<32>> ngen_y_sig_{"ngen_y_sig"};
    sc_signal<sc_uint<32>> ngen_cur_x_sig_{"ngen_cur_x_sig"};
    sc_signal<sc_uint<32>> ngen_cur_y_sig_{"ngen_cur_y_sig"};

    // ── Controller <-> Visited Memory ────────────────────────────────────────
    sc_signal<bool> vis_we_sig_{"vis_we_sig"};
    sc_signal<sc_uint<32>> vis_addr_sig_{"vis_addr_sig"};
    sc_signal<bool> vis_is_visited_sig_{"vis_is_visited_sig"};
    sc_signal<bool> vis_mark_sig_{"vis_mark_sig"};

    // ── Grid Memory (host-loaded) ────────────────────────────────────────────
    sc_signal<bool> grid_we_sig_{"grid_we_sig"};
    sc_signal<sc_uint<32>> grid_waddr_sig_{"grid_waddr_sig"};
    sc_signal<sc_int<32>> grid_wdata_sig_{"grid_wdata_sig"};
    sc_signal<sc_int<32>> grid_rdata_sig_{"grid_rdata_sig"};  // raddr reuses vis_addr_sig_

    // ── Result Interface ─────────────────────────────────────────────────────
    sc_signal<sc_uint<32>> visited_count_sig_{"visited_count_sig"};
    sc_signal<sc_uint<32>> island_count_sig_{"island_count_sig"};
    sc_signal<bool> result_valid_sig_{"result_valid_sig"};
};

}  // namespace dfs

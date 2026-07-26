// Host co-simulation entry point (INT-1): the RISC-V-side program drives the
// real accelerator model (src/model) over TLM 2.0 and cross-checks results
// against the software baseline.
//
//   #29        end-to-end program <-> model over the register/MMIO surface.
//   #38-#41    each of the five algorithms run along the ON path over TLM.
//   #43        per-case cross-check: baseline (OFF) result == accelerator (ON).
//
// RealAcceleratorBridge wraps DfsAccelerator and exposes reg_socket/
// prim_socket — see its file comment for how each maps onto the real
// hardware.

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>

#include <cstdio>
#include <vector>

#include "algorithms/dfs_algorithm.h"
#include "algorithms/number_of_islands/number_of_islands.h"
#include "cases/test_case.h"
#include "driver/accelerator_driver.h"
#include "harness/accelerator.h"
#include "harness/instrumentation.h"
#include "harness/metrics.h"
#include "harness/report.h"
#include "integration/dfs_accelerator_bridge.h"
#include "integration/tlm_accelerator.h"
#include "integration/tlm_bus.h"

namespace dfs::integration {
namespace {

RunMetrics run_with(const AlgoEntry& algo, const TestCase& tc, Accelerator& accel,
                    bool accelerator_on) {
    accel.set_connectivity(tc.input.connectivity);
    accel.load_grid(tc.input.grid);
    const long value = algo.run(tc.input, accel);

    const CostModel cost;
    Counters& c = accel.counters();
    RunMetrics m;
    m.algorithm = algo.name;
    m.case_name = tc.name;
    m.accelerator_on = accelerator_on;
    m.sim_latency_ns = cost.latency_ns(c);
    m.instruction_count = c.ops;
    m.visited_cells = c.visited_cells;
    m.expanded_nodes = c.expanded_nodes;
    m.peak_stack_depth = c.peak_stack_depth;
    m.throughput_cells_per_s =
        m.sim_latency_ns > 0.0
            ? static_cast<double>(c.expanded_nodes) / (m.sim_latency_ns * 1e-9)
            : 0.0;
    m.result_value = value;
    m.passed = tc.expected.valid && value == tc.expected.value;
    return m;
}

}  // namespace

SC_MODULE(Initiator) {
    tlm_utils::simple_initiator_socket<Initiator> reg_init;
    tlm_utils::simple_initiator_socket<Initiator> prim_init;

    std::vector<RunMetrics> metrics;
    bool crosscheck_ok = true;
    int crosscheck_cases = 0;
    bool e2e_ok = true;
    int e2e_cases = 0;

    SC_CTOR(Initiator) : reg_init("reg_init"), prim_init("prim_init") {
        SC_THREAD(run);
    }

    TransportFn reg_transport() {
        return [this](tlm::tlm_generic_payload& t, sc_core::sc_time& d) {
            reg_init->b_transport(t, d);
        };
    }

    TransportFn prim_transport() {
        return [this](tlm::tlm_generic_payload& t, sc_core::sc_time& d) {
            prim_init->b_transport(t, d);
        };
    }

    // INT-1 (#29): drive the accelerator through the register/MMIO surface end to
    // end and confirm the connected-components result matches the SW baseline.
    void run_register_path() {
        TlmBus bus(reg_transport());
        driver::AcceleratorDriver drv(bus);
        CaseLoader loader;

        for (const auto& tc : loader.load("number_of_islands")) {
            Counters base_counters;
            auto baseline = make_accelerator(Mode::Off, base_counters);
            baseline->set_connectivity(tc.input.connectivity);
            baseline->load_grid(tc.input.grid);
            const long expect = number_of_islands(tc.input.grid, *baseline);

            const std::uint32_t params =
                tc.input.connectivity == Connectivity::Eight ? 8u : 4u;
            const AlgoResult got = drv.run(tc.input.grid, {0, 0}, params);

            const bool ok = got.valid && got.value == expect;
            ++e2e_cases;
            if (!ok) e2e_ok = false;
            printf("%-18s %-16s baseline=%ld accel=%ld visited=%u peak=%u  %s\n",
                   "number_of_islands", tc.name.c_str(), expect, got.value,
                   drv.visited(), drv.peak_stack(), ok ? "ok" : "MISMATCH");
        }
    }

    // INT-1 (#38-#41) + cross-check (#43): every algorithm along the ON path over
    // TLM, compared per case against the OFF baseline.
    void run_primitive_path() {
        CaseLoader loader;
        for (const AlgoEntry& algo : make_algorithms()) {
            for (const auto& tc : loader.load(algo.dataset_key)) {
                Counters off_counters;
                auto off = make_accelerator(Mode::Off, off_counters);
                const RunMetrics moff = run_with(algo, tc, *off, false);

                Counters on_counters;
                TlmAccelerator on(Mode::On, on_counters, prim_transport());
                const RunMetrics mon = run_with(algo, tc, on, true);

                ++crosscheck_cases;
                if (moff.result_value != mon.result_value) crosscheck_ok = false;

                metrics.push_back(moff);
                metrics.push_back(mon);
            }
        }
    }

    void run() {
        printf("\n=== INT-1 end-to-end: register/MMIO path (program -> TLM -> model) ===\n");
        run_register_path();
        run_primitive_path();
        sc_stop();
    }
};

}  // namespace dfs::integration

int sc_main(int, char*[]) {
    using namespace dfs;
    using namespace dfs::integration;

    RealAcceleratorBridge model("model");
    Initiator initiator("initiator");
    initiator.reg_init.bind(model.reg_socket);
    initiator.prim_init.bind(model.prim_socket);

    sc_start();

    const bool report_ok = report(initiator.metrics);

    printf("\n=== INT-1 register-path end-to-end: %d/%d cases ok ===\n",
           initiator.e2e_ok ? initiator.e2e_cases : 0, initiator.e2e_cases);
    printf("=== INT cross-check (OFF baseline vs ON accelerator): %s (%d cases) ===\n",
           initiator.crosscheck_ok ? "MATCH" : "MISMATCH", initiator.crosscheck_cases);

    const bool ok = report_ok && initiator.e2e_ok && initiator.crosscheck_ok;
    printf("\n%s\n", ok ? "INTEGRATION PASSED" : "INTEGRATION FAILED");
    return ok ? 0 : 1;
}

# Metrics schema and consolidated CSV (INT-2, #45)

The harness emits one metrics row per **(algorithm case × accelerator mode)**. The
software baseline (`OFF`) and the accelerated path (`ON`) are run for every case,
so each case contributes two rows. The consolidated CSV is produced by
`scripts/run_experiments.sh` at `results/metrics.csv` and consumed by
`scripts/plot_results.py` and the paper (DOC-3, #33).

The schema is defined in code by `dfs::RunMetrics`
([src/program/harness/metrics.h](../../src/program/harness/metrics.h)) and written
by `dfs::report` ([src/program/harness/report.cpp](../../src/program/harness/report.cpp)).

## Columns

| Column | Type | Unit | Meaning |
|---|---|---|---|
| `algorithm` | string | — | Algorithm variant (e.g. `number_of_islands`, `longest_increasing_path_nomemo`). |
| `case` | string | — | Dataset case name (e.g. `noi_classic_4c`). |
| `accelerator_on` | int | 0/1 | Mode: `0` = software baseline (OFF), `1` = accelerated path (ON). |
| `passed` | int | 0/1 | `1` when the result equals the expected value for the case. |
| `result` | long | — | Algorithm result (island count, path count, longest path, …). |
| `latency_ns` | double | ns | Simulated latency = `ops × cycles_per_op × clock_period_ns` (cost model, 10 ns clock). |
| `instruction_count` | uint64 | ops | Modelled operations charged during the run (`Counters::ops`). |
| `expanded_nodes` | uint64 | nodes | DFS nodes expanded (popped from the stack). |
| `visited_cells` | uint64 | cells | Distinct grid cells marked visited. |
| `peak_stack_depth` | uint64 | entries | Maximum DFS stack depth reached. |
| `throughput_cells_per_s` | double | cells/s | `expanded_nodes / (latency_ns × 1e-9)`. |
| `grid_cells` | uint64 | cells | `rows × cols` of the case's grid. The x-axis of the scalability sweep. |
| `tier` | string | — | Dataset tier: `legacy` (hand-written) or `small`/`medium`/`large` (generated). See [synthetic-datasets.md](synthetic-datasets.md). |

## Conventions

- **One row per (case × mode).** Cross-checking OFF vs ON (#43) pairs rows on
  `(algorithm, case)`; `result` must match across modes.
- **Latency** is a *modelled* figure from the shared cost model
  ([src/program/harness/instrumentation.h](../../src/program/harness/instrumentation.h)),
  not wall-clock time, so it is deterministic and reproducible across hosts.
- **Speedup** (reported by the tools, not stored) is `OFF.latency_ns / ON.latency_ns`.
- The co-simulation over TLM (`integration_sim`) emits the same schema at
  `results/integration_metrics.csv`; its ON rows travel through the modelled
  accelerator interface rather than the in-process reference model.
- The on-board throughput benchmark (`src/onboard/benchmark_cynq.cpp`, #91)
  emits the first 11 base columns at `results/onboard_metrics.csv`
  (via `make onboard-fetch-metrics`), reinterpreting some of them for
  real hardware and appending hardware-specific columns:
  - `grid_cells` and `tier` are **not** emitted: this producer runs the
    fixed on-board case fixture (`src/onboard/cases.json`), not the
    tiered scalability sweep those two columns exist for.
  - `accelerator_on` is always `1` -- this producer only exercises the
    accelerated path on the KV260; the ON/OFF contrast is a
    simulation-level experiment (see the paper's experiment plan), not an
    on-board one.
  - `latency_ns` is the **median of repeated real measurements** on the
    KV260 (PS-side `std::chrono` timer around `Start()`/`Sync()`), not the
    cost model -- same pattern as `integration_metrics.csv` substituting
    TLM latency for the cost model, just with real hardware latency
    instead.
  - `instruction_count` is always `0` -- there is no cost-model
    instrumentation inside the HLS kernel to report it from.
  - `throughput_cells_per_s` uses the same formula as the base schema,
    computed from the real `expanded_nodes` and measured median latency.
  - Extra columns: `hw_clock_mhz` (PL clock confirmed via `GetClocks()`
    after `SetClocks()`), `iterations`/`warmup_iterations` (timed vs.
    discarded runs per case), `latency_ns_min`/`latency_ns_median`/
    `latency_ns_max`/`latency_ns_stddev` (full latency distribution across
    the timed iterations), and `throughput_ops_sustained`
    (`1e9 / latency_ns_median` -- full accelerator invocations per second).
  - `noi_all_water` is the case to check first: its cost-model `latency_ns`
    is `0` (no DFS work), but its on-board `latency_ns` is expected to be
    `> 0` (the full grid sweep still costs real cycles) -- that gap is the
    expected finding for #92 (cost model vs. measured hardware), not a bug.

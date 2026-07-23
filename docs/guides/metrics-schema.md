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

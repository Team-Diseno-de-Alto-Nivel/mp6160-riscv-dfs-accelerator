# Generated Datasets and the Scalability Sweep

The hand-written datasets in [cases/datasets.cpp](../../src/program/cases/datasets.cpp)
top out at 5x5. They cover best/worst case qualitatively (4 vs 8 connectivity,
density, no-path, single cell) but say nothing about how the design behaves at
scale. This guide covers the **generated** datasets, which run the same
algorithms over grids from 64x64 up to 2048x2048 (4.19M cells).

Generated grids are never committed. What is committed is the *descriptor*
(topology, size, density, seed, ~50 bytes); the grid is materialized in memory at
run time and freed before the next case. Peak memory stays at roughly one grid.

## Tiers

Grid capacity is bounded at three different points in the stack, so the datasets
are stratified to match. Running a tier past its hardware limit would silently
drop the cross-check that makes the numbers trustworthy.

| Tier | Max size | Runs on | Cross-check |
|---|---|---|---|
| `legacy` | 5x5 | everything | full, up to on-board KV260 |
| `small` | 64x64 (4,096) | software, SystemC, HLS cosim, KV260 | full |
| `medium` | 256x256 (65,536) | software, SystemC integration | against the real `DfsAccelerator` |
| `large` | up to 2048x2048 (4.19M) | native software only | OFF vs ON only |

The limits come from [`dfs_accel.h`](../../src/hls/dfs_accel.h) (`kMaxRows`/`kMaxCols` = 64,
128 KB footprint budget) and [`accelerator_config.h`](../../src/model/config/accelerator_config.h)
(`kMaxRows`/`kMaxCols` = 256). Tier `large` exceeds both, so it is software-only
and `run_experiments.sh` skips the co-simulation for it.

`legacy` is the default: a plain `program` run with no arguments produces exactly
the same 42 runs it always did, so CI and the paper's baseline are unaffected.

## Not every algorithm scales

Three of the seven variants are exponential and will not terminate on a large
grid. This is a property of the problems, not a limitation of the generator, and
it is encoded directly in the `max_tier` field of each `AlgoEntry` in
[dfs_algorithm.cpp](../../src/program/algorithms/dfs_algorithm.cpp):

| Variant | Complexity | `max_tier` |
|---|---|---|
| `number_of_islands` | O(cells) | `large` |
| `pacific_atlantic` | O(cells) | `large` |
| `longest_increasing_path` (memo) | O(cells) | `large` |
| `word_search_ii` (pruned) | ~O(cells x words), `kMaxWordLen` bounds depth | `medium` |
| `longest_increasing_path_nomemo` | exponential in path length | `legacy` |
| `word_search_ii_nopruning` | O(cells x 4^16) | `legacy` |
| `unique_paths_iii` | counts Hamiltonian-style paths, exponential | `legacy` |

A variant capped at `legacy` never receives a generated case, even when its
dataset key has them: the harness intersects the requested tier with the
algorithm's own `max_tier`.

## Recursion depth

`number_of_islands` and `pacific_atlantic` are iterative (explicit `CountedStack`),
so they are safe at any size. `longest_increasing_path` and `word_search_ii`
recurse. Word search is bounded by the word length (<= 16). LIP recurses as deep
as the longest increasing path, which is why the `Gradient` topology uses
`value = r + c`: that caps depth at `rows + cols - 1` (4,095 at 2048x2048). A
monotone topology covering every cell would recurse 4M deep and overflow the
host stack; do not add one.

## Topologies

| Topology | Shape | Purpose |
|---|---|---|
| `UniformRandom` | random 0/1 at `density` | the sweep baseline |
| `Checkerboard` | `(r+c) % 2` | maximum fragmentation; separates 4 vs 8 connectivity |
| `AllLand` | all 1 | one giant flood fill, maximum stack depth |
| `AllWater` | all 0 | degenerate, zero work |
| `Serpentine` | boustrophedon corridor | one long thin region |
| `DiagonalStripes` | `(r+c) % 4 < 2` | diagonal bands |
| `Gradient` | `value = r + c` | monotone ramp; LIP is exactly `rows + cols - 1` |
| `RandomHeights` | random in `[0, levels)` | height maps for LIP and Pacific/Atlantic |
| `Letters` | random over an alphabet | boards for Word Search II |

## Reproducibility

The PRNG is a hand-rolled xorshift64* with splitmix64 seeding
([generators.h](../../src/program/cases/generators.h)). `std::uniform_int_distribution`
is deliberately **not** used: its output is implementation-defined, so the same
seed would produce different grids under different standard libraries and the
RISC-V, native, and container runs would diverge.

Verified: a `--tier=medium` run (146 runs) produces byte-identical CSV on
macOS/arm64 and Linux/amd64.

## How expected values are established

Generated cases cannot carry a hand-written expected value. Two mechanisms cover
them, and every case uses one or the other:

- **By construction**, where the topology fixes the answer: `AllLand` has 1
  island, `Checkerboard` under 4-connectivity has `(rows*cols+1)/2`, under
  8-connectivity 1, `Serpentine` 1, and a `Gradient` grid's longest increasing
  path is `rows + cols - 1`.
- **An independent oracle** ([oracle.cpp](../../src/program/cases/oracle.cpp))
  for everything else. The oracles deliberately do not share code with the
  algorithms under test, and where possible use a different method entirely:
  the LIP oracle sorts cells by height and fills a DP table rather than doing a
  memoized DFS.

This matters because OFF and ON validate against each other, so a defect
affecting both modes equally would pass unnoticed. The oracle closes that gap.

## Running

```bash
make experiments          # legacy, the CI default: 42 runs, unchanged
make experiments-small    # + generated 64x64            94 runs
make experiments-medium   # + generated 256x256         146 runs
make experiments-large    # + generated up to 2048x2048 272 runs
```

Outputs land in `results/metrics_<tier>.csv` and `results/run_<tier>.log`; the
default `legacy` tier keeps writing `results/metrics.csv` and `results/run.log`.
The CSV gains two columns over the schema in
[metrics-schema.md](metrics-schema.md): `grid_cells` and `tier`.

Reference wall-clock on an M-series host: small 0.04 s, medium 0.6 s, large 17 s.

## What the sweep shows

All 272 runs pass at `large`. The measured results:

**Speedup does not grow with grid size.** Under the current cost model the only
modeled advantage is parallel neighbour generation, charged 1 op instead of 4 or
8. Since neighbour queries, visited accesses and stack operations all scale
together, the ratio is fixed by the algorithm's primitive mix, not by size:

| Algorithm | 4,096 cells | 65,536 | 262,144 | 1,048,576 | 4,194,304 |
|---|---:|---:|---:|---:|---:|
| `longest_increasing_path` | 4.000x | 4.000x | 4.000x | 4.000x | 4.000x |
| `number_of_islands` | 1.441x | 1.441x | 1.441x | 1.441x | 1.441x |
| `pacific_atlantic` | 1.181x | 1.073x | 1.041x | 1.022x | 1.011x |

LIP sits exactly on the theoretical ceiling of `k` = 4 for 4-connectivity, which
is reached when neighbour queries dominate and everything else vanishes.

**Pacific/Atlantic degrades toward 1.0x.** This one is not flat, and it is the
sweep's most useful finding. The algorithm ends with two full-grid readbacks
that cost O(cells) in *both* modes, while the flood itself only reaches cells
that have a non-decreasing path to an ocean. That reachable fraction collapses
as the grid grows (11.99% at 4K cells, 0.38% at 4.19M), so the accelerated part
shrinks against a fixed serial cost and Amdahl's law takes over. The accelerator
advantage is effectively gone at 4M cells.

**Stack sizing is validated, and only by dense grids.** `kStackDepth = kMaxCells`
turns out to be correctly sized: the worst case, `AllLand`, peaks at 32,641 on a
256x256 grid, comfortably under the 65,536 capacity, and the peak tracks roughly
`cells / 2`. Random grids never come close (peak 21 to 38 even at 4.19M cells),
so a sweep using only random data would have left this dimension untested.

| Topology (256x256) | Peak stack |
|---|---:|
| `AllLand` | 32,641 |
| `Checkerboard` 8-connectivity | 5,757 |
| `UniformRandom` | 21 |
| `Checkerboard` 4-connectivity | 1 |

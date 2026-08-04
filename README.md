# DFS RISC-V Hardware Accelerator

> Academic project — MP6160 Diseño de Alto Nivel de Sistemas Electrónicos, Instituto Tecnológico de Costa Rica

A high-level model of a RISC-V processor with hardware acceleration for *Depth-First Search* (DFS) over 2D matrices, built with **SystemC**.

**Team:** Gabriel Abarca Aguilar, Jesús Alberto Castro Murillo, José Fabio Jaramillo Cordero, Moisés Leiva Solano, Noel Antonio Pérez Cáceres

---

## Table of Contents

- [Requirements & Build Instructions](#requirements--build-instructions)
- [Repository Organization](#repository-organization)
- [Module Organization](#module-organization)
- [Block Diagram](#block-diagram)
- [Sequence Diagram](#sequence-diagram)
- [Transaction Format](#transaction-format)
- [Memory Map](#memory-map)
- [Results](#results)
- [AI-Assisted Development](#ai-assisted-development)

---

## Requirements & Build Instructions

### Dev container (recommended)

The repository ships a dev container with all dependencies preinstalled, including a pre-compiled SystemC 2.3.4.

**VS Code:**
1. Install the [Dev Containers](https://marketplace.visualstudio.com/items?itemName=ms-vscode.remote-containers) extension
2. Open the repository and select **Reopen in Container**

**Docker CLI:**
```bash
docker build -t dfs-riscv .devcontainer/
docker run -it --rm -v $(pwd):/workspace -w /workspace dfs-riscv make run
```

**GitHub Codespaces:** the same `devcontainer.json` works out of the box.

### Local build

Each part of the project is its own sub-build, with its own toolchain. The root `Makefile` just delegates:

| Target | What it builds | Toolchain |
|---|---|---|
| `make model` | `src/model/` — the SystemC simulation (`sim`) | host C++ compiler (GCC ≥ 9 / Clang ≥ 10, C++17) + CMake ≥ 3.16 |
| `make program` | `src/program/` — the bare-metal RISC-V binary | `riscv64-unknown-elf-gcc`/`g++` (cross-compiler) + CMake ≥ 3.16 |
| `make integration` | `src/program/` (native) — co-simulates the RISC-V software against the real `DfsAccelerator` over TLM and cross-checks every algorithm's ON result against its OFF baseline | host C++ compiler + CMake + **an installed SystemC** (see note below) |
| `make paper` | `docs/paper/main.pdf` | `latexmk`/`pdflatex`/`bibtex` |

```bash
git clone <repository-url>
cd <repository>
make              # builds model + program
make run          # builds both and runs sim, passing it the program binary as a parameter
make integration  # builds and runs the program<->model co-simulation cross-check
make clean        # cleans model + program build dirs
make paper        # builds docs/paper/main.pdf
make paper-clean  # removes LaTeX build artifacts
```

If SystemC is already installed, export `SYSTEMC_HOME` before `make` to skip the download. The RISC-V cross-compiler is only available inside the dev container (see below) — it doesn't install automatically.

`make integration` specifically needs `SYSTEMC_HOME` pointing at an **installed** SystemC (headers under `$SYSTEMC_HOME/include`, library under `$SYSTEMC_HOME/lib`) — unlike `make model`, it does not auto-fetch one. The dev container already has this set up. Building locally without it: run `make model` once (which fetches and builds SystemC via CMake `FetchContent` under `src/model/build/_deps/systemc-build`), then `cmake --install src/model/build --prefix <some-dir>` and export `SYSTEMC_HOME=<some-dir>` before `make integration`.

### Running & demo

- **Run the solution** (simulation and RISC-V emulation) and read the output — [docs/guides/running-the-simulation.md](docs/guides/running-the-simulation.md).
- **Run the experiments** (`make experiments`) to collect the metrics CSV, tables, and plots — [docs/guides/running-experiments.md](docs/guides/running-experiments.md).
- **Run the scalability sweep** (`make experiments-small|-medium|-large`) over generated grids up to 2048×2048 — [docs/guides/synthetic-datasets.md](docs/guides/synthetic-datasets.md).
- **`make demo`** runs the reproducible one-command demo (RISC-V emulation + SystemC co-simulation). The [`demo.yml`](.github/workflows/demo.yml) workflow records it into an MP4 artifact.

### CI/CD

- [`build.yml`](.github/workflows/build.yml): on every push/PR touching `src/`, `Makefile`, or `.devcontainer/`, builds the model and the program, runs the simulation, and verifies the program **both** under RISC-V emulation (qemu) and as a native build.
- [`paper.yml`](.github/workflows/paper.yml): compiles the LaTeX paper (`docs/paper/`) on every push to `main` and every PR that modifies it (PDF uploaded as an artifact); on PRs it also runs an **AI-writing gate** (see below).
- [`demo.yml`](.github/workflows/demo.yml): manually triggered (`workflow_dispatch`); records `make demo` and renders the reproducible demo video (MP4) as a downloadable artifact.

See the [CI/CD guide](docs/guides/ci-cd.md) for flow diagrams and how each step verifies.

---

## Repository Organization

```
.
├── .devcontainer/
│   ├── Dockerfile                 # Linux image with SystemC pre-built
│   └── devcontainer.json          # Dev container config (VS Code / Codespaces)
├── .github/
│   ├── skills/
│   │   └── log-ai.md              # Claude Code skill to log AI usage
│   └── workflows/
│       ├── build.yml              # CI: builds and runs the SystemC simulation
│       └── paper.yml              # CI: compiles the LaTeX paper
├── docs/
│   └── paper/
│       ├── main.tex                # Scientific paper (IEEE format)
│       ├── refs.bib                # Bibliography
│       ├── .gitignore               # LaTeX build artifacts
│       └── Makefile                 # Builds main.pdf via latexmk
├── src/
│   ├── model/                      # SystemC hardware model (host C++)
│   │   ├── config/                 # accelerator_config, memory_map, tlm_protocol
│   │   ├── modules/                 # one SystemC module per HW component
│   │   ├── utils/                   # SystemC value types (types.h)
│   │   ├── sc_main.cpp              # sc_main — instantiates the accelerator top-level
│   │   ├── CMakeLists.txt           # Builds sim; auto-fetches SystemC if needed
│   │   └── Makefile                 # Thin CMake wrapper
│   └── program/                    # bare-metal C++ program run by the simulated RISC-V core
│       ├── algorithms/              # one folder per DFS algorithm (5 problems)
│       ├── cases/                   # test-case format + hand-written datasets
│       ├── harness/                 # case runner, metrics, accelerator ON/OFF seam
│       ├── dfs_types.h              # shared software value types
│       ├── main.cpp                 # entry point — drives the harness
│       ├── riscv-toolchain.cmake    # CMake toolchain file for the RISC-V cross-compiler
│       ├── CMakeLists.txt           # Builds the bare-metal binary
│       └── Makefile                 # Thin CMake wrapper
├── Makefile                       # Delegates to src/model, src/program, docs/paper
└── README.md
```

---

## Module Organization

The project has two halves that meet at a TLM 2.0 interface: a **software baseline**
that runs on the simulated RISC-V core, and the **SystemC hardware model** of the
accelerator it can offload to.

### Software — `src/program/`

Bare-metal C++ compiled for RISC-V. Each DFS problem implements a common
interface so the harness can run them uniformly, once with the accelerator OFF
(pure software) and once ON (primitives offloaded).

| Component | Path | Responsibility |
|---|---|---|
| DFS algorithms | `algorithms/<problem>/` | The five LeetCode DFS problems as free functions (+ no-pruning / no-memo variants) |
| Algorithm registry | `algorithms/dfs_algorithm.h` | `AlgoEntry` registry + `make_algorithms()` — the test bench that calls each function |
| Accelerator seam | `harness/accelerator.h` | `Mode::{Off,On}` abstraction over the DFS primitives |
| Harness | `harness/harness.{h,cpp}` | Runs every case × mode, validates, records metrics |
| Metrics | `harness/metrics.h` | `RunMetrics` (latency, instr. count, throughput, …) |
| Test cases | `cases/test_case.h` + `cases/datasets.cpp` | `Problem` (algorithm input) + `TestCase` (`Problem` + expected) + embedded datasets |
| Generated datasets | `cases/grid_spec.h` + `cases/generators.*` + `cases/synthetic.cpp` | Seeded topology generators and the tiered case catalog (64×64 up to 2048×2048) |
| Expected-value oracles | `cases/oracle.cpp` | Independent reference implementations that validate the generated cases |

> **How it runs:** see the [software pipeline guide](docs/guides/software-pipeline.md)
> for the pipeline flow diagram, the step-by-step walkthrough, and build/run instructions.

### Hardware — `src/model/`

The accelerator is a SystemC top-level module (`DfsAccelerator`) exposing one TLM
2.0 target socket. Internally it is split into three stages — **control**,
**exploration**, and **storage** — matching the paper's proposed architecture.

| Stage | Module | Path | Responsibility |
|---|---|---|---|
| Top-level | `DfsAccelerator` | `modules/dfs_accelerator.h` | TLM socket, host-transaction decode, module interconnect |
| Control | `DfsController` | `modules/dfs_controller.h` | FSM driving the traversal without CPU intervention |
| Control | `ResultInterface` | `modules/result_interface.h` | Latches the result once `done`, so a host reading it can never race an in-flight run |
| Exploration | `NeighborGenerator` | `modules/neighbor_generator.h` | 4/8 neighbours of a cell with boundary handling |
| Exploration | `StackManager` | `modules/stack_manager.h` | LIFO of unexplored nodes; peak-depth tracking |
| Storage | `GridMemory` | `modules/grid_memory.h` | Read-only input grid |
| Storage | `VisitedMemory` | `modules/visited_memory.h` | Per-cell visited state, cleared per run |

> **Status:** all modules are implemented and verified end to end — `make integration`
> co-simulates the real RISC-V software against this real `DfsAccelerator` (not a
> mock) over TLM and cross-checks every algorithm's accelerator-ON result against
> its software-OFF baseline. `DfsController` performs a **multi-source** scan
> (row-major over the whole grid, one flood-fill per unvisited passable cell),
> matching real "number of islands" semantics on the register/whole-traversal
> path; the other four algorithms are exercised through the fine-grained
> primitive path (`is_visited`/`mark_visited`/`neighbours`), bridged onto the same
> `GridMemory`/`VisitedMemory` — see
> [`dfs_accelerator_bridge.h`](src/program/integration/dfs_accelerator_bridge.h).

---

## Block Diagram

```mermaid
flowchart LR
    subgraph HOST["RISC-V host (src/program)"]
        MAIN["main"] --> HARNESS["Harness"]
        HARNESS --> ALGOS["DFS algorithms<br/>(5 problems)"]
        ALGOS --> ACC["Accelerator seam<br/>Mode: Off / On"]
    end

    ACC -- "OFF: software DFS" --> HARNESS
    ACC == "ON: TLM 2.0 transactions" ==> SOCKET

    subgraph MODEL["DFS Accelerator — SystemC model (src/model)"]
        SOCKET(["TLM 2.0<br/>target socket"]) --> CTRL

        subgraph CONTROL["Control"]
            CTRL["DfsController<br/>(FSM)"]
            RES["ResultInterface"]
        end

        subgraph EXPLORE["Exploration"]
            NGEN["NeighborGenerator"]
            STK["StackManager"]
        end

        subgraph STORAGE["Storage"]
            GRID["GridMemory"]
            VIS["VisitedMemory"]
        end

        CTRL <--> NGEN
        CTRL <--> STK
        CTRL <--> VIS
        CTRL <--> GRID
        CTRL --> RES
        RES --> SOCKET
    end
```

---

## Sequence Diagram

A full run along the accelerated path (register/whole-traversal, `Mode::On`).
`DfsController` scans the grid row-major for unvisited, passable cells,
flood-filling from each one — the result is an island count, matching real
"number of islands" semantics:

```mermaid
sequenceDiagram
    participant P as RISC-V program
    participant A as DfsAccelerator (TLM)
    participant C as DfsController (FSM)
    participant G as GridMemory
    participant V as VisitedMemory
    participant N as NeighborGenerator
    participant S as StackManager
    participant R as ResultInterface

    P->>A: LoadGrid + SetRows/Cols/Params
    P->>A: SetEnable(on) + Start
    A->>C: start (self-clearing pulse)
    C->>V: clear
    loop scan cells row-major until the grid is exhausted
        C->>S: push(next unvisited candidate)
        loop until stack empty
            C->>S: pop() -> node
            C->>V: is_visited(node)?
            C->>G: cell value(node)?
            alt not visited and passable
                C->>V: mark_visited(node)
                Note over C: first cell of a fresh region -> island_count++
                C->>N: neighbours(node)
                N-->>C: up to 4/8 in-bounds candidates
                C->>S: push(candidates)
            end
        end
    end
    C->>R: done, island_count, visited_count, peak_stack, overflow
    P->>A: ReadStatus (poll) -> ReadResult / ReadVisited / ReadPeakStack
    A-->>P: register values
```

The other four algorithms (everything except `number_of_islands`) run along a
separate, fine-grained primitive path instead — the software-side algorithm
code calls `is_visited`/`mark_visited`/`neighbours` directly, one TLM
transaction per primitive, bridged onto the same `GridMemory`/`VisitedMemory`
storage (see [`dfs_accelerator_bridge.h`](src/program/integration/dfs_accelerator_bridge.h)).

---

## Transaction Format

The host drives the accelerator purely by address: every transaction is a
plain TLM 2.0 `TLM_WRITE_COMMAND`/`TLM_READ_COMMAND` (4-byte payload) against
a fixed offset from the [memory map](#memory-map) below — see
[`DfsAccelerator::b_transport`](src/model/modules/dfs_accelerator.h) for the
decode logic and [`AcceleratorDriver`](src/program/driver/accelerator_driver.h)
for the host-side sequence: load the grid → set rows/cols/start/params →
enable → start → poll status → read result/visited/peak-stack.

[`src/model/config/tlm_protocol.h`](src/model/config/tlm_protocol.h) also
defines a `CommandExtension` (a proper `tlm::tlm_extension` with a `Command`
enum and a scalar operand, including `clone()`/`copy_from()`) for a possible
future out-of-band or finer-grained command channel — it's fully implemented
but **not** wired into `b_transport`, which only ever decodes by address.

---

## Memory Map

Register layout seen by the host, from
[`src/model/config/memory_map.h`](src/model/config/memory_map.h) (word-addressed):

| Offset | Register | Access | Notes |
|---|---|---|---|
| `0x0000` | `CONTROL` | W | bit0 `START` (self-clearing pulse), bit1 `ENABLE` (ON/OFF) |
| `0x0004` | `STATUS` | R | bit0 `BUSY`, bit1 `DONE`, bit2 `OVERFLOW` (a push was dropped this run — see below) |
| `0x0008` | `ROWS` | W | grid rows |
| `0x000C` | `COLS` | W | grid cols |
| `0x0010` | `START_X` | W | not consulted by `DfsController`'s multi-source scan (it always covers the whole grid); read back as written |
| `0x0014` | `START_Y` | W | same as `START_X` |
| `0x0018` | `PARAMS` | W | connectivity, `4` or `8` (any other value is treated as `4`) |
| `0x0020` | `RESULT` | R | island count |
| `0x0024` | `VISITED` | R | total cells marked visited, across every island |
| `0x0028` | `PEAK_STACK` | R | peak stack depth this run |
| `0x1000`+ | `GRID` | W | input-grid payload window, one word per cell |

`OVERFLOW` is sticky for the run (cleared only when a new one starts): it
means `StackManager` was at capacity (`kStackDepth` = `kMaxCells` =
256×256) when the FSM tried to push at least once, so that push was dropped
and the run's result under-counts real coverage — see the scope note atop
[`dfs_controller.h`](src/model/modules/dfs_controller.h).

---

## Results

`make experiments`: 21 cases × OFF/ON = 42 runs, all passing. Latency is
modelled (cost model, 10 ns clock), not wall-clock; speedup is `OFF / ON`.
See [running-experiments.md](docs/guides/running-experiments.md) and
[metrics-schema.md](docs/guides/metrics-schema.md).

### Per problem

| Problem (variant) | Cases | Latency OFF (ns) | Latency ON (ns) | Speedup range | Geometric mean |
|---|---:|---:|---:|---:|---:|
| Number of Islands | 6 | 4,420 | 2,650 | 1.39×–1.97× | 1.65× |
| Unique Paths III | 3 | 22,180 | 15,040 | 1.47×–1.50× | 1.48× |
| Word Search II | 2 | 1,400 | 920 | 1.47×–1.60× | 1.54× |
| Word Search II (no pruning) | 2 | 1,190 | 740 | 1.55×–1.67× | 1.61× |
| Pacific Atlantic | 2 | 4,030 | 3,010 | 1.33×–1.50× | 1.41× |
| Longest Increasing Path | 3 | 760 | 190 | 4.00× | 4.00× |
| Longest Increasing Path (no memo) | 3 | 1,880 | 470 | 4.00× | 4.00× |
| **Total** | **21** | **35,860** | **23,020** | | **1.56×** (aggregate) |

`noi_all_water` does zero DFS work (0 ns in both modes) and is excluded from the
ranges and means.

### Per case

Traversal counters are identical in both modes, so they are listed once.
Longest Increasing Path does not track visited cells.

| Problem (variant) | Case | Result | Expanded nodes | Visited cells | Peak stack | Latency OFF (ns) | Latency ON (ns) | Speedup | Match |
|---|---|---:|---:|---:|---:|---:|---:|---:|:---:|
| Number of Islands | `noi_classic_4c` | 3 | 7 | 7 | 2 | 660 | 450 | 1.47× | ✓ |
|  | `noi_classic_8c` | 1 | 7 | 7 | 3 | 1,020 | 530 | 1.92× | ✓ |
|  | `noi_checker_4c` | 8 | 8 | 8 | 1 | 640 | 400 | 1.60× | ✓ |
|  | `noi_checker_8c` | 1 | 8 | 8 | 5 | 1,140 | 580 | 1.97× | ✓ |
|  | `noi_all_water` | 0 | 0 | 0 | 0 | 0 | 0 | — | ✓ |
|  | `noi_all_land_4c` | 1 | 9 | 9 | 3 | 960 | 690 | 1.39× | ✓ |
| Unique Paths III | `up3_one_obstacle` | 2 | 95 | 95 | 11 | 7,160 | 4,820 | 1.49× | ✓ |
|  | `up3_open` | 4 | 195 | 195 | 12 | 14,750 | 10,040 | 1.47× | ✓ |
|  | `up3_no_path` | 0 | 5 | 5 | 3 | 270 | 180 | 1.50× | ✓ |
| Word Search II | `ws2_classic` | 2 | 41 | 9 | 5 | 840 | 570 | 1.47× | ✓ |
|  | `ws2_small` | 4 | 10 | 7 | 4 | 560 | 350 | 1.60× | ✓ |
| Word Search II (no pruning) | `ws2_classic` | 2 | 57 | 7 | 4 | 590 | 380 | 1.55× | ✓ |
|  | `ws2_small` | 4 | 18 | 8 | 4 | 600 | 360 | 1.67× | ✓ |
| Longest Increasing Path | `lip_desc` | 4 | 18 | — | 2 | 360 | 90 | 4.00× | ✓ |
|  | `lip_zigzag` | 4 | 18 | — | 4 | 360 | 90 | 4.00× | ✓ |
|  | `lip_single` | 1 | 1 | — | 1 | 40 | 10 | 4.00× | ✓ |
| Longest Increasing Path (no memo) | `lip_desc` | 4 | 23 | — | 4 | 920 | 230 | 4.00× | ✓ |
|  | `lip_zigzag` | 4 | 23 | — | 4 | 920 | 230 | 4.00× | ✓ |
|  | `lip_single` | 1 | 1 | — | 1 | 40 | 10 | 4.00× | ✓ |
| Pacific Atlantic | `pa_classic` | 7 | 32 | 32 | 9 | 3,850 | 2,890 | 1.33× | ✓ |
|  | `pa_single` | 1 | 2 | 2 | 1 | 180 | 120 | 1.50× | ✓ |

`instruction_count` and `throughput_cells_per_s` are derived (latency is
`ops × 10 ns`, throughput is `expanded_nodes / latency`) and live in
`results/metrics.csv`, regenerated by CI as the `experiment-results` artifact.
The program↔model co-simulation over TLM reports the same metrics.

### Scalability (generated datasets)

`make experiments-large`: 115 generated cases plus the 21 hand-written ones,
× OFF/ON = 272 runs, all passing, on grids from 64×64 up to 2048×2048 (4.19M cells). Software-only — these grids
exceed both the SystemC model (256×256) and the HLS kernel (64×64), so there is
no hardware cross-check at this scale. Full method and topology catalog in
[synthetic-datasets.md](docs/guides/synthetic-datasets.md).

Speedup is **invariant to grid size**, because the only advantage the cost model
charges is parallel neighbour generation (1 op instead of 4/8) and every
primitive scales together:

| Algorithm | 4,096 | 65,536 | 262,144 | 1,048,576 | 4,194,304 |
|---|---:|---:|---:|---:|---:|
| Longest Increasing Path | 4.000× | 4.000× | 4.000× | 4.000× | 4.000× |
| Number of Islands | 1.441× | 1.441× | 1.441× | 1.441× | 1.441× |
| Pacific Atlantic | 1.181× | 1.073× | 1.041× | 1.022× | 1.011× |

Longest Increasing Path sits exactly on the theoretical ceiling for
4-connectivity (`k` = 4). Pacific Atlantic instead **decays toward 1.0×**: it
ends with two full-grid readbacks costing O(cells) in both modes, while the
flood only reaches cells with a non-decreasing path to an ocean — a fraction
that collapses from 11.99% at 4K cells to 0.38% at 4.19M. Amdahl's law erases
the accelerator's contribution.

The sweep also validates the `kStackDepth = kMaxCells` sizing choice: the worst
case (`AllLand`) peaks at 32,641 on a 256×256 grid against a 65,536 capacity,
tracking roughly `cells / 2`. Random grids peak at 21–38 even at 4.19M cells, so
only the adversarial topologies exercise this dimension at all.

### Hardware (Vitis HLS → Vivado, Kria KV260)

First HLS synthesis numbers available ([#90](../../issues/90)); Vivado
timing closure is done ([#66](../../issues/66)). Resource utilization and
on-board measurement are still pending.

| Metric | Source | Value | Issue |
|---|---|---:|---|
| Latency (cycles, min/avg/max) | Vitis HLS cosim (`dfs_accel_cosim.rpt`) | 128 / 1,381 / 10,811 | [#90](../../issues/90) |
| Initiation interval (II, worst case) | Vitis HLS `csynth.rpt` | 16 (`VITIS_LOOP_465_4`) | [#90](../../issues/90) |
| LUT | Vivado utilization | _pending_ | [#65](../../issues/65) |
| FF | Vivado utilization | _pending_ | [#65](../../issues/65) |
| BRAM | Vivado utilization | _pending_ | [#65](../../issues/65) |
| DSP | Vivado utilization | _pending_ | [#65](../../issues/65) |
| Fmax (MHz) | Vivado timing closure | 204 | [#66](../../issues/66) |
| Sustained throughput (OP/s) | on-board (PYNQ) | _pending_ | [#91](../../issues/91) |

> HLS-estimated resources (pre-Vivado): BRAM 66 (22%), DSP 50 (4%), FF 8,601
> (3%), LUT 23,473 (20%); estimated Fmax 285.71 MHz. Not all loop-level II
> targets were met during synthesis (`VITIS_LOOP_465_4` and `VITIS_LOOP_338_4`
> both settle at II=16 instead of the target II=1, due to a memory dependency
> on the burst-read loop). Full detail in
> [`src/hls/reports/csynth.rpt`](src/hls/reports/csynth.rpt) and
> [`src/hls/reports/dfs_accel_cosim.rpt`](src/hls/reports/dfs_accel_cosim.rpt).
>
> Vivado timing closure: the 250 MHz target inherited from HLS does NOT
> close post-implementation (WNS -0.909 ns) -- the HLS-estimated Fmax above
> was optimistic, as expected pre-place-and-route. 204 MHz is the Fmax
> confirmed by a full re-implementation at that frequency (WNS +0.084 ns),
> not just an extrapolation from the 250 MHz slack. That margin is thin
> (~2%), so it's reported here as the demonstrated maximum, not as the
> operating frequency: on-board bring-up ([#67](../../issues/67)) targets
> 200 MHz instead, for real timing margin. Full detail in
> [`src/vivado/reports/fmax_summary.txt`](src/vivado/reports/fmax_summary.txt),
> [`src/vivado/reports/timing_summary.rpt`](src/vivado/reports/timing_summary.rpt) (250 MHz),
> and [`src/vivado/reports/timing_summary_204mhz.rpt`](src/vivado/reports/timing_summary_204mhz.rpt) (204 MHz).

### Model vs hardware

Not yet measured. Validates the cost model above against real cycles
([#92](../../issues/92)).

| Problem (variant) | Speedup (modelled) | Speedup (measured) | Relative error |
|---|---:|---:|---:|
| Number of Islands | 1.65× | _pending_ | _pending_ |
| Unique Paths III | 1.48× | _pending_ | _pending_ |
| Word Search II | 1.54× | _pending_ | _pending_ |
| Word Search II (no pruning) | 1.61× | _pending_ | _pending_ |
| Pacific Atlantic | 1.41× | _pending_ | _pending_ |
| Longest Increasing Path | 4.00× | _pending_ | _pending_ |
| Longest Increasing Path (no memo) | 4.00× | _pending_ | _pending_ |
| **Aggregate** | **1.56×** | _pending_ | _pending_ |

---

## AI-Assisted Development

See the declaration included in the paper ([docs/paper/main.tex](docs/paper/main.tex)).

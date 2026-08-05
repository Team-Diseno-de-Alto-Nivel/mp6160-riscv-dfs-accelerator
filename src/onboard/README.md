# On-board validation (issue #67)

Reproducible recipe to get the DFS accelerator running on the KV260 and
validate it against the 21 golden cases. Not a full FPGA setup guide --
that's issue #51 (DOC-1). This just covers the artifacts in this directory.

**Status: working.** `validate_cynq.cpp`, built against
[CYNQ](https://github.com/ECASLab/cynq), gets **21/21 cases matching the
golden result on real hardware**. PYNQ (this directory's original plan --
see below) is blocked on this board; CYNQ is the path that actually works
here. `benchmark_cynq.cpp` (issue #91, sustained throughput) reuses the same
setup -- see "Throughput benchmark" below. **Also confirmed working: 21/21
cases pass, ~64,362 OP/s average (15,248-81,400 range across cases).**

## What's here

| File | From | Purpose |
|---|---|---|
| `validate_cynq.cpp` | hand-written | **The working on-board validator.** Loads `dfs_system.bit` via CYNQ, runs all 21 cases from `cases.json`, compares against the golden results. No JSON library dependency -- see its own header comment. |
| `benchmark_cynq.cpp` | hand-written | **Sustained-throughput benchmark (issue #91).** Same setup as `validate_cynq.cpp`, but times warm-up + repeated `Start()`/`Sync()` per case with a PS-side clock and writes a `results/metrics.csv`-compatible CSV with real latency/OP/s. See "Throughput benchmark" below. |
| `dfs_accel_case_io.h` | hand-written (extracted from `validate_cynq.cpp`) | The `cases.json` parser, register map, and result layout shared by both `validate_cynq.cpp` and `benchmark_cynq.cpp`. |
| `cases.json` | `make onboard-export-cases` | The 21 cases, packed exactly as `dfs_accel()` expects -- versioned in git (see its own generation comment for why) |
| `dfs_system.bit` / `dfs_system.hwh` | `make vivado-bitstream` | The bitstream + hardware handoff. **Not** in git -- gitignored like the rest of `src/vivado/dfs_system/`, only exists on a machine with Vivado |
| `driver.py` / `validate.ipynb` | hand-written | The original PYNQ-based attempt. **Currently blocked** (see below) -- kept as-is, ready to use if the blocker ever clears, not deleted since none of it is wrong, just unusable on this board today. |

## End to end

```bash
make vivado-bitstream                             # needs Vivado on PATH -- generates dfs_system.bit/.hwh (200 MHz)
make onboard-export-cases                         # no Vivado/Vitis needed -- (re)generates cases.json
make onboard-deploy KV260_HOST=<your-ssh-alias>   # copies bit/hwh/cases.json/driver.py/validate.ipynb/validate_cynq.cpp/benchmark_cynq.cpp/dfs_accel_case_io.h to the board
```

### One-time setup on the board: installing CYNQ

CYNQ isn't preinstalled -- build it once per board (it's a system-wide
install via `ninja install`, so this only needs doing once, not per
session):

```bash
ssh <alias> 'sudo apt install -y meson ninja-build'   # usually already present
ssh <alias> 'git clone https://github.com/ECASLab/cynq.git && cd cynq && \
    meson builddir -Dbuild-docs=false -Ddeveloper-mode=false -Dcpp_args="-I/usr/include/xrt" && \
    ninja -C builddir && sudo ninja -C builddir install'
```

The `-Dcpp_args="-I/usr/include/xrt"` works around a known packaging bug
in this XRT build's `pkgconfig` file (documented in CYNQ's own
`docs/Installation.md`).

**Known gotcha #1:** `ninja install` does not install all of CYNQ's
headers (`cynq/debug.hpp` and the `mmio/`/`ultrascale/`/`xrt/`/`dma/`
subdirectories are missing from `/usr/local/include/cynq` after install --
looks like an incomplete list in CYNQ's own `meson.build`). Workaround:
compile against the cloned source tree's headers directly instead of the
installed ones:

```bash
ssh <alias> 'cd dfs_accel && g++ -std=c++17 validate_cynq.cpp \
    -I ~/cynq/include $(pkg-config --cflags --libs cynq) -I/usr/include/xrt \
    -o validate_cynq'
```

### Running it

```bash
ssh -t <alias> 'cd dfs_accel && sudo ./validate_cynq dfs_system.bit cases.json'
```

`sudo` is required -- CYNQ reconfigures the PL directly (not through
`xmutil`), which needs root. Expected output: a table of all 21 cases
followed by `21/21 cases match the golden result (on-board)`.

**Known gotcha #2:** don't use `MemoryType::Dual` with
`IDataMover::Upload()`/`Download()` for this design -- it hangs
indefinitely (reproduced: got stuck at the upload step, no error, no
timeout). This design has no dedicated DMA IP (the HLS kernel's `m_axi`
ports go straight through `smartconnect` to the PS HP port -- see
`src/vivado/scripts/build_bd.tcl`), and `Dual`+`Upload`/`Download` appears
to assume a real DMA IP is present at the address passed to
`GetDataMover()`. Fix: allocate with `MemoryType::Cacheable` and call
`IMemory::Sync(SyncType::HostToDevice/DeviceToHost)` directly on the
buffer instead -- a plain cache flush/invalidate, which is all this
design actually needs since PS and PL share the same DDR.
`validate_cynq.cpp` already does this.

**Known gotcha #3:** CYNQ doesn't parse the `.hwh`, so it doesn't know
this bitstream was built for 200 MHz (`build_bitstream.tcl`, #67) -- call
`platform->SetClocks({200.f, -1.f, -1.f, -1.f})` explicitly after
`IHardware::Create()`, or the accelerator runs at whatever clock the PL
happened to be left at. `validate_cynq.cpp` already does this, and then
reads the clock back with `GetClocks()` and prints it -- don't just trust
`SetClocks()` silently succeeded, confirm it. Confirmed on real hardware:
`Clock 0 after SetClocks: 199.998 MHz` (the ~0.002 MHz gap is the same
PLL-divisor rounding seen from Vivado itself, not an error). Both
`validate_cynq.cpp` and `benchmark_cynq.cpp` do this.

## Throughput benchmark (issue #91)

`benchmark_cynq.cpp` measures sustained on-board latency/throughput for the
21 cases, with a PS-side `std::chrono::steady_clock` timer (not a new
hardware counter IP -- see its own header comment for why) bracketing only
`Start()`/`Sync()`/the result read-back, after hoisting buffer setup out of
the timed loop (per-case buffer reallocation is fine for the correctness
check `validate_cynq.cpp` exists for, but would pollute a throughput
number). Each case runs a few untimed warm-up iterations, then a batch of
timed iterations; the CSV reports min/median/max/stddev latency, with the
median used as the "sustained" figure.

Build it the same way as `validate_cynq.cpp` (gotcha #1 above), just with
the extra source file:

```bash
ssh <alias> 'cd dfs_accel && g++ -std=c++17 benchmark_cynq.cpp \
    -I ~/cynq/include $(pkg-config --cflags --libs cynq) -I/usr/include/xrt \
    -o benchmark_cynq'
```

Run it (also needs `sudo`, same reason as `validate_cynq.cpp`):

```bash
ssh -t <alias> 'cd dfs_accel && sudo ./benchmark_cynq dfs_system.bit cases.json'
```

Defaults: `benchmark_metrics.csv` output, 50 timed iterations, 5 warm-up
iterations -- override with
`./benchmark_cynq dfs_system.bit cases.json <csv_out> <iterations> <warmup>`.
Expected output: a table of all 21 cases with median latency and sustained
OP/s, `21/21 cases passed (on-board benchmark)`, and
`Wrote benchmark_metrics.csv (21 rows)`.

Bring the CSV back to the repo with:

```bash
make onboard-fetch-metrics KV260_HOST=<your-ssh-alias>   # -> results/onboard_metrics.csv
```

`results/onboard_metrics.csv` matches the base 11-column schema of
`results/metrics.csv` (`docs/guides/metrics-schema.md`) plus hardware
columns, so `scripts/plot_results.py results/onboard_metrics.csv` reads it
unmodified. The case to look at first is `noi_all_water`: its cost-model
latency is 0 ns (no DFS work), but on-board it costs the full grid sweep --
that gap is the expected finding from #92 (cost model vs. measured
hardware), not a bug.

## Scale sensitivity and overhead breakdown (issue #92 follow-up)

Two follow-up questions once #92 found the cost model badly underestimates
real latency: does the gap shrink on bigger problems, and which part of an
invocation is actually expensive? See the README's "Model vs hardware"
section for the numbers; this section is only the on-board tooling.

**Bigger cases.** `make onboard-export-cases-small` (Makefile) exports
`cases_small.json`: the 21 legacy cases plus 21 generated 64x64 ones (#109's
tier `small`, the largest the kernel supports -- `kMaxRows`/`kMaxCols` in
`dfs_accel.h`). 5 `lip_rand_64_*` cases fail packing (their generated
heights don't fit `pack_grid`'s expected range) and are skipped; the target
still writes the other 42/47. Deploy and run it exactly like `cases.json`
above, just pointing at the new file:

```bash
ssh -t <alias> 'cd dfs_accel && sudo ./benchmark_cynq dfs_system.bit cases_small.json benchmark_metrics_small.csv'
```

**Overhead breakdown.** `benchmark_batch_cynq.cpp` repeats a single named
case (positional argument, e.g. `noi_rand_64_s1`) 50 times and reports three
numbers: the per-call latency measured the normal way; the same, but
skipping the result DMA read-back on every call except the last (isolates
whether batching read-backs helps -- it doesn't, ~1% difference); and a
per-step breakdown of `Start()` vs.\ `Sync()` vs.\ the DMA read-back, since
`Sync()` alone accounts for ~97% of the time (expected -- that's where the
actual computation happens, not just interrupt overhead). Build and run it
the same way as `benchmark_cynq.cpp`:

```bash
ssh -t <alias> 'cd dfs_accel && g++ -std=c++17 benchmark_batch_cynq.cpp \
    -I ~/cynq/include $(pkg-config --cflags --libs cynq) -I/usr/include/xrt \
    -o benchmark_batch_cynq'
ssh -t <alias> 'cd dfs_accel && sudo ./benchmark_batch_cynq dfs_system.bit cases_small.json noi_rand_64_s1'
```

No `make` target pulls either CSV back automatically (unlike
`onboard-fetch-metrics`) -- both are exploratory, one-off diagnostics, not
part of the regular metrics pipeline; copy them back with a plain `scp` if
you want to keep the raw numbers.

## About `KV260_HOST`

This board sits behind a jump host and its own SSH login on a shared lab
cluster -- host/user/auth are not stable across sessions, so
`KV260_HOST` has no default in the Makefile (a wrong silent default risks
copying files to the wrong machine). Set up an SSH config alias once,
locally, instead of passing raw connection details every time:

```
# ~/.ssh/config (NEVER commit this file -- it's personal, not part of the repo)
Host kria-proxy
    HostName <proxy-ip>
    Port 9222
    User <proxy-user>
    IdentityFile ~/<your-key>

Host kria
    HostName <kria-ip-on-the-cluster-network>
    User ubuntu
    ProxyJump kria-proxy
```

Then `make onboard-deploy KV260_HOST=kria` just works, regardless of how many
times the underlying IP/credentials change -- only the alias's target needs
updating.

## Why PYNQ (`driver.py` / `validate.ipynb`) is blocked here

The lab's KV260 image runs Ubuntu 22.04 with XRT (`shell_type:
XRT_FLAT`), not a stock PYNQ image -- `pynq` is not installed by default.
`pip3 install --user pynq` itself succeeds (PYNQ 3.1.2, no compilation
needed), but `pynq.Overlay(...)` fails at runtime: PYNQ 3.x's device
backend on Zynq UltraScale+ boards goes through XRT, and specifically
needs `pyxrt` (XRT's Python bindings) to enumerate devices
(`pl_server/xrt_device.py`). `pyxrt` is **not present anywhere on this
board** and is **not available via `apt`** for this XRT build (checked
`apt-cache search pyxrt`, `xrt-embedded` -- the latter is just an empty
transitional package that depends on plain `xrt`, adds nothing). Building
`pyxrt` from XRT's source is possible in principle but a much bigger,
riskier undertaking on a board shared by multiple groups -- not
attempted, since CYNQ (see above) already solved the actual problem
without it.

`driver.py` / `validate.ipynb` are kept as-is in case `pyxrt` ever
becomes available on this board -- the register offsets and grid/words
packing logic they use are identical to `validate_cynq.cpp`'s (both come
from the same `xdfs_accel_hw.h`-derived layout and `cases.json`), so
reviving them later is a matter of installing `pyxrt`, not rewriting
logic.

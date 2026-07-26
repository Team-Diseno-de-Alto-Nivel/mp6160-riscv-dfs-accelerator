# Accelerator Scope & Problem Mapping

This guide fixes the scope of the DFS accelerator and maps the five benchmark
problems onto the common interface defined by **HWA-1** (#21), **HWC-2**
(#27), and **HWC-3** (#28). It complements the
[README architecture sections](../../README.md#module-organization), the
[architecture guide](architecture.md), and the paper's *Proposed Solution*
([docs/paper/](../paper/)).

> **Status:** design document (#57). Locks the scope and the primitive set
> before implementation issues (`VisitedMemory`, `DfsController`, …) build on
> top of it.

## 1. Scope

The accelerator offloads **only the DFS traversal primitives** that are common
to all five problems: visiting a cell, marking/unmarking it, and generating its
neighbours. It does **not** implement problem-specific logic.

**In scope (hardware):**

- Grid storage and read access (`GridMemory`)
- Visited-state storage, set/clear/query (`VisitedMemory`)
- Neighbour generation for 4- and 8-connectivity (`NeighborGenerator`)
- Traversal order bookkeeping via a LIFO stack (`StackManager`)
- Run control (start/stop) and result latch (`DfsController`, `ResultInterface`)

**Out of scope (stays in software, on the RISC-V core):**

- Problem-specific control flow (which cells count as a source, when to stop,
  how to combine results)
- The Word Search II trie and its prefix-pruning logic
- The Longest Increasing Path memoization table
- Counting/aggregation logic (island count, path count, matched words, etc.)

This split is what the accelerator seam in
[`harness/accelerator.h`](../../src/program/harness/accelerator.h) already
encodes: every algorithm calls only `load_grid`, `reset_visited`,
`is_visited`, `mark_visited`, `unmark_visited`, and `neighbors`. Everything
else is host-side C++ that is identical whether the accelerator is
`Mode::Off` or `Mode::On`.

## 2. Common interface (recap)

| Primitive | TLM command(s) it rides on | HW module |
|---|---|---|
| `load_grid` | `LoadGrid`, `SetParams` | `GridMemory` |
| `reset_visited` | `SetParams` (implicit clear) | `VisitedMemory` |
| `is_visited` / `mark_visited` / `unmark_visited` | internal; surfaced via `ReadStatus`/`ReadResult` polling | `VisitedMemory` |
| `neighbors` | internal (computed by hardware, not a host-visible command) | `NeighborGenerator` |
| run control | `SetEnable`, `Start` | `DfsController` |
| result readback | `ReadStatus`, `ReadResult` | `ResultInterface` |

Full register layout: [`src/model/config/memory_map.h`](../../src/model/config/memory_map.h).
Full command set: [`src/model/config/tlm_protocol.h`](../../src/model/config/tlm_protocol.h).

## 3. Problem → primitive mapping

| Problem | Traversal pattern | Primitives used | Notes / hardware implications |
|---|---|---|---|
| **Number of Islands** | Iterative flood fill from every unvisited land cell | `is_visited`, `mark_visited`, `neighbors` | Baseline case — single `VisitedMemory` instance, no undo. Connectivity (4/8) is a run parameter (`PARAMS` register). |
| **Unique Paths III** | Recursive backtracking that must revisit a cell after abandoning a branch | `is_visited`, `mark_visited`, `unmark_visited`, `neighbors` | Only problem that needs **`unmark_visited`** — confirms `VisitedMemory` needs a real clear-single-bit op, not just clear-all. |
| **Word Search II** | Backtracking guided by a shared trie, pruning branches with no valid prefix | `is_visited`, `mark_visited`, `unmark_visited`, `neighbors` | Trie traversal and prefix pruning are **software-only** — the accelerator has no notion of "word" or "dictionary." Hardware sees the same mark/unmark/neighbor pattern as Unique Paths III. |
| **Longest Increasing Path** | Multi-source DFS with top-down memoization | `neighbors` (+ software-side memo table) | Memoization table (best path length per cell) lives in software — it is a *result cache*, not a visited flag, so it is not part of the offloaded primitive set. The strictly-increasing constraint is checked host-side by reading grid values. |
| **Pacific Atlantic Water Flow** | Two independent multi-source floods (from opposite borders), intersected at the end | `reset_visited`, `is_visited`, `mark_visited`, `neighbors`, called twice | Needs **two independent visited sets** run sequentially — matches the note in [`visited_memory.h`](../../src/model/modules/visited_memory.h) that `VisitedMemory` supports "multiple instances for algorithms needing more than one set." Confirms `reset_visited` must be callable mid-run, not only at power-on. |

## 4. Implications this locks in for other issues

- `VisitedMemory` must support a real **clear-single-bit** (`unmark_visited`)
  in addition to set and clear-all — driven by Unique Paths III and Word
  Search II.
- `reset_visited` must be callable **mid-run**, not only once — driven by
  Pacific Atlantic's two-flood structure.
- No hardware module needs to understand tries or memoization tables — the
  accelerator's contract stops at "cell visited/unvisited + neighbours,"
  which keeps `DfsController`'s FSM generic across all five problems.
- Neighbour generation must support both 4- and 8-connectivity as a runtime
  parameter (`PARAMS` register), not a compile-time choice — driven by Number
  of Islands' connectivity sweep.

## 5. Open questions

- **Longest Increasing Path memoization**: this doc assumes the best-path-length-
  per-cell table stays entirely in software (it's a *result cache*, not a
  visited flag, so it doesn't belong in `VisitedMemory`). Needs explicit
  confirmation from whoever implements the hardware side of this problem —
  if anyone had a different design in mind for offloading it, raise it in the
  PR review before this is merged.

## 6. Traceability to the paper

This mapping is the hardware-side justification for the *Plan de Experimento*
in [`docs/paper/main.tex`](../paper/main.tex) (Section III-A), which selects
these same five LeetCode problems specifically because they cover the
computation patterns identified in the related-work analysis: linear flood
fill, backtracking with undo, pruned backtracking, memoized multi-source
search, and dual multi-source search.

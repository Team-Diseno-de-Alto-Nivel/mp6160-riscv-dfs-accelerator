# CI/CD

Three GitHub Actions workflows support the repository. The two guard workflows
run each step to a non-zero exit code on failure, so a red check means a concrete
gate failed; the third is a manually-triggered artifact generator.

- **[build.yml](../../.github/workflows/build.yml)** — the code CI: builds the dev
  container, the SystemC model and the RISC-V program, then verifies the program
  **two ways** (RISC-V emulation and native build).
- **[paper.yml](../../.github/workflows/paper.yml)** — the paper CI: compiles the
  IEEE PDF and, on pull requests, runs an AI-writing gate.
- **[demo.yml](../../.github/workflows/demo.yml)** — the demo generator: on demand,
  records `make demo` and renders the Avance II demo video (MP4) as an artifact.

## Code CI — `build.yml`

Triggered on every push to `main`/`develop` and on every pull request. A
`paths-filter` gate runs the heavy job **only** when `src/**`, `Makefile`, or
`.devcontainer/**` changed, so doc-only changes skip it.

```mermaid
flowchart TB
    T["Trigger:<br/>push main/develop · any PR"] --> CH

    subgraph CH["job: changes"]
        F{"paths-filter<br/>src/** · Makefile · .devcontainer/**"}
    end

    F -->|no match| SKIP["build job skipped"]
    F -->|match| BUILD

    subgraph BUILD["job: build (ubuntu-latest)"]
        direction TB
        B1["Build dev container<br/>.devcontainer/Dockerfile · GHA cache"]
        B2["make → sim (SystemC) + program (RISC-V ELF)"]
        B3["Run simulation<br/>sim + program"]
        B4["Verify under qemu<br/>qemu-system-riscv64 (bare-metal)"]
        B5["Verify native build<br/>make -C src/program run-native"]
        B1 --> B2 --> B3 --> B4 --> B5
    end

    BUILD --> OK["✅ every step exit 0 → check passes"]

    B2 -. non-zero .-> FAIL["❌ check fails"]
    B4 -. any case fails → exit 1 .-> FAIL
    B5 -. any case fails → exit 1 .-> FAIL
```

### How it verifies

| Step | What it proves |
|---|---|
| Build dev container | The environment (RISC-V cross-compiler, SystemC, qemu, native toolchain) builds reproducibly |
| `make` | The SystemC model **and** the RISC-V program compile (cross-compilation works) |
| Run simulation | The SystemC executable elaborates and runs |
| **qemu emulation** | The RISC-V binary runs on a RISC-V target and all 42 case runs pass — exit 1 on any mismatch |
| **native build** | The algorithm logic is correct independent of cross-compilation — same 42 runs, exit 1 on any mismatch |

The program's `main` returns non-zero if any case fails validation, so the last two
steps are the functional test gate: the code must be correct in **both** RISC-V
emulation and a native build for the check to go green.

## Paper CI — `paper.yml`

Triggered only when `docs/paper/**` (or the workflow / AI-check script) changes.
A push to `main` builds the clean PDF; a pull request also runs the AI-writing gate
and posts an annotated report.

```mermaid
flowchart TB
    PT["Trigger: change under docs/paper/**"] --> EV{"event?"}

    EV -->|push to main| CLEAN1["Compile clean PDF<br/>(latex-action)"]
    CLEAN1 --> UP1["Upload PDF artifact"]

    EV -->|pull request| AI["Run AI-writing check<br/>scripts/ai_check.py · continue-on-error"]
    AI --> CLEAN2["Compile clean PDF"]
    CLEAN2 --> UP2["Upload clean PDF"]
    UP2 --> ANN["Compile + upload annotated PDF<br/>(AI-flagged paragraphs)"]
    ANN --> CMT["Post AI report comment on PR"]
    CMT --> GATE{"avg AI score > 0.18?"}
    GATE -->|yes| FAIL["❌ Enforce threshold → exit 1"]
    GATE -->|no| PASS["✅ check passes"]
```

### How it verifies

| Step | What it proves |
|---|---|
| Compile clean PDF | The LaTeX source builds; the PDF is uploaded as a downloadable artifact |
| AI-writing check (PR) | Runs a local detector (`desklib/ai-text-detector`, not Turnitin) over the prose |
| Post report + annotated PDF | Surfaces flagged paragraphs on the PR for review |
| Enforce threshold | Fails the PR if the average AI score exceeds **0.18** (a margin below the 20% rule) |

The AI check is a free, local proxy — no API key or secret. It computes per-paragraph
scores, comments them on the PR with an annotated PDF, and blocks merge only when the
average crosses the threshold.

## Demo video — `demo.yml`

Triggered manually (`workflow_dispatch`) from the **Actions** tab. It produces the
reproducible Avance II demo video without any hand recording.

```mermaid
flowchart TB
    DT["Trigger: workflow_dispatch (manual)"] --> D1
    D1["Build dev container<br/>(GHA cache)"] --> D2
    D2["make → build model + program"] --> D3
    D3["Install asciinema + agg + ffmpeg (runner)"] --> D4
    D4["asciinema rec → 'make demo'<br/>(scripts/demo.sh under QEMU + SystemC)"] --> D5
    D5["agg → GIF → ffmpeg → MP4"] --> D6
    D6["Upload artifact: demo-video (demo.mp4 + demo.cast)"]
```

### How it works

| Step | What it does |
|---|---|
| Build container + `make` | Same reproducible environment as `build.yml`; pre-builds so the recording is short |
| `asciinema rec` | Records `make demo` (`scripts/demo.sh`): RISC-V emulation under `qemu-system-riscv64` + the SystemC co-simulation, with on-screen captions |
| `agg` + `ffmpeg` | Renders the terminal cast to an MP4 |
| Upload artifact | Publishes `demo.mp4` (and the raw `demo.cast`) for download |

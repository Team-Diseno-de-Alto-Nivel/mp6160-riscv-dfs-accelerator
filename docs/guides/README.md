# Guides & How-Tos

Project-owned documentation: step-by-step instructions, design decisions, and usage
notes that complement the [README](../../README.md) and the paper
([docs/paper/](../paper/)).

## Index

- [architecture.md](architecture.md) — code architecture of the software baseline
  and the SystemC hardware model, with block, sequence, and layer diagrams.
- [software-pipeline.md](software-pipeline.md) — how the `src/program/` software
  baseline runs: pipeline flow diagram, steps, components, and how to build/run it.
- [ci-cd.md](ci-cd.md) — the GitHub Actions workflows (code + paper), with flow
  diagrams and how each step verifies.
- [accelerator-scope.md](accelerator-scope.md) — accelerator scope and the
  five-problem-to-primitive mapping that fixes the hardware/software boundary.
- [setup.md](setup.md) — how to reproducibly stand up the toolchain (dev
  container / Docker / Codespaces / local build) and run `make model`,
  `make program`, `make run`.
- [running-the-simulation.md](running-the-simulation.md) — correr la solución en
  simulación (SystemC) y emulación (RISC-V bajo QEMU) y cómo leer la salida.
- [running-experiments.md](running-experiments.md) — correr el runner de
  experimentos (`make experiments`) y generar el CSV, las tablas y los gráficos.
- [metrics-schema.md](metrics-schema.md) — esquema de columnas y unidades del CSV
  consolidado de métricas.
- [adding-a-dfs-algorithm.md](adding-a-dfs-algorithm.md) — cómo agregar un nuevo
  problema DFS al baseline de software.
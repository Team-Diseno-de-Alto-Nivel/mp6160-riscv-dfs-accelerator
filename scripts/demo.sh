#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

REPO="https://github.com/Team-Diseno-de-Alto-Nivel/mp6160-riscv-dfs-accelerator"
PROG_ELF="src/program/build/program"
INTEG="src/program/build-native/integration_sim"

CYAN=$'\033[1;36m'
DIM=$'\033[2m'
RST=$'\033[0m'

banner() {
    printf '\n%s== %s ==%s\n\n' "$CYAN" "$1" "$RST"
    sleep "${2:-2}"
}

build_quiet() {
    local log
    log="$(mktemp)"
    if ! make -C src/program "$1" >"$log" 2>&1; then
        echo "Build step '$1' failed:"
        cat "$log"
        rm -f "$log"
        exit 1
    fi
    rm -f "$log"
}

printf '%s' "$DIM"
[ -x "$PROG_ELF" ] || build_quiet all
[ -x "$INTEG" ] || build_quiet native
printf '%s' "$RST"

banner "Acelerador DFS en RISC-V  --  demo reproducible" 2
printf '  %s\n' "$REPO"
sleep 2

banner "Emulacion: binario RISC-V bajo QEMU (qemu-system-riscv64)" 2
timeout 90 qemu-system-riscv64 -machine virt -nographic -bios none -kernel "$PROG_ELF"
sleep 3

banner "Co-simulacion SystemC/TLM con el modelo de hardware real" 2
"$INTEG"
sleep 3

banner "Reproducible con 'make demo'  --  $REPO" 3

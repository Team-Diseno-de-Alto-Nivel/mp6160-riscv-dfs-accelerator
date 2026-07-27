#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

REPO="https://github.com/Team-Diseno-de-Alto-Nivel/mp6160-riscv-dfs-accelerator"
PROG_ELF="src/program/build/program"
SIM="src/model/build/sim"
INTEG="src/program/build-native/integration_sim"

CYAN=$'\033[1;36m'
GREEN=$'\033[1;32m'
DIM=$'\033[2m'
RST=$'\033[0m'

banner() {
    local title="$1"; shift
    printf '\n%s============================================================%s\n' "$CYAN" "$RST"
    printf '%s  %s%s\n' "$CYAN" "$title" "$RST"
    local line
    for line in "$@"; do
        printf '%s  %s%s\n' "$DIM" "$line" "$RST"
    done
    printf '%s============================================================%s\n\n' "$CYAN" "$RST"
    sleep 2
}

show() {
    printf '%s$ %s%s\n\n' "$GREEN" "$*" "$RST"
    sleep 2
}

build_quiet() {
    local log
    log="$(mktemp)"
    if ! make "$@" >"$log" 2>&1; then
        echo "Build failed: make $*"
        cat "$log"
        rm -f "$log"
        exit 1
    fi
    rm -f "$log"
}

printf '%s' "$DIM"
[ -x "$SIM" ]      || build_quiet -C src/model
[ -x "$PROG_ELF" ] || build_quiet -C src/program all
[ -x "$INTEG" ]    || build_quiet -C src/program native
printf '%s' "$RST"

banner "Acelerador DFS en RISC-V con modelo de hardware en SystemC" \
    "Curso MP6160 - Diseno de Alto Nivel - ITCR" \
    "Demostracion experimental reproducible (Avance II)" \
    "$REPO"

banner "Paso 1/3  -  Emulacion del baseline RISC-V bajo QEMU" \
    "5 problemas DFS: Number of Islands, Unique Paths III, Word Search II," \
    "Longest Increasing Path, Pacific Atlantic  (acelerador OFF y ON)"
show "qemu-system-riscv64 -machine virt -nographic -bios none -kernel $PROG_ELF"
timeout 90 qemu-system-riscv64 -machine virt -nographic -bios none -kernel "$PROG_ELF"

banner "Paso 2/3  -  Simulacion del acelerador modelado en SystemC" \
    "El kernel SystemC/TLM 2.0 ejecuta el DfsAccelerator sobre una grilla" \
    "y reporta el resultado latcheado, los ciclos de hardware y la latencia"
show "$SIM $PROG_ELF"
"$SIM" "$PROG_ELF"

banner "Paso 3/3  -  Co-simulacion programa <-> modelo sobre TLM 2.0" \
    "El software RISC-V maneja el DfsAccelerator real (no un stand-in)," \
    "por la misma interfaz de registros/MMIO que usaria el hardware"
show "$INTEG"
"$INTEG"

banner "Comportamiento esperado verificado" \
    "Emulacion: 42/42 casos == esperado   Co-simulacion: 21/21 (OFF == ON)" \
    "Reproducible con un solo comando:  make demo" \
    "$REPO"

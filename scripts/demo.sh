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
    local title="$1"; shift
    printf '\n%s============================================================%s\n' "$CYAN" "$RST"
    printf '%s  %s%s\n' "$CYAN" "$title" "$RST"
    local line
    for line in "$@"; do
        printf '%s  %s%s\n' "$DIM" "$line" "$RST"
    done
    printf '%s============================================================%s\n\n' "$CYAN" "$RST"
    sleep 3
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

banner "Acelerador DFS en RISC-V con modelo de hardware en SystemC" \
    "Curso MP6160 - Diseno de Alto Nivel - ITCR" \
    "Demostracion experimental reproducible (Avance II)" \
    "$REPO"

banner "Prueba 1/2  -  Emulacion del baseline RISC-V bajo QEMU" \
    "5 problemas DFS: Number of Islands, Unique Paths III, Word Search II," \
    "Longest Increasing Path, Pacific Atlantic" \
    "Cada caso corre con el acelerador OFF (software) y ON (modelado)"
timeout 90 qemu-system-riscv64 -machine virt -nographic -bios none -kernel "$PROG_ELF"

banner "Comportamiento esperado: resultado == esperado en los 42 casos" \
    "OFF y ON dan el mismo resultado; ON reduce la latencia (speedup 1.3x - 4x)"

banner "Prueba 2/2  -  Co-simulacion sobre el modelo de hardware real" \
    "El software RISC-V maneja el DfsAccelerator (SystemC/TLM 2.0) real," \
    "no un stand-in, por la misma interfaz de registros del hardware"
"$INTEG"

banner "INTEGRATION PASSED: el hardware coincide con el baseline" \
    "21 casos verificados (registros 6/6, primitivas 42/42, OFF vs ON MATCH)" \
    "Reproducible con un solo comando:  make demo" \
    "$REPO"

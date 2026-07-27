# Correr la simulación (`make run`, `run-emu`, `run-native`, `integration`)

Cómo ejecutar la solución en **simulación** (modelo SystemC) y **emulación**
(binario RISC-V bajo qemu), y cómo leer la salida. Todos los comandos corren
dentro del dev container — ver [setup.md](setup.md) para levantar el entorno.

## Resumen de rutas

| Comando | Qué corre | Sim / Emu | Requiere |
|---|---|---|---|
| `make run-emu` | El programa DFS (ELF RISC-V bare-metal) bajo `qemu-system-riscv64` (máquina `virt`) | Emulación | qemu-system |
| `make run-native` | El mismo programa compilado nativo (iterar sin qemu) | — | g++ |
| `make run` | El modelo SystemC (`sim`) con el binario del programa | Simulación | SystemC |
| `make integration` | Co-simulación programa↔modelo sobre TLM 2.0 (acelerador real) | Simulación | SystemC |

Las cuatro son **reproducibles**: mismos datasets embebidos, mismo resultado en
cada corrida. Un fallo de validación devuelve **exit ≠ 0**, así que sirven de
suite de pruebas además de demo.

## Emulación — `make run-emu`

Compila el programa como ELF RISC-V bare-metal y lo corre en full-system bajo
`qemu-system-riscv64 -machine virt -nographic -bios none -kernel …`:

```console
$ make run-emu
```

Corre los 7 algoritmos (5 problemas + variantes *no-pruning* / *no-memo*) sobre
todos sus casos, dos veces cada uno (acelerador **OFF** y **ON**), y termina con:

```
ALL CASES PASSED (42 runs)
```

Para iterar en desarrollo sin qemu, `make run-native` produce exactamente el
mismo reporte usando un build nativo del host.

## Leer la salida

El reporte tiene tres bloques.

### 1. `=== Per-run metrics ===`

Una fila por (algoritmo × caso × modo):

```
ALGORITHM                      CASE             ACC  OK     RESULT    LAT(ns)      OPS   PEAK
number_of_islands              noi_classic_4c   OFF  ok          3        660       66      2
number_of_islands              noi_classic_4c   ON   ok          3        450       45      2
```

| Columna | Significado |
|---|---|
| `ACC` | Acelerador `OFF` (baseline software) u `ON` (acelerador modelado). |
| `OK` | `ok` si el resultado coincide con el valor esperado del caso; `FAIL` si no. |
| `RESULT` | Salida del algoritmo (p. ej. número de islas). Debe ser igual en OFF y ON. |
| `LAT(ns)` | Latencia estimada por el modelo de costo (ciclos × periodo de reloj). |
| `OPS` | Operaciones-proxy (aproximación al conteo de instrucciones). |
| `PEAK` | Profundidad pico del stack de la traversal. |

### 2. `=== Accelerator ON vs OFF ===`

El *speedup* por caso (`OFF latency / ON latency`) — la evidencia de la ventaja
del acelerador:

```
ALGORITHM                      CASE                  OFF(ns)       ON(ns)  SPEEDUP
number_of_islands              noi_classic_4c            660          450    1.47x
longest_increasing_path        lip_desc                  360           90    4.00x
```

### 3. `=== CSV ===`

El mismo dato en CSV (una fila por corrida) para post-procesarlo — es lo que el
runner de experimentos extrae. El esquema de columnas está en
[metrics-schema.md](metrics-schema.md).

La última línea, `ALL CASES PASSED (42 runs)`, es el veredicto global; el
proceso sale con código 0 solo si **todos** los casos pasan.

## Simulación SystemC — `make run`

Corre el modelo de hardware standalone (`sim`) sobre una grilla de demostración
de 3×3 cargada por la interfaz de registros/MMIO, y reporta el resultado latcheado
por el hardware:

```console
$ make run
...
result=9 visited=9 peak_stack=13
```

Esto ejercita el `DfsAccelerator` real (FSM de control, generador de vecinos,
stack manager, memorias) elaborado en SystemC, no el baseline de software.

## Co-simulación — `make integration`

La ruta más completa: corre los 5 problemas contra el **acelerador SystemC real**
(no un stand-in) sobre la misma interfaz TLM 2.0 de registros que usaría el
hardware, con el acelerador OFF y ON, y hace cross-check de que ambos coinciden
con el baseline:

```console
$ make integration
...
INTEGRATION PASSED
```

Verifica la ruta de registros (6/6 casos de *number of islands*), la ruta de
primitivas fina (42/42 corridas de los 5 algoritmos) y el cruce OFF vs ON
(MATCH en los 21 casos). Es lo que respalda los resultados preliminares del
paper.

## Ver también

- [running-experiments.md](running-experiments.md) — recolectar el CSV y generar
  tablas y gráficos a partir de estas corridas.
- [software-pipeline.md](software-pipeline.md) — cómo está construido el pipeline
  de software por dentro.

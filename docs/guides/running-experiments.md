# Correr los experimentos (`make experiments`)

Cómo correr el runner de experimentos que ejecuta el baseline vs. el acelerador
(OFF/ON), recolecta el CSV consolidado, y genera las tablas y gráficos que
alimentan el *Results and Analysis* del paper. Todo corre dentro del dev
container — ver [setup.md](setup.md).

## Un comando

```console
$ make experiments
```

Equivale a correr `scripts/run_experiments.sh` seguido de
`scripts/plot_results.py`.

## Qué hace, paso a paso

1. **Compila** el programa (build nativo) en `src/program/build-native/`.
2. **Corre** los 7 algoritmos sobre todos sus casos, dos veces cada uno
   (acelerador OFF y ON), y guarda el reporte completo en `results/run.log`.
3. **Extrae** el bloque CSV del reporte a `results/metrics.csv` (una fila por
   caso × modo).
4. Si el `integration_sim` (SystemC) está disponible, corre además la
   **co-simulación** programa↔modelo sobre TLM y guarda `results/integration.log`
   + `results/integration_metrics.csv`.
5. **Genera** tablas y gráficos a partir de `results/metrics.csv`.

## Salidas (bajo `results/`)

| Archivo | Contenido |
|---|---|
| `results/run.log` | Reporte completo del programa (tabla por corrida, speedup, CSV). |
| `results/metrics.csv` | Métricas consolidadas, una fila por (caso × modo). Ver [metrics-schema.md](metrics-schema.md). |
| `results/integration.log` | Reporte de la co-simulación (si se corrió). |
| `results/integration_metrics.csv` | Métricas de la co-simulación (si se corrió). |
| `results/tables/speedup.md` | Tabla markdown (latencia OFF vs ON + speedup + match). |
| `results/tables/speedup.tex` | La misma tabla en LaTeX `booktabs`, para pegar en el paper. |
| `results/plots/speedup.png` | Gráfico de barras de speedup por caso. |
| `results/plots/latency.png` | Barras agrupadas de latencia OFF vs ON por caso. |

`results/` está en `.gitignore`: son artefactos generados, no se commitean.

## Requisitos

- Las **tablas** (`.md` / `.tex`) usan solo la librería estándar de Python, así
  que siempre se producen.
- Los **gráficos** (`.png`) requieren `matplotlib`. Si no está instalado, el
  script lo informa y omite los PNG sin fallar (las tablas igual salen).

## Regenerar solo las tablas/gráficos

Si ya tenés `results/metrics.csv` y solo querés rehacer las tablas y plots:

```console
$ python3 scripts/plot_results.py
```

Toma `results/metrics.csv` por defecto, o podés pasarle otra ruta de CSV como
argumento.

## En CI

El workflow [`build.yml`](../../.github/workflows/build.yml) corre este mismo
runner en cada push/PR que toca `src/**` y publica el directorio `results/`
(CSV, tablas y gráficos) como artefacto descargable. Ver
[ci-cd.md](ci-cd.md).

## Ver también

- [running-the-simulation.md](running-the-simulation.md) — correr una sola pasada
  y leer la salida en detalle.
- [metrics-schema.md](metrics-schema.md) — esquema de columnas y unidades del CSV.

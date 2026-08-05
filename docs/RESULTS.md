# Raspberry Pi 3 Results

This file is intentionally a report template until measurements have been made
on the physical target. Do not populate it with emulated or desktop timings.
Document device preparation, collection, validation, and result transfer with
the published data so the experiment remains reproducible.

## Experimental platform

Copy or link the `metadata-*.txt` sidecar and record:

- Raspberry Pi revision, RAM, cooling, power supply, and enclosure;
- OS image, kernel, compiler version, and complete build target;
- CPU governor and observed frequency range;
- ambient/start/end temperatures and `vcgencmd get_throttled` state;
- `perf` version, permissions, and unavailable PMU events.

## Hypotheses

1. SoA will reduce cross-cell stride and enable vectorization, but layout alone
   may not outperform AoS at every cache-resident size.
2. Auto-vectorized and NEON kernels will reduce cycles per update on one core.
3. SIMD gains will shrink as the working set moves beyond shared L2 and memory
   bandwidth becomes dominant.
4. Four-core speedup will be sublinear and may plateau for the largest grid.

## Correctness

| Kernel / threads | Maximum reference difference | Mass error | Velocity L2 error | Finite |
|---|---:|---:|---:|---:|
| AoS / 1 | reference | | | |
| SoA / 1 | | | | |
| auto / 1 | | | | |
| NEON / 1 | | | | |
| NEON / 4 | | | | |

State the tolerance before examining performance. Explain the ARMv7 reciprocal
rounding difference if it is measurable.

## Single-core results

Report median and median absolute deviation (or another declared dispersion
measure), not only the minimum.

| Grid | Working set | Kernel | Median MLUPS | MAD | ns/update | Speedup vs AoS |
|---:|---:|---|---:|---:|---:|---:|
| | | | | | | |

Add `single-core.png`, then connect changes to layout, compiler reports,
generated assembly, cycles/update, instructions/update, IPC, and cache events.
Do not infer L1 behavior from generic `cache-misses`.

## Multicore scaling

| Threads | Median MLUPS | Speedup | Parallel efficiency | Temperature range |
|---:|---:|---:|---:|---:|
| 1 | | 1.00 | 1.00 | |
| 2 | | | | |
| 3 | | | | |
| 4 | | | | |

Add `multicore.png`. Discuss barrier overhead for small grids and shared-memory
bandwidth saturation for large grids. Check frequency and throttling evidence
before attributing a plateau to bandwidth.

## Conclusions and threats to validity

Summarize which hypotheses survived. Include counter multiplexing, background
OS activity, temperature/frequency variation, PMU availability, compiler
specificity, estimated versus measured traffic, and the limits of one board.

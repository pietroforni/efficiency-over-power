# Benchmark results

This directory contains the accepted measurements from the Raspberry Pi 3
experiment.

The reference run is stored in
[`pi3/reference-2026-08-06/`](pi3/reference-2026-08-06/). It contains 240
finite benchmark samples collected with active fan cooling and no firmware
throttling.

| Artifact | Purpose |
|---|---|
| `benchmark-*.csv` | Raw timing, temperature, frequency, and correctness data |
| `metadata-*.txt` | Hardware, operating system, compiler, governor, and thermal state |
| `perf-*.csv` | Hardware performance-counter profile |
| `vectorization-auto-*.txt` | Strict and relaxed compiler-vectorization evidence |
| `plots/summary.csv` | Derived medians, MAD, speedups, and parallel efficiency |
| `plots/*.png` | Single-core and multicore figures |

The raw benchmark data and metadata are preserved unchanged. See the
[complete results report](../docs/RESULTS.md) for the methodology,
interpretation, and limitations.

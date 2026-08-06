# Raspberry Pi 3 Results

Accepted raw data, vectorization diagnostics, PMU profiles, and derived plots
are in `results/pi3/reference-2026-08-06/`. The earlier ice-cooled run was
rejected because it reached the Pi 3 B+'s soft thermal limit; this reference
run used active fan cooling.

## Experimental platform

- Raspberry Pi 3 Model B Plus Rev 1.3; 32-bit ARMv7 Raspberry Pi OS, Linux
  `6.12.75+rpt-rpi-v7`.
- GCC 14.2.0; `rpi3-armv7-neon-O3` build
  (`-mcpu=cortex-a53 -mfpu=neon-vfpv4 -mfloat-abi=hard`).
- `ondemand` governor with a configured 600 MHz--1.4 GHz range.
- The accepted run started at 38.6 C and ended at 53.7 C. Per-sample maximum
  was 54.8 C; `vcgencmd get_throttled` was `0x0` both before and after.

## Correctness and compiler diagnostics

The Pi test suite passed before collection. Cross-kernel test tolerances are
2e-6 for SoA/auto and 1e-5 for NEON, accounting for the ARMv7 reciprocal.
The 512x512, 500-step benchmark diagnostics were finite for every sample.

| Kernel / threads | Mass relative error | Velocity L2 error |
|---|---:|---:|
| AoS / 1 | 3.73e-6 | 2.1333e-2 |
| SoA / 1 | 3.73e-6 | 2.1333e-2 |
| auto / 1 | 3.73e-6 | 2.1333e-2 |
| NEON / 1 or 4 | 6.67e-6 | 2.1332e-2 |

`vectorization.txt` shows that strict GCC 14 did not vectorize the auto
kernel's floating-point interior loop. The separately labeled relaxed-math
diagnostic does vectorize it with 16-byte and 8-byte vectors; it is excluded
from the primary results.

## Performance

Values are median MLUPS with MAD in parentheses, from ten samples per primary
kernel/grid. The NEON 512x512 one-thread point combines the equivalent
single-core and scaling samples (20 total).

| Grid | AoS | SoA | auto | NEON | NEON / AoS |
|---:|---:|---:|---:|---:|---:|
| 16x16 | 4.315 (0.006) | 5.429 (0.010) | 6.451 (0.010) | 7.270 (0.023) | 1.68x |
| 64x64 | 4.388 (0.001) | 3.671 (0.067) | 4.281 (0.044) | 7.482 (0.023) | 1.71x |
| 128x128 | 4.106 (0.007) | 2.741 (0.033) | 3.315 (0.065) | 4.919 (0.031) | 1.20x |
| 256x256 | 4.049 (0.003) | 2.530 (0.068) | 2.950 (0.074) | 4.724 (0.021) | 1.17x |
| 512x512 | 3.998 (0.002) | 1.894 (0.012) | 2.249 (0.011) | 4.198 (0.040) | 1.05x |

The derived plots are `plots/single-core.png` and `plots/multicore.png`; the
full machine-readable statistics are in `plots/summary.csv`.

## Multicore NEON scaling at 512x512

| Threads | Median MLUPS | Speedup vs 1 thread | Parallel efficiency | Temperature range |
|---:|---:|---:|---:|---:|
| 1 | 4.198 | 1.00x | 100.0% | 44.0--45.1 C |
| 2 | 7.954 | 1.89x | 94.7% | 45.1--48.3 C |
| 3 | 11.186 | 2.66x | 88.8% | 48.3--52.1 C |
| 4 | 13.593 | 3.24x | 81.0% | 51.0--54.8 C |

One PMU profile of the one-thread NEON 512x512 case recorded 45.30 billion
cycles, 13.19 billion instructions, IPC 0.29, and a reported 16.64% miss rate
for both generic `cache-misses` and `L1-dcache-load-misses`. On this PMU those
generic and L1 event counts were identical, so the generic event must not be
interpreted as an independent cache level.

## Conclusions and threats to validity

Explicit NEON was fastest at every grid size. Its advantage over AoS shrank
from 1.68x at 16x16 to 1.05x at 512x512, consistent with a bandwidth-limited
larger working set. The strict auto kernel was not vectorized by GCC 14 and was
slower than AoS beyond 16x16; relaxed math vectorization is a separately
labeled compiler-sensitivity result, not part of this comparison. Four-core
scaling was sublinear but strong at 3.24x, consistent with shared-memory
bandwidth and synchronization costs.

This is one actively cooled board under an `ondemand` governor. Frequency
transitions, background OS activity, a single PMU profile, and the dependence
on GCC 14/ARMv7 are material limitations. The earlier thermally throttled run
is intentionally excluded.

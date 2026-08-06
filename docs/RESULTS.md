# Raspberry Pi 3 measurement report

This report describes the accepted run stored in
`results/pi3/reference-2026-08-06/`. The earlier ice-cooled run was rejected
after firmware reported thermal throttling. The accepted run used a fan and
completed without throttling.

## Platform and protocol

- Raspberry Pi 3 Model B Plus Rev 1.3
- 32-bit ARMv7 Raspberry Pi OS, Linux `6.12.75+rpt-rpi-v7`
- GCC 14.2.0 with `-O3 -mcpu=cortex-a53 -mfpu=neon-vfpv4
  -mfloat-abi=hard`
- `ondemand` governor, configured from 600 MHz to 1.4 GHz
- 500 timed steps and 20 warm-up steps per sample
- CPU affinity requested for each benchmark
- ten samples per primary kernel and grid configuration
- active fan cooling, with a 55 C pre-sample temperature limit

The harness records one configuration at a time and changes the kernel order
between grid sizes. It checks the firmware throttle state before and after
each sample. Initialization, validation, and file output are not included in
the timed interval; thread creation and joining occur once per timed run.

The metadata temperature rose from 38.628 C before the sweep to 53.692 C
after it. The highest per-sample reading was 54.768 C. Firmware reported
`throttled=0x0` before and after the run.

Of the 240 samples, 238 recorded 1.4 GHz before timing. The first AoS 16x16
batch contained the two lower readings, at 700 MHz and 1.0 GHz. All 64x64 and
larger samples recorded 1.4 GHz. Frequency is sampled once before each timed
run, so it is supporting metadata rather than a complete frequency trace.

## Correctness

The test suite passed on the Pi before measurement. It compares macroscopic
density and velocity across implementations with tolerances of `2e-6` for SoA
and auto, and `1e-5` for NEON. The larger NEON tolerance accounts for the
ARMv7 reciprocal estimate and two Newton refinements.

All 240 benchmark rows were finite. At 512x512 after 500 steps:

| Kernel / threads | Relative mass error | Velocity L2 error |
|---|---:|---:|
| AoS / 1 | 3.73e-6 | 2.1333e-2 |
| SoA / 1 | 3.73e-6 | 2.1333e-2 |
| auto / 1 | 3.73e-6 | 2.1333e-2 |
| NEON / 1 or 4 | 6.67e-6 | 2.1332e-2 |

These checks show that each measured implementation remained numerically
usable for this workload. The Taylor–Green comparison is a validation
diagnostic, not a claim that the discrete weakly compressible model exactly
matches the continuum solution.

## Single-core performance

Values are median MLUPS with MAD in parentheses. The 512x512 one-thread NEON
entry combines its ten-sample primary group and the repeated ten-sample
one-thread scaling group. The other entries use ten samples.

| Grid | AoS | SoA | auto | NEON | NEON / AoS |
|---:|---:|---:|---:|---:|---:|
| 16x16 | 4.315 (0.006) | 5.429 (0.010) | 6.451 (0.010) | 7.270 (0.023) | 1.68x |
| 64x64 | 4.388 (0.001) | 3.671 (0.067) | 4.281 (0.044) | 7.482 (0.023) | 1.71x |
| 128x128 | 4.106 (0.007) | 2.741 (0.033) | 3.315 (0.065) | 4.919 (0.031) | 1.20x |
| 256x256 | 4.049 (0.003) | 2.530 (0.068) | 2.950 (0.074) | 4.724 (0.021) | 1.17x |
| 512x512 | 3.998 (0.002) | 1.894 (0.012) | 2.249 (0.011) | 4.198 (0.040) | 1.05x |

The explicit NEON implementation was fastest at all five sizes. The scalar
SoA implementation only exceeded AoS at 16x16. The `auto` implementation was
faster than scalar SoA at every size, despite the strict compiler report
showing that its interior loop was not vectorized.

## Multicore NEON scaling at 512x512

| Threads | Median MLUPS | Speedup vs one thread | Parallel efficiency | Temperature range |
|---:|---:|---:|---:|---:|
| 1 | 4.198 | 1.00x | 100.0% | 44.0--45.1 C |
| 2 | 7.954 | 1.89x | 94.7% | 45.1--48.3 C |
| 3 | 11.186 | 2.66x | 88.8% | 48.3--52.1 C |
| 4 | 13.593 | 3.24x | 81.0% | 51.0--54.8 C |

Throughput increased with every added core. Scaling was sublinear, but this
experiment does not separate synchronization cost from cache, memory, OS, or
other shared-resource effects.

## Compiler and PMU evidence

The focused compiler diagnostics are stored as
`vectorization-auto-strict.txt` and `vectorization-auto-relaxed.txt`. GCC 14
did not vectorize the strict `auto` interior loop. The archived relaxed report
used `-funsafe-math-optimizations` for the full diagnostic build and emitted
16-byte and 8-byte vector loops. It is evidence that floating-point semantics
blocked this loop, not a primary performance result. The current
`rpi3-relaxed-auto-vector-report` target limits that flag to the auto kernel
when reproducing the diagnostic.

The successful profile in `perf-20260806T114609Z.csv` measured the one-thread
NEON 512x512 case. It recorded 45.30 billion cycles, 13.19 billion retired
instructions, and 0.29 instructions per cycle. It also reported a 16.64% miss
rate for both generic cache events and the named L1 data-cache events. Because
those event counts are identical on this PMU, they must not be interpreted as
independent measurements of two cache levels.

## Conclusions and limits

The measurements support four direct conclusions: NEON was fastest in this
test; its relative gain changed substantially with grid size; SoA alone did
not guarantee higher throughput; and four cores produced a useful but
sublinear speedup. The change with size is consistent with working-set and
memory effects, while the scaling loss is consistent with synchronization and
shared-resource costs. The available experiment does not identify either
cause independently.

This is one board, compiler, operating-system image, governor, cooling setup,
and numerical workload. Kernel order changes only between grid sizes, the PMU
section uses one successful profile, and electrical energy was not measured.
The raw CSV and metadata remain the authoritative record; `plots/summary.csv`
contains the derived medians and MAD values.

# Cortex-A53 Lattice-Boltzmann Optimization Study

A dependency-free D2Q9 lattice-Boltzmann benchmark that studies data layout,
compiler vectorization, explicit ARM NEON SIMD, and multicore scaling on a
32-bit Raspberry Pi 3. The physical case is a periodic Taylor–Green vortex,
which makes numerical correctness measurable rather than relying on a timing
kernel with no scientific result.

This project deliberately does **not** compare unoptimized C with optimized
intrinsics and call the difference a SIMD speedup. Every rung changes one
primary variable:

| Kernel | Layout | Compiler vectorization | Explicit SIMD |
|---|---|---:|---:|
| `aos` | Array of Structures | disabled | no |
| `soa` | Structure of Arrays | disabled | no |
| `auto` | Structure of Arrays | enabled | no |
| `neon` | Structure of Arrays | intrinsic implementation | ARM NEON |

The `--threads` option applies static row decomposition to a kernel without
including thread creation in every lattice step. The main scaling experiment
uses `neon` with one through four threads.

## Build and test

The scalar implementation is portable C11. On non-ARM machines the NEON
kernel is reported as unavailable and skipped. AArch64 hosts with NEON can run
it as a correctness convenience, but publishable timings for this study must
come from the physical Raspberry Pi 3.

```sh
make
make test
./build/lbm_bench --kernel all --nx 128 --ny 128 \
  --steps 500 --warmup 20 --repetitions 10 --threads 1
```

For 32-bit Raspberry Pi OS with GCC:

```sh
make rpi3
./build/lbm_bench --kernel neon --nx 512 --ny 512 \
  --steps 500 --warmup 20 --repetitions 10 --threads 4 --pin
```

`make rpi3` adds `-mcpu=cortex-a53 -mfpu=neon-vfpv4
-mfloat-abi=hard`. Confirm that the OS, compiler, and installed userland are
actually 32-bit before using that target. `make vector-report` rebuilds with
GCC or Clang vectorization diagnostics; the report should identify the
interior x-loop in `kernel_soa_auto.c` as vectorized. `make unoptimized` exists
only for a separately labeled compiler-sensitivity appendix; it is not part of
the layout or SIMD speedup calculation.

Run `./build/lbm_bench --help` for the complete CLI. CSV is written to stdout,
or written to a result file with `--output FILE`. Allocation,
initialization, validation, and CSV output are outside the timed region.

## Reproduce the study

The core measurement commands are:

```sh
sh scripts/run_tests.sh
sh scripts/profile_perf.sh
uv sync
uv run python scripts/plot_results.py results/benchmark-*.csv
```

Run these only after the Raspberry Pi build, tests, and NEON smoke check pass.
`run_tests.sh` records timing CSV and a before/after metadata sidecar, rotates
kernel order, and waits above the configured thermal threshold. It never
changes privileged system settings.

The `perf` wrapper probes events by attempting a minimal measurement before
using them. This matters on ARM, where kernel/PMU combinations do not expose a
uniform counter set. Generic `cache-misses` is **not** presented as an L1 miss
counter. If `L1-dcache-load-misses` is unsupported, the result says so instead
of substituting a differently scoped counter.

Useful reported quantities are:

- MLUPS (millions of lattice updates per second) and ns/update;
- estimated traffic of 72 bytes/update (nine float loads and stores), clearly
  labeled as an algorithmic estimate rather than measured DRAM traffic;
- cycles, retired instructions, IPC, and supported cache events from `perf`;
- mass-conservation error, Taylor–Green velocity error, and a checksum that
  keeps the numerical result observable.

Preserve every raw sample. Store accepted Raspberry Pi data under a stable
`results/pi3/reference-YYYY-MM-DD/` directory alongside its derived plots.

## Numerical model

The code uses a single-relaxation-time D2Q9 BGK operator in lattice units:

```text
f_q(x + c_q, t + 1) = f_q(x, t) + omega (f_q^eq - f_q)
omega = 1 / (3 nu + 1/2)
```

Streaming and collision are fused using a pull scheme and two 64-byte-aligned
distribution buffers. Periodic boundaries avoid branch-heavy wall rules in
the timed kernel. The initial divergence-free Taylor–Green velocity decays as
`exp(-nu (kx^2 + ky^2) t)`. The benchmark reports its relative L2 velocity
error, but this analytical comparison is a validation diagnostic—not a claim
that the discrete weakly-compressible LBM is the exact continuum solution.

Default velocity is 0.05 lattice units to keep the Mach number modest. The
primary study does not use `-ffast-math`; relaxed floating-point semantics can
be investigated only as a separately labeled experiment.

## Interpreting results

Two complete distribution grids require approximately `72 * nx * ny` bytes.
The default sweep therefore includes working sets that fit roughly within L1,
within the shared L2, and well beyond L2. Exact effective capacities and cache
competition depend on the kernel and all concurrently resident data.

LBM has low arithmetic intensity and can become bandwidth-bound. Consequently,
manual NEON need not beat compiler vectorization substantially, and four cores
need not achieve fourfold speedup. A plateau accompanied by improved IPC but
unchanged MLUPS is useful evidence of a memory bottleneck, not a failed result.

See [docs/RESULTS.md](docs/RESULTS.md) for the report checklist and tables to
fill using physical-device measurements.

## Repository map

```text
include/             Public solver and benchmark interfaces
src/                 Common model, four kernels, threading, CLI, telemetry
tests/                Numerical and cross-kernel correctness tests
scripts/              Reproducible sweeps, system metadata, perf, plotting
docs/RESULTS.md       Measurement report template
results/README.md     Raw and derived result storage policy
.github/workflows/    Portable GCC/Clang correctness CI
```

Runtime requirements are libc, libm, POSIX threads, and Linux `perf` for PMU
measurements. Plotting is deliberately an optional development dependency.

## Limitations

- The ARMv7 NEON reciprocal uses two Newton refinements, so its rounding may
  differ slightly from scalar division. Tests compare macroscopic fields with
  an explicit tolerance.
- A `perf` event can be absent or inaccessible because of kernel configuration
  or `perf_event_paranoid`; the program does not attempt to change permissions.
- Thermal readings are unavailable on non-Linux development hosts and appear
  as `-1` in CSV.
- This first release studies single-precision BGK and periodic Taylor–Green
  flow. Boundary-heavy geometries, MRT collision, MPI, and GPU backends are out
  of scope.

Licensed under the MIT License.

#!/usr/bin/env python3
"""Generate median-performance and scaling plots from benchmark CSV files."""

import argparse
import csv
import statistics
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt


def read_rows(paths):
    rows = []
    for path in paths:
        with path.open(newline="") as stream:
            rows.extend(csv.DictReader(stream))
    return rows


def median_groups(rows, keys, value):
    groups = defaultdict(list)
    for row in rows:
        groups[tuple(row[key] for key in keys)].append(float(row[value]))
    return {key: statistics.median(values) for key, values in groups.items()}


def write_summary(rows, path):
    groups = defaultdict(list)
    for row in rows:
        key = (row["kernel"], int(row["nx"]), int(row["ny"]),
               int(row["threads"]))
        groups[key].append(float(row["mlups"]))
    baselines = {}
    for (kernel, nx, ny, threads), values in groups.items():
        if kernel == "aos" and threads == 1:
            baselines[(nx, ny)] = statistics.median(values)
    with path.open("w", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(["kernel", "nx", "ny", "threads", "samples",
                         "median_mlups", "mad_mlups", "speedup_vs_aos_1t",
                         "parallel_efficiency"])
        for key in sorted(groups, key=lambda item: (item[1], item[2], item[0], item[3])):
            kernel, nx, ny, threads = key
            values = groups[key]
            median = statistics.median(values)
            mad = statistics.median(abs(value - median) for value in values)
            baseline = baselines.get((nx, ny))
            speedup = median / baseline if baseline else ""
            one_thread = groups.get((kernel, nx, ny, 1))
            one_thread_median = statistics.median(one_thread) if one_thread else None
            efficiency = median / one_thread_median / threads if one_thread_median else ""
            writer.writerow([kernel, nx, ny, threads, len(values),
                             f"{median:.9g}", f"{mad:.9g}",
                             f"{speedup:.9g}" if speedup != "" else "",
                             f"{efficiency:.9g}" if efficiency != "" else ""])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("csv", nargs="+", type=Path)
    parser.add_argument("--output-dir", type=Path, default=Path("results/plots"))
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    rows = read_rows(args.csv)
    valid = [row for row in rows if row["finite"] == "1"]
    if not valid:
        raise SystemExit("No finite benchmark samples found")
    write_summary(valid, args.output_dir / "summary.csv")

    single = [row for row in valid if row["threads"] == "1"]
    by_size = median_groups(single, ("kernel", "nx"), "mlups")
    fig, axis = plt.subplots(figsize=(8, 5))
    kernels = sorted({key[0] for key in by_size})
    for kernel in kernels:
        points = sorted((int(size), rate) for (name, size), rate in by_size.items()
                        if name == kernel)
        axis.plot([point[0] for point in points], [point[1] for point in points],
                  marker="o", label=kernel)
    axis.set(xlabel="Square lattice width", ylabel="Median MLUPS",
             title="Single-core D2Q9 performance")
    axis.set_xscale("log", base=2)
    axis.grid(True, alpha=0.3)
    axis.legend()
    fig.tight_layout()
    fig.savefig(args.output_dir / "single-core.png", dpi=180)
    plt.close(fig)

    multicore = [row for row in valid if row["kernel"] == "neon"]
    by_threads = median_groups(multicore, ("nx", "threads"), "mlups")
    fig, axis = plt.subplots(figsize=(8, 5))
    for size in sorted({key[0] for key in by_threads}, key=int):
        points = sorted((int(threads), rate)
                        for (width, threads), rate in by_threads.items()
                        if width == size)
        if len(points) > 1:
            axis.plot([point[0] for point in points], [point[1] for point in points],
                      marker="o", label=f"{size}x{size}")
    axis.set(xlabel="Threads", ylabel="Median MLUPS",
             title="NEON multicore scaling", xticks=[1, 2, 3, 4])
    axis.grid(True, alpha=0.3)
    axis.legend()
    fig.tight_layout()
    fig.savefig(args.output_dir / "multicore.png", dpi=180)
    plt.close(fig)


if __name__ == "__main__":
    main()

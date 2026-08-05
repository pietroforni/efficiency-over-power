#!/bin/sh
set -eu

BENCHMARK=${BENCHMARK:-./build/lbm_bench}
PERF_REPETITIONS=${PERF_REPETITIONS:-5}
PERF_OUTPUT=${PERF_OUTPUT:-results/perf-$(date -u +%Y%m%dT%H%M%SZ).csv}

if ! command -v perf >/dev/null 2>&1; then
    echo "perf is not installed or not on PATH" >&2
    exit 1
fi

events=""
for event in cycles instructions cache-references cache-misses L1-dcache-loads L1-dcache-load-misses; do
    if probe=$(perf stat -e "$event" -- true 2>&1 >/dev/null) &&
       ! printf '%s\n' "$probe" | awk '/<not supported>|<not counted>/ { bad=1 } END { exit !bad }'; then
        if [ -z "$events" ]; then events=$event; else events="$events,$event"; fi
    else
        echo "PMU event unavailable: $event" >&2
    fi
done

if [ -z "$events" ]; then
    echo "No requested hardware events are exposed by this kernel/PMU" >&2
    exit 1
fi

echo "Profiling events: $events" >&2
mkdir -p "$(dirname "$PERF_OUTPUT")"
echo "Writing perf counters to $PERF_OUTPUT" >&2
exec perf stat -r "$PERF_REPETITIONS" -x, -o "$PERF_OUTPUT" -e "$events" -- \
    "$BENCHMARK" --kernel neon --nx 512 --ny 512 --steps 500 \
    --warmup 20 --repetitions 1 --threads 1 --pin "$@"

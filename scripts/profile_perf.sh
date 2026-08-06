#!/bin/sh
set -eu

BENCHMARK=${BENCHMARK:-./build/lbm_bench}
# A single 512x512 sample is sufficient for counter collection; repeating it
# inside perf would bypass the benchmark harness's between-sample cooling.
PERF_REPETITIONS=${PERF_REPETITIONS:-1}
PERF_OUTPUT=${PERF_OUTPUT:-results/perf-$(date -u +%Y%m%dT%H%M%SZ).csv}
MAX_TEMP_C=${MAX_TEMP_C:-55}

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

temperature_c() {
    if [ -r /sys/class/thermal/thermal_zone0/temp ]; then
        awk '{ printf "%.1f", $1 / 1000.0 }' /sys/class/thermal/thermal_zone0/temp
    else
        printf '%s' "unavailable"
    fi
}

wait_for_temperature() {
    current=$(temperature_c)
    [ "$current" = "unavailable" ] && return 0
    while awk -v current="$current" -v maximum="$MAX_TEMP_C" 'BEGIN { exit !(current >= maximum) }'; do
        echo "SoC temperature ${current} C; waiting below ${MAX_TEMP_C} C..." >&2
        sleep 5
        current=$(temperature_c)
    done
}

check_throttling() {
    if command -v vcgencmd >/dev/null 2>&1; then
        status=$(vcgencmd get_throttled)
        if [ "$status" != "throttled=0x0" ]; then
            echo "Firmware throttling detected: $status" >&2
            echo "Discard this profile, cool the board, reboot to clear latched flags, then retry." >&2
            exit 1
        fi
    fi
}

wait_for_temperature
check_throttling
echo "Profiling events: $events" >&2
mkdir -p "$(dirname "$PERF_OUTPUT")"
echo "Writing perf counters to $PERF_OUTPUT" >&2
set +e
perf stat -r "$PERF_REPETITIONS" -x, -o "$PERF_OUTPUT" -e "$events" -- \
    "$BENCHMARK" --kernel neon --nx 512 --ny 512 --steps 500 \
    --warmup 20 --repetitions 1 --threads 1 --pin "$@"
status=$?
set -e
check_throttling
exit "$status"

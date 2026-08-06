#!/bin/sh
set -eu

BENCHMARK=${BENCHMARK:-./build/lbm_bench}
RESULT_DIR=${RESULT_DIR:-results}
SIZES=${SIZES:-"16 64 128 256 512"}
STEPS=${STEPS:-500}
REPETITIONS=${REPETITIONS:-10}
WARMUP=${WARMUP:-20}
# The Pi 3 B+ may begin soft-throttling around 60 C.  Leave headroom for a
# single timed sample, and cool between samples rather than between batches.
MAX_TEMP_C=${MAX_TEMP_C:-55}

mkdir -p "$RESULT_DIR"
STAMP=$(date -u +%Y%m%dT%H%M%SZ)
OUTPUT="$RESULT_DIR/benchmark-$STAMP.csv"
TEMP_FILE="$RESULT_DIR/.sample-$STAMP.csv"
METADATA="$RESULT_DIR/metadata-$STAMP.txt"

cleanup() {
    rm -f "$TEMP_FILE"
}
trap cleanup EXIT INT TERM

{
    echo "[before]"
    sh scripts/system_info.sh
} > "$METADATA"

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
            echo "Discard this run, cool the board, reboot to clear latched flags, then retry." >&2
            exit 1
        fi
    fi
}

append_sample() {
    wait_for_temperature
    check_throttling
    "$BENCHMARK" "$@" --warmup "$WARMUP" --repetitions 1 --output "$TEMP_FILE"
    check_throttling
    if [ ! -s "$OUTPUT" ]; then
        sed -n '1p' "$TEMP_FILE" > "$OUTPUT"
    fi
    sed -n '2,$p' "$TEMP_FILE" >> "$OUTPUT"
}

append_run() {
    completed=0
    while [ "$completed" -lt "$REPETITIONS" ]; do
        append_sample "$@"
        completed=$((completed + 1))
    done
}

order_index=0
for size in $SIZES; do
    case $((order_index % 4)) in
        0) order="aos soa auto neon" ;;
        1) order="soa auto neon aos" ;;
        2) order="auto neon aos soa" ;;
        3) order="neon aos soa auto" ;;
    esac
    for kernel in $order; do
        append_run --kernel "$kernel" --nx "$size" --ny "$size" \
            --steps "$STEPS" --threads 1 --pin
    done
    order_index=$((order_index + 1))
done

for threads in 1 2 3 4; do
    append_run --kernel neon --nx 512 --ny 512 --steps "$STEPS" --threads "$threads" --pin
done

{
    echo "[after]"
    sh scripts/system_info.sh
} >> "$METADATA"

echo "Results written to $OUTPUT"
echo "Metadata written to $METADATA"

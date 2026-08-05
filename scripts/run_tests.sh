#!/bin/sh
set -eu

BENCHMARK=${BENCHMARK:-./build/lbm_bench}
RESULT_DIR=${RESULT_DIR:-results}
SIZES=${SIZES:-"16 64 128 256 512"}
STEPS=${STEPS:-500}
REPETITIONS=${REPETITIONS:-10}
WARMUP=${WARMUP:-20}
MAX_TEMP_C=${MAX_TEMP_C:-75}

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

append_run() {
    wait_for_temperature
    "$BENCHMARK" "$@" --warmup "$WARMUP" --repetitions "$REPETITIONS" --output "$TEMP_FILE"
    if [ ! -s "$OUTPUT" ]; then
        sed -n '1p' "$TEMP_FILE" > "$OUTPUT"
    fi
    sed -n '2,$p' "$TEMP_FILE" >> "$OUTPUT"
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

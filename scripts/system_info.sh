#!/bin/sh
set -eu

read_value() {
    key=$1
    path=$2
    if [ -r "$path" ]; then
        value=$(sed -n '1p' "$path")
    else
        value=unavailable
    fi
    printf '%s=%s\n' "$key" "$value"
}

printf 'captured_utc=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
printf 'uname=%s\n' "$(uname -a)"
printf 'compiler=%s\n' "${CC:-cc}"
${CC:-cc} --version 2>&1 | sed -n '1s/^/compiler_version=/p' || true

if [ -r /proc/cpuinfo ]; then
    sed -n 's/^Model[[:space:]]*:[[:space:]]*/cpu_model=/p' /proc/cpuinfo | sed -n '1p'
fi
read_value governor /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
read_value minimum_frequency_khz /sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq
read_value maximum_frequency_khz /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq
read_value temperature_millicelsius /sys/class/thermal/thermal_zone0/temp

if command -v vcgencmd >/dev/null 2>&1; then
    printf 'firmware_throttled=%s\n' "$(vcgencmd get_throttled)"
else
    printf 'firmware_throttled=unavailable\n'
fi

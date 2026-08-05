#define _POSIX_C_SOURCE 200809L

#include "benchmark.h"

#include <stdio.h>
#include <time.h>

double benchmark_now_seconds(void) {
    struct timespec value;
#if defined(CLOCK_MONOTONIC_RAW)
    const clockid_t clock_id = CLOCK_MONOTONIC_RAW;
#else
    const clockid_t clock_id = CLOCK_MONOTONIC;
#endif
    if (clock_gettime(clock_id, &value) != 0) return -1.0;
    return (double)value.tv_sec + (double)value.tv_nsec * 1.0e-9;
}

uint64_t benchmark_unix_time(void) {
    return (uint64_t)time(NULL);
}

static double read_number(const char *path, double divisor) {
    FILE *file = fopen(path, "r");
    if (!file) return -1.0;
    double value = -1.0;
    if (fscanf(file, "%lf", &value) != 1) value = -1.0;
    fclose(file);
    return value < 0.0 ? -1.0 : value / divisor;
}

double benchmark_temperature_c(void) {
    return read_number("/sys/class/thermal/thermal_zone0/temp", 1000.0);
}

long benchmark_cpu_frequency_khz(void) {
    const double value = read_number(
        "/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", 1.0);
    return value < 0.0 ? -1L : (long)value;
}

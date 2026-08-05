#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <stdint.h>

double benchmark_now_seconds(void);
double benchmark_temperature_c(void);
long benchmark_cpu_frequency_khz(void);
uint64_t benchmark_unix_time(void);

#endif

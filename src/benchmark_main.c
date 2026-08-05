#define _POSIX_C_SOURCE 200809L

#include "benchmark.h"
#include "lbm.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef BUILD_FLAGS
#define BUILD_FLAGS "unknown"
#endif

typedef struct {
    size_t nx;
    size_t ny;
    size_t steps;
    size_t warmup;
    unsigned repetitions;
    unsigned threads;
    int pin;
    int all_kernels;
    lbm_kernel kernel;
    float viscosity;
    float velocity;
    const char *output_path;
} options;

static void usage(FILE *stream, const char *program) {
    fprintf(stream,
        "Usage: %s [options]\n"
        "  --kernel aos|soa|auto|neon|all  Kernel to run (default: all)\n"
        "  --nx N --ny N                   Lattice dimensions (default: 128x128)\n"
        "  --steps N                       Timed steps per sample (default: 500)\n"
        "  --warmup N                      Untimed warm-up steps (default: 20)\n"
        "  --repetitions N                 Samples per kernel (default: 10)\n"
        "  --threads N                     Worker threads (default: 1)\n"
        "  --pin                           Request Linux CPU affinity\n"
        "  --viscosity X                   Lattice viscosity (default: 0.1666667)\n"
        "  --velocity X                    Initial velocity, Mach < 0.1 advised (default: 0.05)\n"
        "  --output FILE                   Write CSV to FILE instead of stdout\n"
        "  --help                          Show this help\n", program);
}

static int parse_size(const char *text, size_t *value) {
    char *end = NULL;
    errno = 0;
    unsigned long long parsed = strtoull(text, &end, 10);
    if (errno || !end || *end || parsed == 0) return -1;
    *value = (size_t)parsed;
    return (unsigned long long)*value == parsed ? 0 : -1;
}

static int parse_unsigned(const char *text, unsigned *value) {
    size_t parsed;
    if (parse_size(text, &parsed) != 0 || parsed > 1024) return -1;
    *value = (unsigned)parsed;
    return 0;
}

static int parse_float(const char *text, float *value) {
    char *end = NULL;
    errno = 0;
    float parsed = strtof(text, &end);
    if (errno || !end || *end) return -1;
    *value = parsed;
    return 0;
}

static int parse_options(int argc, char **argv, options *result) {
    *result = (options){
        .nx = 128, .ny = 128, .steps = 500, .warmup = 20,
        .repetitions = 10, .threads = 1, .all_kernels = 1,
        .viscosity = 1.0f / 6.0f, .velocity = 0.05f
    };
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0) return 1;
        if (strcmp(argv[i], "--pin") == 0) { result->pin = 1; continue; }
        if (i + 1 >= argc) return -1;
        const char *value = argv[++i];
        if (strcmp(argv[i - 1], "--kernel") == 0) {
            result->all_kernels = strcmp(value, "all") == 0;
            if (!result->all_kernels && lbm_kernel_from_name(value, &result->kernel) != 0) return -1;
        } else if (strcmp(argv[i - 1], "--nx") == 0) {
            if (parse_size(value, &result->nx) != 0) return -1;
        } else if (strcmp(argv[i - 1], "--ny") == 0) {
            if (parse_size(value, &result->ny) != 0) return -1;
        } else if (strcmp(argv[i - 1], "--steps") == 0) {
            if (parse_size(value, &result->steps) != 0) return -1;
        } else if (strcmp(argv[i - 1], "--warmup") == 0) {
            if (parse_size(value, &result->warmup) != 0) return -1;
        } else if (strcmp(argv[i - 1], "--repetitions") == 0) {
            if (parse_unsigned(value, &result->repetitions) != 0) return -1;
        } else if (strcmp(argv[i - 1], "--threads") == 0) {
            if (parse_unsigned(value, &result->threads) != 0) return -1;
        } else if (strcmp(argv[i - 1], "--viscosity") == 0) {
            if (parse_float(value, &result->viscosity) != 0) return -1;
        } else if (strcmp(argv[i - 1], "--velocity") == 0) {
            if (parse_float(value, &result->velocity) != 0) return -1;
        } else if (strcmp(argv[i - 1], "--output") == 0) {
            result->output_path = value;
        } else {
            return -1;
        }
    }
    if (result->nx < 3 || result->ny < 3 || result->viscosity <= 0.0f ||
        result->velocity <= 0.0f || result->velocity >= 0.1f) return -1;
    return 0;
}

static int run_kernel(FILE *output, const options *config, lbm_kernel kernel) {
    if (!lbm_kernel_available(kernel)) {
        fprintf(stderr, "Skipping unavailable kernel: %s\n", lbm_kernel_name(kernel));
        return 0;
    }
    const lbm_layout layout = kernel == LBM_KERNEL_AOS
        ? LBM_LAYOUT_AOS : LBM_LAYOUT_SOA;
    lbm_state state;
    if (lbm_state_create(&state, config->nx, config->ny, layout,
                         config->viscosity, config->velocity) != 0) {
        fprintf(stderr, "Failed to allocate %zux%zu %s state\n",
                config->nx, config->ny, lbm_kernel_name(kernel));
        return -1;
    }

    lbm_initialize_taylor_green(&state);
    if (lbm_run(&state, kernel, config->warmup, config->threads, config->pin) != 0) {
        fprintf(stderr, "Warm-up failed for %s\n", lbm_kernel_name(kernel));
        lbm_state_destroy(&state);
        return -1;
    }

    for (unsigned repetition = 0; repetition < config->repetitions; ++repetition) {
        lbm_initialize_taylor_green(&state);
        const double initial_mass = lbm_total_mass(&state);
        const double temperature_before = benchmark_temperature_c();
        const long frequency = benchmark_cpu_frequency_khz();
        const double start = benchmark_now_seconds();
        if (lbm_run(&state, kernel, config->steps, config->threads, config->pin) != 0) {
            lbm_state_destroy(&state);
            return -1;
        }
        const double finish = benchmark_now_seconds();
        const double temperature_after = benchmark_temperature_c();
        const lbm_validation validation = lbm_validate_taylor_green(
            &state, config->steps, initial_mass);
        const double elapsed = finish - start;
        const double updates = (double)config->nx * (double)config->ny *
                               (double)config->steps;
        const double mlups = updates / elapsed / 1.0e6;
        const double ns_per_update = elapsed * 1.0e9 / updates;
        const double estimated_gbps = updates * (2.0 * LBM_Q * sizeof(float)) /
                                      elapsed / 1.0e9;
        fprintf(output,
            "%llu,%s,%zu,%zu,%zu,%zu,%u,%u,%d,%.9f,%.6f,%.6f,%.6f,"
            "%.9e,%.9e,%.12e,%.3f,%.3f,%ld,%d,\"%s\",\"%s\"\n",
            (unsigned long long)benchmark_unix_time(), lbm_kernel_name(kernel),
            config->nx, config->ny, config->steps, config->warmup,
            repetition, config->threads, config->pin, elapsed, mlups,
            ns_per_update, estimated_gbps, validation.relative_mass_error,
            validation.velocity_l2_error, validation.checksum,
            temperature_before, temperature_after, frequency,
            validation.finite,
#ifdef __VERSION__
            __VERSION__,
#else
            "unknown",
#endif
            BUILD_FLAGS);
        fflush(output);
    }
    lbm_state_destroy(&state);
    return 0;
}

int main(int argc, char **argv) {
    options config;
    const int parsed = parse_options(argc, argv, &config);
    if (parsed != 0) {
        usage(parsed > 0 ? stdout : stderr, argv[0]);
        return parsed > 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    FILE *output = stdout;
    if (config.output_path) {
        output = fopen(config.output_path, "w");
        if (!output) {
            perror(config.output_path);
            return EXIT_FAILURE;
        }
    }
    fprintf(output,
        "timestamp,kernel,nx,ny,steps,warmup,repetition,threads,pinned,"
        "seconds,mlups,ns_per_update,estimated_gbps,mass_relative_error,"
        "velocity_l2_error,checksum,temp_before_c,temp_after_c,cpu_khz,"
        "finite,compiler,build_flags\n");
    int status = 0;
    if (config.all_kernels) {
        for (int kernel = LBM_KERNEL_AOS; kernel <= LBM_KERNEL_NEON; ++kernel) {
            if (run_kernel(output, &config, (lbm_kernel)kernel) != 0) status = 1;
        }
    } else {
        status = run_kernel(output, &config, config.kernel) != 0;
    }
    if (output != stdout) fclose(output);
    return status ? EXIT_FAILURE : EXIT_SUCCESS;
}

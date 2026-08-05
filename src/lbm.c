#define _POSIX_C_SOURCE 200112L

#include "lbm.h"
#include "lbm_internal.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

const int lbm_cx[LBM_Q] = {0, 1, 0, -1, 0, 1, -1, -1, 1};
const int lbm_cy[LBM_Q] = {0, 0, 1, 0, -1, 1, 1, -1, -1};
const float lbm_weight[LBM_Q] = {
    4.0f / 9.0f,
    1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f,
    1.0f / 36.0f, 1.0f / 36.0f, 1.0f / 36.0f, 1.0f / 36.0f
};

static size_t distribution_index(const lbm_state *state,
                                 size_t cell, int q) {
    return state->layout == LBM_LAYOUT_AOS
        ? cell * LBM_Q + (size_t)q
        : (size_t)q * state->cells + cell;
}

static int allocate_aligned(float **ptr, size_t count) {
    void *memory = NULL;
    if (posix_memalign(&memory, 64, count * sizeof(float)) != 0) {
        return -1;
    }
    *ptr = memory;
    return 0;
}

int lbm_state_create(lbm_state *state, size_t nx, size_t ny,
                     lbm_layout layout, float viscosity,
                     float initial_velocity) {
    if (!state || nx < 3 || ny < 3 || viscosity <= 0.0f ||
        nx > SIZE_MAX / ny || nx * ny > SIZE_MAX / LBM_Q) {
        return -1;
    }
    memset(state, 0, sizeof(*state));
    state->nx = nx;
    state->ny = ny;
    state->cells = nx * ny;
    state->layout = layout;
    state->viscosity = viscosity;
    state->omega = 1.0f / (3.0f * viscosity + 0.5f);
    state->initial_velocity = initial_velocity;
    const size_t count = state->cells * LBM_Q;
    if (allocate_aligned(&state->buffer[0], count) != 0 ||
        allocate_aligned(&state->buffer[1], count) != 0) {
        lbm_state_destroy(state);
        return -1;
    }
    return 0;
}

void lbm_state_destroy(lbm_state *state) {
    if (!state) return;
    free(state->buffer[0]);
    free(state->buffer[1]);
    memset(state, 0, sizeof(*state));
}

static void initialize_cell(lbm_state *state, size_t cell,
                            float density, float ux, float uy) {
    for (int q = 0; q < LBM_Q; ++q) {
        const float value = lbm_equilibrium(q, density, ux, uy);
        state->buffer[0][distribution_index(state, cell, q)] = value;
        state->buffer[1][distribution_index(state, cell, q)] = value;
    }
}

void lbm_initialize_uniform(lbm_state *state, float density,
                            float ux, float uy) {
    for (size_t cell = 0; cell < state->cells; ++cell) {
        initialize_cell(state, cell, density, ux, uy);
    }
    state->current = 0;
}

void lbm_initialize_taylor_green(lbm_state *state) {
    const double two_pi = 6.2831853071795864769;
    const double kx = two_pi / (double)state->nx;
    const double ky = two_pi / (double)state->ny;
    const double y_scale = kx / ky;
    for (size_t y = 0; y < state->ny; ++y) {
        for (size_t x = 0; x < state->nx; ++x) {
            const double px = kx * ((double)x + 0.5);
            const double py = ky * ((double)y + 0.5);
            const float ux = state->initial_velocity *
                (float)(sin(px) * cos(py));
            const float uy = -state->initial_velocity * (float)y_scale *
                (float)(cos(px) * sin(py));
            initialize_cell(state, y * state->nx + x, 1.0f, ux, uy);
        }
    }
    state->current = 0;
}

void lbm_macroscopic_at(const lbm_state *state, size_t x, size_t y,
                        float *density, float *ux, float *uy) {
    const float *data = state->buffer[state->current];
    const size_t cell = y * state->nx + x;
    float f[LBM_Q];
    float rho = 0.0f;
    for (int q = 0; q < LBM_Q; ++q) {
        f[q] = data[distribution_index(state, cell, q)];
        rho += f[q];
    }
    *density = rho;
    *ux = (f[1] - f[3] + f[5] - f[6] - f[7] + f[8]) / rho;
    *uy = (f[2] - f[4] + f[5] + f[6] - f[7] - f[8]) / rho;
}

double lbm_total_mass(const lbm_state *state) {
    const float *data = state->buffer[state->current];
    double mass = 0.0;
    for (size_t cell = 0; cell < state->cells; ++cell) {
        for (int q = 0; q < LBM_Q; ++q) {
            mass += data[distribution_index(state, cell, q)];
        }
    }
    return mass;
}

lbm_validation lbm_validate_taylor_green(const lbm_state *state,
                                         size_t completed_steps,
                                         double initial_mass) {
    lbm_validation result = {0};
    const double two_pi = 6.2831853071795864769;
    const double kx = two_pi / (double)state->nx;
    const double ky = two_pi / (double)state->ny;
    const double decay = exp(-(double)state->viscosity *
                             (kx * kx + ky * ky) *
                             (double)completed_steps);
    const double y_scale = kx / ky;
    double error2 = 0.0;
    double reference2 = 0.0;
    double checksum = 0.0;
    result.finite = 1;
    for (size_t y = 0; y < state->ny; ++y) {
        for (size_t x = 0; x < state->nx; ++x) {
            float rho, ux, uy;
            lbm_macroscopic_at(state, x, y, &rho, &ux, &uy);
            const double px = kx * ((double)x + 0.5);
            const double py = ky * ((double)y + 0.5);
            const double expected_x = state->initial_velocity * decay *
                                      sin(px) * cos(py);
            const double expected_y = -state->initial_velocity * decay *
                                      y_scale * cos(px) * sin(py);
            const double dx = (double)ux - expected_x;
            const double dy = (double)uy - expected_y;
            error2 += dx * dx + dy * dy;
            reference2 += expected_x * expected_x + expected_y * expected_y;
            checksum += (double)rho + 0.5 * (double)ux + 0.25 * (double)uy;
            if (!isfinite(rho) || !isfinite(ux) || !isfinite(uy)) {
                result.finite = 0;
            }
        }
    }
    result.mass = lbm_total_mass(state);
    result.relative_mass_error = initial_mass == 0.0 ? 0.0 :
        fabs(result.mass - initial_mass) / fabs(initial_mass);
    result.velocity_l2_error = reference2 == 0.0 ? sqrt(error2) :
        sqrt(error2 / reference2);
    result.checksum = checksum;
    return result;
}

double lbm_max_macro_difference(const lbm_state *a, const lbm_state *b) {
    if (!a || !b || a->nx != b->nx || a->ny != b->ny) return INFINITY;
    double maximum = 0.0;
    for (size_t y = 0; y < a->ny; ++y) {
        for (size_t x = 0; x < a->nx; ++x) {
            float ar, ax, ay, br, bx, by;
            lbm_macroscopic_at(a, x, y, &ar, &ax, &ay);
            lbm_macroscopic_at(b, x, y, &br, &bx, &by);
            const double differences[3] = {
                fabs((double)ar - br), fabs((double)ax - bx),
                fabs((double)ay - by)
            };
            for (int i = 0; i < 3; ++i) {
                if (differences[i] > maximum) maximum = differences[i];
            }
        }
    }
    return maximum;
}

const char *lbm_kernel_name(lbm_kernel kernel) {
    static const char *names[] = {"aos", "soa", "auto", "neon"};
    return kernel >= LBM_KERNEL_AOS && kernel <= LBM_KERNEL_NEON
        ? names[kernel] : "unknown";
}

int lbm_kernel_from_name(const char *name, lbm_kernel *kernel) {
    if (!name || !kernel) return -1;
    for (int k = LBM_KERNEL_AOS; k <= LBM_KERNEL_NEON; ++k) {
        if (strcmp(name, lbm_kernel_name((lbm_kernel)k)) == 0) {
            *kernel = (lbm_kernel)k;
            return 0;
        }
    }
    return -1;
}

int lbm_kernel_available(lbm_kernel kernel) {
    return kernel != LBM_KERNEL_NEON || lbm_neon_available();
}

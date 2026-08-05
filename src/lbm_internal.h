#ifndef LBM_INTERNAL_H
#define LBM_INTERNAL_H

#include "lbm.h"

extern const int lbm_cx[LBM_Q];
extern const int lbm_cy[LBM_Q];
extern const float lbm_weight[LBM_Q];

typedef void (*lbm_range_fn)(const lbm_state *state, const float *input,
                             float *output, size_t y_begin, size_t y_end);

void lbm_step_aos_range(const lbm_state *, const float *, float *, size_t, size_t);
void lbm_step_soa_scalar_range(const lbm_state *, const float *, float *, size_t, size_t);
void lbm_step_soa_auto_range(const lbm_state *, const float *, float *, size_t, size_t);
void lbm_step_neon_range(const lbm_state *, const float *, float *, size_t, size_t);
int lbm_neon_available(void);

static inline size_t lbm_aos_index(const lbm_state *state,
                                   size_t x, size_t y, int q) {
    return ((y * state->nx + x) * LBM_Q) + (size_t)q;
}

static inline size_t lbm_soa_index(const lbm_state *state,
                                   size_t x, size_t y, int q) {
    return ((size_t)q * state->cells) + y * state->nx + x;
}

static inline float lbm_equilibrium(int q, float density,
                                    float ux, float uy) {
    const float cu = (float)lbm_cx[q] * ux + (float)lbm_cy[q] * uy;
    const float u2 = ux * ux + uy * uy;
    return lbm_weight[q] * density *
           (1.0f + 3.0f * cu + 4.5f * cu * cu - 1.5f * u2);
}

static inline void lbm_collide_soa_cell(const lbm_state *state,
                                        const float *input, float *output,
                                        size_t x, size_t y) {
    const size_t nx = state->nx;
    const size_t xp = x + 1 == nx ? 0 : x + 1;
    const size_t xm = x == 0 ? nx - 1 : x - 1;
    const size_t yp = y + 1 == state->ny ? 0 : y + 1;
    const size_t ym = y == 0 ? state->ny - 1 : y - 1;
    const size_t p[LBM_Q] = {
        y * nx + x, y * nx + xm, ym * nx + x,
        y * nx + xp, yp * nx + x, ym * nx + xm,
        ym * nx + xp, yp * nx + xp, yp * nx + xm
    };
    float f[LBM_Q];
    float rho = 0.0f;
    for (int q = 0; q < LBM_Q; ++q) {
        f[q] = input[(size_t)q * state->cells + p[q]];
        rho += f[q];
    }
    const float inv_rho = 1.0f / rho;
    const float ux = (f[1] - f[3] + f[5] - f[6] - f[7] + f[8]) * inv_rho;
    const float uy = (f[2] - f[4] + f[5] + f[6] - f[7] - f[8]) * inv_rho;
    const size_t cell = y * nx + x;
    for (int q = 0; q < LBM_Q; ++q) {
        const float feq = lbm_equilibrium(q, rho, ux, uy);
        output[(size_t)q * state->cells + cell] =
            f[q] + state->omega * (feq - f[q]);
    }
}

#endif

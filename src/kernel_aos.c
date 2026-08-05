#include "lbm_internal.h"

void lbm_step_aos_range(const lbm_state *state, const float *input,
                        float *output, size_t y_begin, size_t y_end) {
    const size_t nx = state->nx;
    for (size_t y = y_begin; y < y_end; ++y) {
        const size_t yp = y + 1 == state->ny ? 0 : y + 1;
        const size_t ym = y == 0 ? state->ny - 1 : y - 1;
        for (size_t x = 0; x < nx; ++x) {
            const size_t xp = x + 1 == nx ? 0 : x + 1;
            const size_t xm = x == 0 ? nx - 1 : x - 1;
            const size_t source[LBM_Q] = {
                y * nx + x, y * nx + xm, ym * nx + x,
                y * nx + xp, yp * nx + x, ym * nx + xm,
                ym * nx + xp, yp * nx + xp, yp * nx + xm
            };
            float f[LBM_Q];
            float rho = 0.0f;
            for (int q = 0; q < LBM_Q; ++q) {
                f[q] = input[source[q] * LBM_Q + (size_t)q];
                rho += f[q];
            }
            const float inv_rho = 1.0f / rho;
            const float ux = (f[1] - f[3] + f[5] - f[6] - f[7] + f[8]) * inv_rho;
            const float uy = (f[2] - f[4] + f[5] + f[6] - f[7] - f[8]) * inv_rho;
            const size_t destination = (y * nx + x) * LBM_Q;
            for (int q = 0; q < LBM_Q; ++q) {
                const float feq = lbm_equilibrium(q, rho, ux, uy);
                output[destination + (size_t)q] =
                    f[q] + state->omega * (feq - f[q]);
            }
        }
    }
}

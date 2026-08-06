#include "lbm_internal.h"

void lbm_step_soa_auto_range(const lbm_state *state,
                             const float *restrict input,
                             float *restrict output,
                             size_t y_begin, size_t y_end) {
    const size_t nx = state->nx;
    const size_t cells = state->cells;
    const float omega = state->omega;
    for (size_t y = y_begin; y < y_end; ++y) {
        const size_t ym = y == 0 ? state->ny - 1 : y - 1;
        const size_t yp = y + 1 == state->ny ? 0 : y + 1;
        lbm_collide_soa_cell(state, input, output, 0, y);

        /* Double buffers make these iterations independent; the pragma avoids
           excessive ARMv7 alias-versioning checks. */
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC ivdep
#elif defined(__clang__)
#pragma clang loop vectorize(enable)
#endif
        for (size_t x = 1; x + 1 < nx; ++x) {
            const size_t cell = y * nx + x;
            const float f0 = input[0 * cells + cell];
            const float f1 = input[1 * cells + cell - 1];
            const float f2 = input[2 * cells + ym * nx + x];
            const float f3 = input[3 * cells + cell + 1];
            const float f4 = input[4 * cells + yp * nx + x];
            const float f5 = input[5 * cells + ym * nx + x - 1];
            const float f6 = input[6 * cells + ym * nx + x + 1];
            const float f7 = input[7 * cells + yp * nx + x + 1];
            const float f8 = input[8 * cells + yp * nx + x - 1];
            const float rho = f0 + f1 + f2 + f3 + f4 + f5 + f6 + f7 + f8;
            const float ux = (f1 - f3 + f5 - f6 - f7 + f8) / rho;
            const float uy = (f2 - f4 + f5 + f6 - f7 - f8) / rho;
            const float u2 = ux * ux + uy * uy;
            const float f[LBM_Q] = {f0, f1, f2, f3, f4, f5, f6, f7, f8};
            for (int q = 0; q < LBM_Q; ++q) {
                const float cu = (float)lbm_cx[q] * ux + (float)lbm_cy[q] * uy;
                const float feq = lbm_weight[q] * rho *
                    (1.0f + 3.0f * cu + 4.5f * cu * cu - 1.5f * u2);
                output[(size_t)q * cells + cell] = f[q] + omega * (feq - f[q]);
            }
        }

        lbm_collide_soa_cell(state, input, output, nx - 1, y);
    }
}

#include "lbm_internal.h"

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>

static inline float32x4_t reciprocal(float32x4_t value) {
#if defined(__aarch64__)
    return vdivq_f32(vdupq_n_f32(1.0f), value);
#else
    float32x4_t estimate = vrecpeq_f32(value);
    estimate = vmulq_f32(vrecpsq_f32(value, estimate), estimate);
    estimate = vmulq_f32(vrecpsq_f32(value, estimate), estimate);
    return estimate;
#endif
}

void lbm_step_neon_range(const lbm_state *state, const float *input,
                         float *output, size_t y_begin, size_t y_end) {
    const size_t nx = state->nx;
    const size_t cells = state->cells;
    const float32x4_t one = vdupq_n_f32(1.0f);
    const float32x4_t three = vdupq_n_f32(3.0f);
    const float32x4_t four_half = vdupq_n_f32(4.5f);
    const float32x4_t one_half = vdupq_n_f32(1.5f);
    const float32x4_t omega = vdupq_n_f32(state->omega);

    for (size_t y = y_begin; y < y_end; ++y) {
        const size_t ym = y == 0 ? state->ny - 1 : y - 1;
        const size_t yp = y + 1 == state->ny ? 0 : y + 1;
        lbm_collide_soa_cell(state, input, output, 0, y);
        size_t x = 1;
        for (; x + 4 < nx; x += 4) {
            const size_t cell = y * nx + x;
            float32x4_t f[LBM_Q];
            f[0] = vld1q_f32(input + 0 * cells + cell);
            f[1] = vld1q_f32(input + 1 * cells + cell - 1);
            f[2] = vld1q_f32(input + 2 * cells + ym * nx + x);
            f[3] = vld1q_f32(input + 3 * cells + cell + 1);
            f[4] = vld1q_f32(input + 4 * cells + yp * nx + x);
            f[5] = vld1q_f32(input + 5 * cells + ym * nx + x - 1);
            f[6] = vld1q_f32(input + 6 * cells + ym * nx + x + 1);
            f[7] = vld1q_f32(input + 7 * cells + yp * nx + x + 1);
            f[8] = vld1q_f32(input + 8 * cells + yp * nx + x - 1);

            float32x4_t rho = f[0];
            for (int q = 1; q < LBM_Q; ++q) rho = vaddq_f32(rho, f[q]);
            const float32x4_t inv_rho = reciprocal(rho);
            float32x4_t ux = vsubq_f32(f[1], f[3]);
            ux = vaddq_f32(ux, vsubq_f32(f[5], f[6]));
            ux = vaddq_f32(ux, vsubq_f32(f[8], f[7]));
            ux = vmulq_f32(ux, inv_rho);
            float32x4_t uy = vsubq_f32(f[2], f[4]);
            uy = vaddq_f32(uy, vaddq_f32(f[5], f[6]));
            uy = vsubq_f32(uy, vaddq_f32(f[7], f[8]));
            uy = vmulq_f32(uy, inv_rho);
            const float32x4_t u2 = vaddq_f32(vmulq_f32(ux, ux), vmulq_f32(uy, uy));

            for (int q = 0; q < LBM_Q; ++q) {
                float32x4_t cu = vdupq_n_f32(0.0f);
                if (lbm_cx[q] == 1) cu = vaddq_f32(cu, ux);
                if (lbm_cx[q] == -1) cu = vsubq_f32(cu, ux);
                if (lbm_cy[q] == 1) cu = vaddq_f32(cu, uy);
                if (lbm_cy[q] == -1) cu = vsubq_f32(cu, uy);
                float32x4_t polynomial = vsubq_f32(one, vmulq_f32(one_half, u2));
                polynomial = vaddq_f32(polynomial, vmulq_f32(three, cu));
                polynomial = vaddq_f32(polynomial,
                                       vmulq_f32(four_half, vmulq_f32(cu, cu)));
                const float32x4_t feq = vmulq_n_f32(vmulq_f32(rho, polynomial),
                                                    lbm_weight[q]);
                const float32x4_t result = vaddq_f32(
                    f[q], vmulq_f32(omega, vsubq_f32(feq, f[q])));
                vst1q_f32(output + (size_t)q * cells + cell, result);
            }
        }
        for (; x + 1 < nx; ++x) {
            lbm_collide_soa_cell(state, input, output, x, y);
        }
        lbm_collide_soa_cell(state, input, output, nx - 1, y);
    }
}

int lbm_neon_available(void) { return 1; }

#else

void lbm_step_neon_range(const lbm_state *state, const float *input,
                         float *output, size_t y_begin, size_t y_end) {
    (void)state;
    (void)input;
    (void)output;
    (void)y_begin;
    (void)y_end;
}

int lbm_neon_available(void) { return 0; }

#endif

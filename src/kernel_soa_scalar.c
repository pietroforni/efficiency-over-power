#include "lbm_internal.h"

void lbm_step_soa_scalar_range(const lbm_state *state, const float *input,
                               float *output, size_t y_begin, size_t y_end) {
    for (size_t y = y_begin; y < y_end; ++y) {
        for (size_t x = 0; x < state->nx; ++x) {
            lbm_collide_soa_cell(state, input, output, x, y);
        }
    }
}

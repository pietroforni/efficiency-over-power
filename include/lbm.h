#ifndef LBM_H
#define LBM_H

#include <stddef.h>

#define LBM_Q 9

typedef enum {
    LBM_LAYOUT_AOS = 0,
    LBM_LAYOUT_SOA = 1
} lbm_layout;

typedef enum {
    LBM_KERNEL_AOS = 0,
    LBM_KERNEL_SOA_SCALAR,
    LBM_KERNEL_SOA_AUTO,
    LBM_KERNEL_NEON
} lbm_kernel;

typedef struct {
    size_t nx;
    size_t ny;
    size_t cells;
    lbm_layout layout;
    float *buffer[2];
    unsigned current;
    float viscosity;
    float omega;
    float initial_velocity;
} lbm_state;

typedef struct {
    double mass;
    double relative_mass_error;
    double velocity_l2_error;
    double checksum;
    int finite;
} lbm_validation;

int lbm_state_create(lbm_state *state, size_t nx, size_t ny,
                     lbm_layout layout, float viscosity,
                     float initial_velocity);
void lbm_state_destroy(lbm_state *state);
void lbm_initialize_taylor_green(lbm_state *state);
void lbm_initialize_uniform(lbm_state *state, float density,
                            float ux, float uy);
void lbm_macroscopic_at(const lbm_state *state, size_t x, size_t y,
                        float *density, float *ux, float *uy);
lbm_validation lbm_validate_taylor_green(const lbm_state *state,
                                         size_t completed_steps,
                                         double initial_mass);
double lbm_total_mass(const lbm_state *state);
double lbm_max_macro_difference(const lbm_state *a, const lbm_state *b);

int lbm_run(lbm_state *state, lbm_kernel kernel, size_t steps,
            unsigned threads, int pin_threads);
int lbm_kernel_available(lbm_kernel kernel);
const char *lbm_kernel_name(lbm_kernel kernel);
int lbm_kernel_from_name(const char *name, lbm_kernel *kernel);

#endif

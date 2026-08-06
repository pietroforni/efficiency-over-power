#include "lbm.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static int failures;

#define CHECK(condition, message) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL: %s (line %d)\n", message, __LINE__); \
        ++failures; \
    } \
} while (0)

static void test_uniform_equilibrium(void) {
    lbm_state state;
    CHECK(lbm_state_create(&state, 8, 7, LBM_LAYOUT_SOA, 1.0f / 6.0f, 0.05f) == 0,
          "allocate uniform state");
    lbm_initialize_uniform(&state, 1.0f, 0.03f, -0.02f);
    float rho, ux, uy;
    lbm_macroscopic_at(&state, 3, 4, &rho, &ux, &uy);
    CHECK(fabsf(rho - 1.0f) < 2.0e-6f, "equilibrium density recovery");
    CHECK(fabsf(ux - 0.03f) < 2.0e-6f, "equilibrium x velocity recovery");
    CHECK(fabsf(uy + 0.02f) < 2.0e-6f, "equilibrium y velocity recovery");
    const double initial_mass = lbm_total_mass(&state);
    CHECK(lbm_run(&state, LBM_KERNEL_SOA_SCALAR, 20, 1, 0) == 0,
          "uniform scalar run");
    CHECK(fabs(lbm_total_mass(&state) - initial_mass) / initial_mass < 2.0e-6,
          "uniform mass conservation");
    lbm_state_destroy(&state);
}

static void test_invalid_state_parameters(void) {
    lbm_state state;
    CHECK(lbm_state_create(&state, 8, 8, (lbm_layout)99,
                           1.0f / 6.0f, 0.05f) != 0,
          "reject invalid layout");
    CHECK(lbm_state_create(&state, 8, 8, LBM_LAYOUT_SOA, NAN, 0.05f) != 0,
          "reject non-finite viscosity");
    CHECK(lbm_state_create(&state, 8, 8, LBM_LAYOUT_SOA,
                           1.0f / 6.0f, INFINITY) != 0,
          "reject non-finite velocity");
    CHECK(lbm_state_create(&state, SIZE_MAX, 3, LBM_LAYOUT_SOA,
                           1.0f / 6.0f, 0.05f) != 0,
          "reject overflowing allocation size");
}

static void compare_kernel(lbm_kernel candidate, unsigned threads,
                           size_t nx, size_t ny, double tolerance) {
    lbm_state reference;
    lbm_state tested;
    CHECK(lbm_state_create(&reference, nx, ny, LBM_LAYOUT_AOS,
                           1.0f / 6.0f, 0.04f) == 0,
          "allocate AoS reference");
    CHECK(lbm_state_create(&tested, nx, ny, LBM_LAYOUT_SOA,
                           1.0f / 6.0f, 0.04f) == 0,
          "allocate SoA candidate");
    lbm_initialize_taylor_green(&reference);
    lbm_initialize_taylor_green(&tested);
    CHECK(lbm_run(&reference, LBM_KERNEL_AOS, 13, 1, 0) == 0,
          "run AoS reference");
    CHECK(lbm_run(&tested, candidate, 13, threads, 0) == 0,
          "run candidate kernel");
    const double difference = lbm_max_macro_difference(&reference, &tested);
    if (!(difference < tolerance)) {
        fprintf(stderr, "FAIL: %s/%u differs by %.9e (limit %.9e)\n",
                lbm_kernel_name(candidate), threads, difference, tolerance);
        ++failures;
    }
    lbm_state_destroy(&tested);
    lbm_state_destroy(&reference);
}

static void test_kernel_equivalence(void) {
    compare_kernel(LBM_KERNEL_SOA_SCALAR, 1, 17, 19, 2.0e-6);
    compare_kernel(LBM_KERNEL_SOA_AUTO, 1, 17, 19, 2.0e-6);
    compare_kernel(LBM_KERNEL_SOA_AUTO, 3, 17, 19, 2.0e-6);
    if (lbm_kernel_available(LBM_KERNEL_NEON)) {
        compare_kernel(LBM_KERNEL_NEON, 1, 17, 19, 1.0e-5);
        compare_kernel(LBM_KERNEL_NEON, 3, 17, 19, 1.0e-5);
    }
}

static void test_taylor_green_validation(void) {
    lbm_state state;
    CHECK(lbm_state_create(&state, 64, 64, LBM_LAYOUT_SOA,
                           1.0f / 6.0f, 0.03f) == 0,
          "allocate Taylor-Green state");
    lbm_initialize_taylor_green(&state);
    const double initial_mass = lbm_total_mass(&state);
    CHECK(lbm_run(&state, LBM_KERNEL_SOA_AUTO, 50, 2, 0) == 0,
          "run Taylor-Green case");
    const lbm_validation result = lbm_validate_taylor_green(&state, 50, initial_mass);
    CHECK(result.finite, "Taylor-Green remains finite");
    CHECK(result.relative_mass_error < 3.0e-6, "Taylor-Green mass conservation");
    CHECK(result.velocity_l2_error < 0.08, "Taylor-Green analytic decay error");
    lbm_state_destroy(&state);
}

int main(void) {
    test_invalid_state_parameters();
    test_uniform_equilibrium();
    test_kernel_equivalence();
    test_taylor_green_validation();
    if (failures) {
        fprintf(stderr, "%d test(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    puts("All LBM tests passed.");
    return EXIT_SUCCESS;
}

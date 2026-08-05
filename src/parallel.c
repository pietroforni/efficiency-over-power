#if defined(__linux__)
#define _GNU_SOURCE
#endif

#include "lbm_internal.h"

#include <pthread.h>
#include <stdlib.h>

#if defined(__linux__)
#include <sched.h>
#include <unistd.h>
#endif

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    unsigned waiting;
    unsigned participants;
    unsigned generation;
} lbm_barrier;

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    int released;
    int abort;
} start_gate;

typedef struct {
    lbm_state *state;
    lbm_range_fn function;
    lbm_barrier *barrier;
    start_gate *gate;
    size_t y_begin;
    size_t y_end;
    size_t steps;
    unsigned index;
    int pin;
} worker_argument;

static int barrier_init(lbm_barrier *barrier, unsigned participants) {
    barrier->waiting = 0;
    barrier->participants = participants;
    barrier->generation = 0;
    if (pthread_mutex_init(&barrier->mutex, NULL) != 0) return -1;
    if (pthread_cond_init(&barrier->condition, NULL) != 0) {
        pthread_mutex_destroy(&barrier->mutex);
        return -1;
    }
    return 0;
}

static void barrier_destroy(lbm_barrier *barrier) {
    pthread_cond_destroy(&barrier->condition);
    pthread_mutex_destroy(&barrier->mutex);
}

static void barrier_wait(lbm_barrier *barrier) {
    pthread_mutex_lock(&barrier->mutex);
    const unsigned generation = barrier->generation;
    if (++barrier->waiting == barrier->participants) {
        barrier->waiting = 0;
        ++barrier->generation;
        pthread_cond_broadcast(&barrier->condition);
    } else {
        while (generation == barrier->generation) {
            pthread_cond_wait(&barrier->condition, &barrier->mutex);
        }
    }
    pthread_mutex_unlock(&barrier->mutex);
}

static void pin_current_thread(unsigned index) {
#if defined(__linux__)
    const long count = sysconf(_SC_NPROCESSORS_ONLN);
    if (count <= 0) return;
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(index % (unsigned)count, &set);
    (void)pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
#else
    (void)index;
#endif
}

static void *worker_main(void *opaque) {
    worker_argument *argument = opaque;
    if (argument->pin) pin_current_thread(argument->index);
    pthread_mutex_lock(&argument->gate->mutex);
    while (!argument->gate->released) {
        pthread_cond_wait(&argument->gate->condition, &argument->gate->mutex);
    }
    const int abort = argument->gate->abort;
    pthread_mutex_unlock(&argument->gate->mutex);
    if (abort) return NULL;
    const unsigned initial = argument->state->current;
    for (size_t step = 0; step < argument->steps; ++step) {
        const unsigned input_index = initial ^ (unsigned)(step & 1u);
        const unsigned output_index = input_index ^ 1u;
        argument->function(argument->state,
                           argument->state->buffer[input_index],
                           argument->state->buffer[output_index],
                           argument->y_begin, argument->y_end);
        barrier_wait(argument->barrier);
    }
    return NULL;
}

static lbm_range_fn select_function(lbm_kernel kernel) {
    switch (kernel) {
        case LBM_KERNEL_AOS: return lbm_step_aos_range;
        case LBM_KERNEL_SOA_SCALAR: return lbm_step_soa_scalar_range;
        case LBM_KERNEL_SOA_AUTO: return lbm_step_soa_auto_range;
        case LBM_KERNEL_NEON: return lbm_step_neon_range;
    }
    return NULL;
}

int lbm_run(lbm_state *state, lbm_kernel kernel, size_t steps,
            unsigned threads, int pin_threads) {
    lbm_range_fn function = select_function(kernel);
    if (!state || !function || threads == 0 || !lbm_kernel_available(kernel)) return -1;
    if ((kernel == LBM_KERNEL_AOS) != (state->layout == LBM_LAYOUT_AOS)) return -1;
    if (steps == 0) return 0;
    if (threads > state->ny) threads = (unsigned)state->ny;

    if (threads == 1) {
        if (pin_threads) pin_current_thread(0);
        for (size_t step = 0; step < steps; ++step) {
            const unsigned output = state->current ^ 1u;
            function(state, state->buffer[state->current], state->buffer[output],
                     0, state->ny);
            state->current = output;
        }
        return 0;
    }

    lbm_barrier barrier;
    if (barrier_init(&barrier, threads) != 0) return -1;
    start_gate gate;
    gate.released = 0;
    gate.abort = 0;
    if (pthread_mutex_init(&gate.mutex, NULL) != 0) {
        barrier_destroy(&barrier);
        return -1;
    }
    if (pthread_cond_init(&gate.condition, NULL) != 0) {
        pthread_mutex_destroy(&gate.mutex);
        barrier_destroy(&barrier);
        return -1;
    }
    pthread_t *ids = calloc(threads, sizeof(*ids));
    worker_argument *arguments = calloc(threads, sizeof(*arguments));
    if (!ids || !arguments) {
        free(ids);
        free(arguments);
        pthread_cond_destroy(&gate.condition);
        pthread_mutex_destroy(&gate.mutex);
        barrier_destroy(&barrier);
        return -1;
    }

    unsigned created = 0;
    for (unsigned i = 0; i < threads; ++i) {
        arguments[i].state = state;
        arguments[i].function = function;
        arguments[i].barrier = &barrier;
        arguments[i].gate = &gate;
        arguments[i].y_begin = state->ny * i / threads;
        arguments[i].y_end = state->ny * (i + 1u) / threads;
        arguments[i].steps = steps;
        arguments[i].index = i;
        arguments[i].pin = pin_threads;
        if (pthread_create(&ids[i], NULL, worker_main, &arguments[i]) != 0) break;
        ++created;
    }
    pthread_mutex_lock(&gate.mutex);
    gate.abort = created != threads;
    gate.released = 1;
    pthread_cond_broadcast(&gate.condition);
    pthread_mutex_unlock(&gate.mutex);
    if (created != threads) {
        for (unsigned i = 0; i < created; ++i) pthread_join(ids[i], NULL);
        free(arguments);
        free(ids);
        pthread_cond_destroy(&gate.condition);
        pthread_mutex_destroy(&gate.mutex);
        barrier_destroy(&barrier);
        return -1;
    }
    for (unsigned i = 0; i < threads; ++i) pthread_join(ids[i], NULL);
    state->current ^= (unsigned)(steps & 1u);

    free(arguments);
    free(ids);
    pthread_cond_destroy(&gate.condition);
    pthread_mutex_destroy(&gate.mutex);
    barrier_destroy(&barrier);
    return 0;
}

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <xcore/lock.h>
#include <xcore/chanend.h>
#include <xcore/thread.h>

typedef int64_t _instant_t;

/**
 * Interval of time.
 */
typedef int64_t _interval_t;

/**
 * Microstep instant.
 */
typedef uint32_t _microstep_t;

/**
 * For user-friendly reporting of time values, the buffer length required.
 * This is calculated as follows, based on 64-bit time in nanoseconds:
 * Maximum number of weeks is 15,250
 * Maximum number of days is 6
 * Maximum number of hours is 23
 * Maximum number of minutes is 59
 * Maximum number of seconds is 59
 * Maximum number of nanoseconds is 999,999,999
 * Maximum number of microsteps is 4,294,967,295
 * Total number of characters for the above is 24.
 * Text descriptions and spaces add an additional 55,
 * for a total of 79. One more allows for a null terminator.
 */
#define LF_TIME_BUFFER_LENGTH 80
#define _LF_TIMEOUT -2

#include <inttypes.h>  // Needed to define PRId64 and PRIu32
#define PRINTF_TIME "%" PRId64
#define PRINTF_MICROSTEP "%" PRIu32

// For convenience, the following string can be inserted in a printf
// format for printing both time and microstep as follows:
//     printf("Tag is " PRINTF_TAG "\n", time_value, microstep);
#define PRINTF_TAG "(%" PRId64 ", %" PRIu32 ")"
// The underlying physical clock for Linux
#define _LF_CLOCK CLOCK_MONOTONIC

#ifdef NUMBER_OF_WORKERS
// FIXME: We shouldnt need more threads than specified workers...
#define NUMBER_OF_THREADS NUMBER_OF_WORKERS+1

typedef xthread_t _lf_thread_t;        // Type to hold handle to a thread

// Add owner and level to support recursive mutex
typedef struct {
    lock_t lock;
    int owner;
    int level;
} _lf_mutex_t;


typedef struct {
    bool signal;
    bool waiting;
    chanend_t chan;
} cond_elem_t;

typedef struct {
    cond_elem_t waiter[NUMBER_OF_THREADS];
    chanend_t signal_chan;
} _lf_cond_t;            // Type to hold handle to a condition variable

// FIXME: This mapping of atomics to a SINGLE lock is probably very inefficent
//  but I dont see another way without chaning reactor_threaded.c

/*
 * Atomically compare the variable that ptr points to against oldval. If the
 * current value is oldval, then write newval into *ptr.
 * @param ptr A pointer to a variable.
 * @param oldval The value to compare against.
 * @param newval The value to assign to *ptr if comparison is successful.
 * @return True if comparison was successful. False otherwise.
 */

bool lf_xmos_bool_compare_and_swap(bool *ptr, bool oldval, bool newval);
#define lf_bool_compare_and_swap(ptr, oldval, newval) lf_xmos_bool_compare_and_swap((bool *) ptr, oldval, newval)


/*
 * Atomically compare the 32-bit value that ptr points to against oldval. If the
 * current value is oldval, then write newval into *ptr.
 * @param ptr A pointer to a variable.
 * @param oldval The value to compare against.
 * @param newval The value to assign to *ptr if comparison is successful.
 * @return The initial value of *ptr.
 */
int lf_xmos_val_compare_and_swap(int *ptr, int oldval, int newval);
#define lf_val_compare_and_swap(ptr, oldval, newval) lf_xmos_val_compare_and_swap((int*) ptr, oldval, newval)

/*
 * Atomically increment the variable that ptr points to by the given value, and return the original value of the variable.
 * @param ptr A pointer to a variable. The value of this variable will be replaced with the result of the operation.
 * @param value The value to be added to the variable pointed to by the ptr parameter.
 * @return The original value of the variable that ptr points to (i.e., from before the application of this operation).
 */
int lf_xmos_atomic_fetch_add(int *ptr, int val);
#define lf_atomic_fetch_add(ptr, value) lf_xmos_atomic_fetch_add((int *) ptr, value)

/*
 * Atomically increment the variable that ptr points to by the given value, and return the new value of the variable.
 * @param ptr A pointer to a variable. The value of this variable will be replaced with the result of the operation.
 * @param value The value to be added to the variable pointed to by the ptr parameter.
 * @return The new value of the variable that ptr points to (i.e., from before the application of this operation).
 */
int lf_xmos_atomic_add_fetch(int *ptr, int val);
#define lf_atomic_add_fetch(ptr, value) lf_xmos_atomic_add_fetch((int *) ptr, value)

#endif
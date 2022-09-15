#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <xcore/port.h> //FIXME: Hack to get resource_t definition. WHy doesnt xcore/thread.h work?
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
#define LF_TIMEOUT -2

#include <inttypes.h>  // Needed to define PRId64 and PRIu32
#define PRINTF_TIME "%" PRId64
#define PRINTF_MICROSTEP "%" PRIu32

// For convenience, the following string can be inserted in a printf
// format for printing both time and microstep as follows:
//     printf("Tag is " PRINTF_TAG "\n", time_value, microstep);
#define PRINTF_TAG "(%" PRId64 ", %" PRIu32 ")"
// The underlying physical clock for Linux
#define _LF_CLOCK CLOCK_MONOTONIC

// FIXME: Where do I get this info?
#define N_WORKERS 4

typedef resource_t _lf_mutex_t;
typedef resource_t _lf_thread_t;        // Type to hold handle to a thread

typedef struct {
    bool signal;
    bool waiting;
} cond_elem_t;

typedef struct {
    cond_elem_t waiter[N_WORKERS];
} _lf_cond_t;            // Type to hold handle to a condition variable
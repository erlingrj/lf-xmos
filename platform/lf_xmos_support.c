#include "lf_xmos_support.h"
#include "lf_platform.h"

#include <stdint.h>
#include <stdlib.h>

#include <platform.h>
#include <xcore/hwtimer.h>
#include <xcore/thread.h>
#include <xcore/interrupt.h>
#include <xcore/assert.h>
#include "swlock.h"

// HW timer 
static hwtimer_t lf_timer;
static int32_t time_hi = 0;
static uint32_t last_time = 0;

// Thread stack
// FIXME: How big should it be?
#define N_WORKERS 4
#define STACK_WORDS_PER_THREAD 256
#define STACK_BYTES_PER_WORD 4

typedef void *(*lf_function_t) (void *);

typedef struct {
    void *stack_base_ptr;
    xthread_t xthread_id;
    bool running;
} thread_info_t;
static thread_info_t thread_info[N_WORKERS];

// Return first available thread info. 
// -1 on failure. Else the idx of the thread
static int get_available_thread() {
    for (int i = 0; i<N_WORKERS; i++) {
        if (!thread_info[i].running) {
            return i;
        }
    }
    return -1;
}

static void return_thread(thread_info_t *tinfo) {
    xassert(tinfo);
    //FIXME: Malloc+Free might cause issues?
    free(tinfo->stack_base_ptr);
    tinfo->running = false;
}

// Starting point for all threads

typedef struct {
    lf_function_t func;
    void * args;
} function_and_arg_t ;

void thread_function(void * args) {
    function_and_arg_t *func_and_arg = (function_and_arg_t *) args;
    func_and_arg->func(func_and_arg->args);
}

// FIXME: Return the number specified by the user
int lf_available_cores() {
    return N_WORKERS;
}

/*
* This function needs to do a little hacking because the XMOS thread API is not directly usable
* 1. XMOS thread API needs to 
*/
int lf_thread_create(lf_thread_t* thread, void *(*lf_thread) (void *), void* arguments) {
    int idx = get_available_thread();
    xassert(idx >= 0);

    thread_info_t * tinfo = &thread_info[idx];

    // FIXME: What is appropriate stack size? Is there a safe and guaranteed way to do this?
    //  Should maybe have a statically allocated stack area? Or take stack requirements as arg?
    tinfo->stack_base_ptr = malloc(STACK_WORDS_PER_THREAD*STACK_BYTES_PER_WORD);
    void *stack_ptr = stack_base(tinfo->stack_base_ptr,STACK_WORDS_PER_THREAD);

    // FIXME: Is this memory safe? This structure is on the stack and will be removed when function returns
    //  not really sure
    function_and_arg_t func_and_arg = {lf_thread, arguments};

    xthread_t thread_ptr = xthread_alloc_and_start(thread_function, (void *) &func_and_arg, stack_ptr);
    if (thread_ptr) {
        tinfo->xthread_id = thread_ptr;
        tinfo->running = true;
        *thread = idx;
        return 0; 
    } else {
        return -1;
    }
}

/**
 * Make calling thread wait for termination of the thread.  The
 * exit status of the thread is stored in thread_return, if thread_return
 * is not NULL.
 * 
 * @return 0 on success, platform-specific error number otherwise.
 */
int lf_thread_join(lf_thread_t thread, void** thread_return) {
    xassert(thread >= 0);
    xassert(thread < N_WORKERS);
    thread_info_t *tinfo = &thread_info[thread];
    xassert(tinfo->running);

    xthread_wait_and_free(tinfo->xthread_id);
    return_thread(tinfo);
    return 0;
}

/** 
 * Initialize a conditional variable.
 * 
 * @return 0 on success, platform-specific error number otherwise.
 */
// FIXME: Can we use the SW Locks instead of CondVars here?
int lf_cond_init(lf_cond_t* cond) {
    xassert(cond);
    swlock_init(cond);
    return 0;
}

/** 
 * Wake up all threads waiting for condition variable cond.
 * 
 * @return 0 on success, platform-specific error number otherwise.
 */
int lf_cond_broadcast(lf_cond_t* cond) {
    xassert(cond);
    swlock_release(cond);
    return 0;
}

/** 
 * Wake up one thread waiting for condition variable cond.
 * 
 * @return 0 on success, platform-specific error number otherwise.
 */
int lf_cond_signal(lf_cond_t* cond) {
    xassert(cond);
    swlock_release(cond);
    return 0;
}
/** 
 * Wait for condition variable "cond" to be signaled or broadcast.
 * "mutex" is assumed to be locked before.
 * 
 * @return 0 on success, platform-specific error number otherwise.
 */
int lf_cond_wait(lf_cond_t* cond, lf_mutex_t* mutex) {
    xassert(cond);
    swlock_acquire(cond);
    return 0;
}

/** 
 * Block current thread on the condition variable until condition variable
 * pointed by "cond" is signaled or time pointed by "absolute_time_ns" in
 * nanoseconds is reached.
 * 
 * @return 0 on success, LF_TIMEOUT on timeout, and platform-specific error
 *  number otherwise.
 */
int lf_cond_timedwait(lf_cond_t* cond, lf_mutex_t* mutex, instant_t absolute_time_ns) {
    xassert(cond);
    instant_t now;
    int lock_acquired;
    do {
        lock_acquired = swlock_try_acquire(cond);
        lf_clock_gettime(&now);
    } while (lock_acquired == SWLOCK_NOT_ACQUIRED && now < absolute_time_ns);

    if (lock_acquired == SWLOCK_NOT_ACQUIRED) {
        return LF_TIMEOUT;
    } else {
        return 0;
    }
}

void lf_initialize_clock(void) {
    lf_timer = hwtimer_alloc();
}

int lf_clock_gettime(instant_t* t) {
    xassert(t);
    uint32_t now = hwtimer_get_time(lf_timer);
    if (now < last_time) {
        // Overflow has occurred 
        time_hi++;
    }
    int64_t now_ext = (((int64_t) time_hi) << 32) | ((int64_t) now);

    *t = now_ext*10;
    last_time = now;

    return 0;
}

// FIXME: We should probably also wait for a signal from lf_notify event
//  however. Is that API used in threaded impl also? 
int lf_sleep(interval_t sleep_duration) {
    uint64_t sleep_xmos_ticks = sleep_duration/10;
    
    while(sleep_xmos_ticks > UINT32_MAX) {
        hwtimer_delay(lf_timer, UINT32_MAX);
        sleep_xmos_ticks -= UINT32_MAX;
    }

    hwtimer_delay(lf_timer, (uint32_t) sleep_xmos_ticks);
    return 0;
}


int lf_sleep_until(instant_t wakeup_time) {
    instant_t now;
    lf_clock_gettime(&now);
    instant_t sleep_duration = wakeup_time - now;
    lf_sleep(sleep_duration);
    return 0;
}

int lf_critical_section_enter() {
    interrupt_mask_all();
    return 0;
}

int lf_critical_section_exit() {
    interrupt_unmask_all();
    return 0;
}

//FIXME: This is how physical actions can enter the LF system from external threads
int lf_notify_of_event() {
    return 0;
}
#include "lf_xmos_support.h"
#include "lf_platform.h"

#include <stdint.h>
#include <stdlib.h>

#include <platform.h>
#include <xcore/hwtimer.h>
#include <xcore/thread.h>
#include <xcore/assert.h>
#include <xcore/lock.h>
#include <xcore/interrupt.h>
#include <xcore/select.h>



// HW timer 
static hwtimer_t lf_timer;
static int32_t time_hi = 0;
static uint32_t last_time = 0;

// FIXME: Return the number specified by the user
int lf_available_cores() {
    return 1;
}
// Thread stack
// FIXME: How big should it be?

#ifdef NUMBER_OF_WORKERS
#define XMOS_MAX_NUMBER_OF_THREADS 8
#define STACK_WORDS_PER_THREAD 256
#define STACK_BYTES_PER_WORD 4

static int get_tid() {
    int result;
    asm ("get r11, id" ::);
    asm ("mov %0, r11" :"=r"(result):);

    xassert(result < NUMBER_OF_THREADS);
    return result;
}

// One and only mutex
lf_mutex_t mutex;
lf_cond_t event_q_changed;

typedef void *(*lf_function_t) (void *);

typedef struct {
    void *stack_base_ptr;
    xthread_t xthread_id;
    bool running;
} thread_info_t;
static thread_info_t thread_info[NUMBER_OF_THREADS];

static lock_t atomics_lock;
#endif

void lf_initialize_clock(void) {
    lf_timer = hwtimer_alloc();

    //FIXME: This does not belong here really :(
    #ifdef NUMBER_OF_WORKERS
        atomics_lock = lock_alloc();
    #endif
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
    xassert(false);
    interrupt_mask_all();
    return 0;
}

int lf_critical_section_exit() {
    xassert(false);
    interrupt_unmask_all();
    return 0;
}

//FIXME: This is how physical actions can enter the LF system from external threads
int lf_notify_of_event() {
    xassert(false);
    return 0;
}

#ifdef NUMBER_OF_WORKERS
// Return first available thread info. 
// -1 on failure. Else the idx of the thread
static int get_available_thread() {
    for (int i = 0; i<NUMBER_OF_THREADS; i++) {
        if (!thread_info[i].running) {
            return i;
        }
    }
    return -1;
}

static cond_elem_t* get_available_cond(lf_cond_t *cond) {
    for (int i = 0; i<NUMBER_OF_THREADS; i++) {
        if (!cond->waiter[i].waiting) {
            return &cond->waiter[i];
        }
    }
    return NULL;
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
    xassert(thread < NUMBER_OF_THREADS);
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
int lf_cond_init(lf_cond_t* cond) {
    xassert(cond);
    
    for (int i = 0; i<NUMBER_OF_THREADS; i++) {
        cond->waiter[i].signal = false;
        cond->waiter[i].waiting = false;
        cond->waiter[i].chan = chanend_alloc();
        xassert(cond->waiter[i].chan);
    }
    cond->signal_chan = chanend_alloc();
    xassert(cond->signal_chan);
    return 0;
}

/** 
 * Wake up all threads waiting for condition variable cond.
 * 
 * @return 0 on success, platform-specific error number otherwise.
 */
int lf_cond_broadcast(lf_cond_t* cond) {
    xassert(cond);
    for (int i = 0; i<NUMBER_OF_THREADS; i++) {
        cond_elem_t *waiter = &cond->waiter[i];
        if (waiter->waiting) {
            waiter->signal = true;
            chanend_set_dest(cond->signal_chan, waiter->chan);
            chanend_out_control_token(cond->signal_chan, 0x1);
        }
    }

    return 0;
}

/** 
 * Wake up one thread waiting for condition variable cond.
 * 
 * @return 0 on success, platform-specific error number otherwise.
 */
int lf_cond_signal(lf_cond_t* cond) {
    xassert(cond);
    
    for (int i = 0; i<NUMBER_OF_THREADS; i++) {
        cond_elem_t *waiter = &cond->waiter[i];
        if (waiter->waiting) {
            waiter->signal = true;
            chanend_set_dest(cond->signal_chan, waiter->chan);
            chanend_out_control_token(cond->signal_chan, 0x1);
            return 0;
        }
    }
    
    return 0;
}
/** 
 * Wait for condition variable "cond" to be signaled or broadcast.
 * "mutex" is assumed to be locked before.
 * 
 * @return 0 on success, platform-specific error number otherwise.
 */
int lf_cond_wait(lf_cond_t* cond, lf_mutex_t* mutex) {
    xassert(cond && mutex);

    // Find available waiting spot
    cond_elem_t *waiter = get_available_cond(cond);
    xassert(waiter != NULL);
    waiter->waiting = true;
    lf_mutex_unlock(mutex);
    // Wait for signal
    char in = chanend_in_control_token(waiter->chan);
    xassert(in == 0x1);
    
    lf_mutex_lock(mutex);
    
    waiter->signal = false;
    waiter->waiting = false;

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
 //FIXME: Busy-polling here is really inefficient. But what are our alternatives:

int lf_cond_timedwait(lf_cond_t* cond, lf_mutex_t* mutex, instant_t absolute_time_ns) {
    xassert(cond && mutex);
    
    // Find available waiter spot
    cond_elem_t *waiter = get_available_cond(cond);
    xassert(waiter != NULL);
    
    // Solve race condition with cond signal being stuck in channel
    if(waiter->signal) {
        SELECT_RES(
            CASE_THEN(waiter->chan, old_signal_handler),
            DEFAULT_THEN(fault))
        {
            old_signal_handler:
            {
                char in = chanend_in_control_token(waiter->chan);
                xassert(in == 0x1);
                break;
            }
            fault:
            {
                xassert(false);
                break;
            }
        }
        waiter->signal = false;
    }

    waiter->waiting = true;
    lf_mutex_unlock(mutex);
    // Wait for timeout or signal
    // FIXME: What if we dont get the timer. While loop and wait
    hwtimer_t t = hwtimer_alloc();
    xassert(t);

    // FIXME: Check for overflows
    hwtimer_set_trigger_time(t, (uint32_t) absolute_time_ns/10);

    bool timeout = false;
    SELECT_RES(
    CASE_THEN(t, timer_handler),
    CASE_THEN(waiter->chan, signal_handler))
    {
    timer_handler:
    {
        timeout = true;  
        break; //FIXME: Verify that break rather than continue is appropriate
    }
    signal_handler:
    {
        char in = chanend_in_control_token(waiter->chan);
        xassert(in == 0x1);
        break;
    }
    }
    
    lf_mutex_lock(mutex);
    
    waiter->waiting = false;
    // FIXME: Update condVar in critical section?
    if (timeout) {
        return LF_TIMEOUT;
    } else {
        return 0;
    }
}

/**
 * Initialize a mutex.
 * 
 * @return 0 on success, platform-specific error number otherwise.
 */
 
int lf_mutex_init(lf_mutex_t* mutex) {
    xassert(mutex);
    lock_t lock = lock_alloc();
    if(lock) {
        mutex->lock = lock;
        mutex->owner = -1;
        mutex->level = 0;
        return 0;   
    } else {
        return -1;
    }
}

/**
 * Lock a mutex. Support resursive mutex
 * 
 * @return 0 on success, platform-specific error number otherwise.
 */
int lf_mutex_lock(lf_mutex_t* mutex) {
    xassert(mutex);
    int tid = get_tid();
    xassert(tid >= 0);

    if (tid == mutex->owner) {
        mutex->level++;
    } else {
        lock_acquire(mutex->lock);
        mutex->owner = tid;
        mutex->level = 1;
    }

    return 0;
}

/** 
 * Unlock a mutex.
 * 
 * @return 0 on success, platform-specific error number otherwise.
 */
int lf_mutex_unlock(lf_mutex_t* mutex) {
    xassert(mutex);
    mutex->level--;
    if (mutex->level == 0) {
        mutex->owner = -1;
        lock_release(mutex->lock);
    }

    return 0;
}


bool lf_xmos_bool_compare_and_swap(bool *ptr, bool oldval, bool newval) {
    bool res =  false;
    lock_acquire(atomics_lock);
    if (*ptr  == oldval) {
        *ptr = newval;
        res = true;
    } 
    lock_release(atomics_lock);
    return res;
}

int lf_xmos_val_compare_and_swap(int *ptr, int oldval, int newval) {
    bool res = false;
    lock_acquire(atomics_lock);
    if (*ptr  == oldval) {
        *ptr = newval;
        res = true;
    } 
    lock_release(atomics_lock);
    return res;
}

int lf_xmos_atomic_fetch_add(int *ptr, int val) {
    lock_acquire(atomics_lock);
    int res = *ptr;
    *ptr += val;
    lock_release(atomics_lock);
    return res;
}

int lf_xmos_atomic_add_fetch(int *ptr, int val) {
    lock_acquire(atomics_lock);
    int res = *ptr + val;
    *ptr = res;
    lock_release(atomics_lock);
    return res;
}


#endif
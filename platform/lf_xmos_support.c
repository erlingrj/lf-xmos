#include "lf_xmos_support.h"
#include "../platform.h"

#include <stdint.h>

#include <xcore/hwtimer.h>
#include <xcore/interrupt.h>
#include <platform.h>

static hwtimer_t timer;
static int32_t time_hi = 0;
static uint32_t last_time = 0;



void lf_initialize_clock(void) {
    timer = hwtimer_alloc();
}

int lf_clock_gettime(instant_t* t) {
    uint32_t now = hwtimer_get_time(timer);
    if (now < last_time) {
        // Overflow has occurred 
        time_hi++;
    }
    int64_t now_ext = (((int64_t) time_hi) << 32) | ((int64_t) now);

    *t = now_ext/10;
    last_time = now;

    return 0;
}

int lf_sleep(interval_t sleep_duration) {
    uint64_t sleep_xmos_ticks = sleep_duration/10;
    
    while(sleep_xmos_ticks > UINT32_MAX) {
        hwtimer_delay(timer, UINT32_MAX);
        sleep_xmos_ticks -= UINT32_MAX;
    }

    hwtimer_delay(timer, (uint32_t) sleep_xmos_ticks);
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


int lf_notify_of_event() {
    return 0;
}
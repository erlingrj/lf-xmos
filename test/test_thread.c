#include <stdio.h>

#include <xcore/thread.h>
#include <xcore/assert.h>

#include "platform/lf_xmos_support.h"
#include "platform/lf_platform.h"

bool thread_is_in_critical_section = false;

void print_stuff(void * args) {
    int val = (int) args;
    printf("Thread prints stuff. Recv = %d\n", val);
}


void synchronize(void * args) {
    swlock_t *lock = (swlock_t *) args;
    lf_cond_wait(lock);
    printf("thread enter\n");
    xassert(!thread_is_in_critical_section);
    thread_is_in_critical_section = true;
    // Stay here some timee
    lf_sleep(1000000);
    thread_is_in_critical_section = false;
    printf("thread leave\n");
    lf_cond_signal(lock);
}

void synchronize_wait(void * args) {
    swlock_t *lock = (swlock_t *) args;
    instant_t now;
    lf_clock_gettime(&now);
    lf_sleep(100);
    int res = lf_cond_timedwait(lock, NULL, now + 2000000);
    xassert(res == SWLOCK_NOT_ACQUIRED);
}

void test_single_thread() {

    lf_thread_t t_id;

    lf_thread_create(&t_id, &print_stuff, 99);
    printf("t+id=%u\n", t_id);
    lf_thread_join(t_id,0);
    printf("returned\n");
}

void run_multiple_threads() {
    lf_thread_t tid[4];
    for (int i = 0; i<5; i++) {
    
    for (int i = 0; i<4; i++) {
        lf_thread_create(&tid[i], &print_stuff,i);
    }
    for (int i = 0; i<4; i++) {
        lf_thread_join(tid[i], 0);
    }

    }

}

void test_lock() {
    swlock_t lock;
    lf_cond_init(&lock);

    lf_thread_t tid[4];
    for (int i = 0; i<4; i++) {
        lf_thread_create(&tid[i], &synchronize,&lock);
    }
    for (int i = 0; i<4; i++) {
        lf_thread_join(tid[i], 0);
    }
}

void test_timed_lock() {
    swlock_t lock;
    lf_cond_init(&lock);
    lf_thread_t t1,t2;
    lf_thread_create(&t1, &synchronize, &lock);
    lf_thread_create(&t1, &synchronize_wait, &lock);
}

int main(void) {
   lf_initialize_clock();
  //  test_single_thread();
  //  run_multiple_threads();
  test_timed_lock();
}

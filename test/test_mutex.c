#include <stdio.h>

#include <xcore/thread.h>
#include <xcore/assert.h>

#include "platform/lf_xmos_support.h"
#include "platform/lf_platform.h"

bool thread_is_in_critical_section = false;

void synchronize(void * args) {
    lf_mutex_t *mutex = (lf_mutex_t *) args;
    lf_mutex_lock(mutex);
    printf("thread enter\n");
    xassert(!thread_is_in_critical_section);
    thread_is_in_critical_section = true;
    // Stay here some timee
    lf_sleep(1000000);
    thread_is_in_critical_section = false;
    printf("thread leave\n");
    lf_mutex_unlock(mutex);
}


void test_mutex() {
    lf_mutex_t mutex;
    lf_mutex_init(&mutex);

    lf_thread_t tid[4];
    for (int i = 0; i<4; i++) {
        lf_thread_create(&tid[i], &synchronize,&mutex);
    }
    for (int i = 0; i<4; i++) {
        lf_thread_join(tid[i], 0);
    }
}

void test_recursive_mutex() {
    lf_mutex_t m;
    lf_mutex_init(&m);
    lf_mutex_lock(&m);
    lf_mutex_lock(&m);
    lf_mutex_unlock(&m);
    lf_mutex_unlock(&m);
}

int main() {
    lf_initialize_clock();
    test_mutex();
    test_recursive_mutex();
}

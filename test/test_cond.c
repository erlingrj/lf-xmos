#include <stdio.h>

#include <xcore/thread.h>
#include <xcore/assert.h>
#include <xcore/lock.h>

#include "platform/lf_xmos_support.h"
#include "platform/lf_platform.h"


typedef struct {
    lf_mutex_t* mutex;
    lf_cond_t *cond;
} args_t;

void synchronize_wait_timeout(void * args) {
    args_t *a = (args_t *) args;
    instant_t now;
    lf_clock_gettime(&now);
    lf_sleep(100);
    lf_mutex_lock(a->mutex);
    int res = lf_cond_timedwait(a->cond, a->mutex, now + 2000);
    xassert(res == LF_TIMEOUT);
    printf("wait timed out\n");
    lf_mutex_unlock(a->mutex);
}

void synchronize_wait(void * args) {
    args_t *a = (args_t *) args;
    instant_t now;
    lf_clock_gettime(&now);
    lf_mutex_lock(a->mutex);
    int res = lf_cond_timedwait(a->cond, a->mutex, now + 20000);
    xassert(res == 0);
    printf("received signal\n");
    lf_mutex_unlock(a->mutex);
}


void test_timed_wait() {
    lf_cond_t cond;
    lf_mutex_t mutex;
    lf_mutex_init(&mutex);
    lf_cond_init(&cond);
    args_t args = {&mutex, &cond};

    lf_thread_t t1;
    lf_thread_create(&t1, &synchronize_wait_timeout, &args);
    lf_thread_join(t1, NULL);
    lock_free(mutex.lock);
}

void test_timed_wait_no_timeout() {
    lf_cond_t cond;
    lf_mutex_t mutex;
    lf_mutex_init(&mutex);
    lf_cond_init(&cond);
    args_t args = {&mutex, &cond};

    lf_thread_t t1;
    lf_thread_create(&t1, &synchronize_wait, &args);
    lf_sleep(1000);
    lf_cond_broadcast(&cond);
    lf_thread_join(t1, NULL);
    lock_free(mutex.lock);
}

void wait(void * args) {
    args_t *a = (args_t *) args;
    lf_mutex_lock(a->mutex);
    printf("Waiter has mutex. Wait for cond \n");
    lf_cond_wait(a->cond, a->mutex);
    printf("waiter has been signalled\n");
    lf_mutex_unlock(a->mutex);
}

void signal(void * args) {
    args_t *a = (args_t *) args;
    lf_mutex_lock(a->mutex);
    printf("Signal has mutex. \n");
    lf_cond_signal(a->cond);
    printf("Has signalled\n");
    lf_mutex_unlock(a->mutex);
}

void broadcast(void * args) {
    args_t *a = (args_t *) args;
    lf_mutex_lock(a->mutex);
    printf("Signal has mutex. \n");
    lf_cond_broadcast(a->cond);
    printf("Has signalled\n");
    lf_mutex_unlock(a->mutex);
}

void test_single_wait_and_signal() {
lf_cond_t cond;
lf_mutex_t mutex;
lf_mutex_init(&mutex);
lf_cond_init(&cond);
args_t args = {&mutex, &cond};

lf_thread_t t1,t2;


    lf_thread_create(&t1, &wait, &args);
    lf_thread_create(&t2, &signal, &args);
    lf_thread_join(t1, NULL);
    lf_thread_join(t2, NULL);
    lock_free(mutex.lock);

}

void test_multiple_wait_and_broadcast() {

lf_cond_t cond;
lf_mutex_t mutex;
lf_mutex_init(&mutex);
lf_cond_init(&cond);
args_t args = {&mutex, &cond};

lf_thread_t t1,t2,t3,t4;


    lf_thread_create(&t1, &wait, &args);
    lf_thread_create(&t3, &wait, &args);
    lf_thread_create(&t4, &wait, &args);
    lf_thread_create(&t2, &broadcast, &args);
    lf_thread_join(t1, NULL);
    lf_thread_join(t2, NULL);
    lf_thread_join(t3, NULL);
    lf_thread_join(t4, NULL);
    lock_free(mutex.lock);

}

int main() {
    lf_initialize_clock();
    test_timed_wait_no_timeout();
    test_multiple_wait_and_broadcast();
    test_single_wait_and_signal();
    test_timed_wait();
}
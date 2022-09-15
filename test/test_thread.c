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


int main(void) {
   lf_initialize_clock();
   test_single_thread();
   run_multiple_threads();
}

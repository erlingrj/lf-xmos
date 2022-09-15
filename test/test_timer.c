#include <stdio.h>

#include "platform/lf_xmos_support.h"
#include "platform/lf_platform.h"

int main(void) {

    instant_t now;
    lf_initialize_clock();
    lf_clock_gettime(&now);

    printf("time is %lli\n", now);

    instant_t sleep_for = 1000;
    lf_sleep(sleep_for);
    lf_clock_gettime(&now);

    printf("time is %lli\n", now);

    return 0;
}

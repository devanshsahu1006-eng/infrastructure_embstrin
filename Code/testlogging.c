#include <stdio.h>
#include "embstrin_logging.h"
#include "embstrin_logging.c"

int main(void)
{
    // setup
    EMBSTRIN_LogConfig config = {
        .min_level     = EMBSTRIN_LOG_DEBUG,  // show everything
        .flush_freq_hz = 10
    };
    EMBSTRIN_LOG_init(&config);

    // test host logs
    EMBSTRIN_LOG_host(EMBSTRIN_LOG_DEBUG, "TASK_SUBMITTED task_id=1");
    EMBSTRIN_LOG_host(EMBSTRIN_LOG_INFO,  "TASK_ASSIGNED task_id=1 device_id=2");
    EMBSTRIN_LOG_host(EMBSTRIN_LOG_WARN,  "DEVICE_QUEUE_FULL device_id=2");
    EMBSTRIN_LOG_host(EMBSTRIN_LOG_ERROR, "TASK_FAILED task_id=1");

    // test device log
    EMBSTRIN_LOG_device(2, 83441, EMBSTRIN_LOG_INFO, "TASK_STARTED task_id=1");

    // flush
    EMBSTRIN_LOG_flush();

    return 0;
}
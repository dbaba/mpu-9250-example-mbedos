#include "mbed-drivers/mbed.h"

#define ASYNC 1

#if ASYNC
    #include "mpu-9250/motion_async.hpp"
#else
    #include "mpu-9250/motion_sync.hpp"
#endif

static void blinky(void) {
    static DigitalOut led(LED1);
    led = !led;
}

void app_start(int, char**) {
#if ASYNC
    mpu9250_async_task_init();
    minar::Scheduler::postCallback(mpu9250_async_task);
#else
    mpu9250_sync_task_init();
    minar::Scheduler::postCallback(mpu9250_sync_task).period(minar::milliseconds(200));
#endif
    minar::Scheduler::postCallback(blinky).period(minar::milliseconds(1000));
}

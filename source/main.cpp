#include <memory>
#include "mbed.h"
#include "mpu-9250/motion_sync.hpp"

static auto pc = std::make_shared<Serial>(USBTX, USBRX);

static void blinky(void) {
    static DigitalOut led(LED1);
    while (true) {
        led = !led;
        Thread::wait(500);
    }
}

static void mpu9250_task(void) {
    while (true) {
        mpu9250_sync_task();
        Thread::wait(1);
    }
}

int main(int, char**) {
    pc->baud(115200);

    mpu9250_sync_task_init();

    Thread blinky_task(blinky);
    Thread mpu9250_sync_task(mpu9250_task);
    mpu9250_sync_task.join();
    return 0;
}

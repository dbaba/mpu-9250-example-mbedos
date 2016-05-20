#include "mpu-9250/motion_async.hpp"

// I2C1 port, I2C Bus 1
static mbed::drivers::v2::I2C i2cAsyncBus(PB_9, PB_8);

// MPU9250
static MPU9250Async* motion_sensor;

void mpu9250_async_task(void) {
    printf("%s\r\n", "OK. mpu9250_async_task() started!");
}

static void mpu9250_async_task_who_am_i(void) {
    motion_sensor->whoAmI1([](i2c_async::StatusCode status, uint8_t data) {
        if (status == i2c_async::OK && data == 0x71) {
            printf("%s\r\n", "MPU-9250 WHO_AM_I is SUCCESS!!!");
            motion_sensor->initAllAsync([](i2c_async::StatusCode status) {
                if (status == i2c_async::OK) {
                    minar::Scheduler::postCallback(mpu9250_async_task);
                } else {
                    printf("%s\r\n", "Error. Something wrong");
                }
            });
        } else {
            printf("%s status:%d\r\n", "MPU-9250 WHO_AM_I is FAAAAAAAAAIL....", status);
            minar::Scheduler::postCallback(mpu9250_async_task_who_am_i).delay(minar::milliseconds(100));
        }
    });
}

void mpu9250_async_task_init(void) {
    i2cAsyncBus.frequency(400000);
    motion_sensor = new MPU9250Async(&i2cAsyncBus, 1);
    mpu9250_async_task_who_am_i();
}

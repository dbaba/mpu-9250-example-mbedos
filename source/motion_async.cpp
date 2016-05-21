#include "mpu-9250/motion_async.hpp"

// I2C1 port, I2C Bus 1
static mbed::drivers::v2::I2C i2cAsyncBus(PB_9, PB_8);

// MPU9250
static MPU9250Async* motion_sensor;

void mpu9250_async_task(void) {
    motion_sensor->getAccelGyroAsync([](i2c_async::StatusCode status, std::size_t, uint8_t* byte_vals) {
        if (status != i2c_async::OK) {
            printf("[ERROR] <ACCEL/GYRO> Status=%d\r\n", status);
            return;
        }
        float* vals = (float*) byte_vals;
        printf("%s\r\n", "========================================================");
        printf("[ACCEL (m/s2)] x:%11.6f y:%11.6f z:%11.6f\r\n", vals[0], vals[1], vals[2]);
        printf("[GYRO (rad/s)] x:%11.6f y:%11.6f z:%11.6f\r\n", vals[3], vals[4], vals[5]);
        delete[] byte_vals;

        motion_sensor->getMagAsync([](i2c_async::StatusCode status, std::size_t, uint8_t* byte_vals) {
            if (status != i2c_async::OK) {
                printf("[ERROR] <MAG> Status=%d\r\n", status);
                return;
            }
            float* vals = (float*) byte_vals;
            printf("[MAG (mG)    ] x:%11.6f y:%11.6f z:%11.6f\r\n", vals[0], vals[1], vals[2]);
            delete[] byte_vals;

            uint8_t q[4 * 4]; // float = 4 bytes
            motion_sensor->performMadgwickQuaternionUpdate(q);
            vals = (float*) q;
            printf("[QUARTERNION ] w:%11.6f x:%11.6f y:%11.6f z:%f\r\n", vals[0], vals[1], vals[2], vals[3]);

            minar::Scheduler::postCallback(mpu9250_async_task).delay(minar::milliseconds(200));
        });
    });
}

static void mpu9250_async_task_who_am_i(void) {
    motion_sensor->whoAmI1([](i2c_async::StatusCode status, uint8_t data) {
        if (status == i2c_async::OK && data == 0x71) {
            printf("%s\r\n", "MPU-9250 WHO_AM_I is SUCCESS!!!");
            motion_sensor->setMagBias(
                299.809, // +North(-South) (mG)
                -387.89, // +East(-West) (mG)
                353.871  // +Down(-Up) (mG)
            );
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

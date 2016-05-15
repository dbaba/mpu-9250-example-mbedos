#include "mbed-drivers/mbed.h"
#include "mpu-9250/MPU9250.hpp"

// I2C1 port, I2C Bus 1
static I2C i2c(PB_9, PB_8);

// MPU9250
static MPU9250* motion_sensor;

static void blinky(void) {
    static DigitalOut led(LED1);
    led = !led;
}

static void mpu9250_init(MPU9250* sensor) {
    if (sensor->whoAmI1() != 0x71) {
        return;
    }

    sensor->initAll();
}

static void mpu9250_collect_data(MPU9250* sensor, uint8_t *data_store) {
    if (sensor->isInitialized()) {
        sensor->getAccelGyro(data_store); // float [0:5]
    } else {
        mpu9250_init(sensor);
    }
}

static void ak8963_collect_data(MPU9250* sensor, uint8_t *data_store) {
    if (sensor && sensor->isInitialized()) {
        sensor->getMag(data_store); // float [0:2]
        sensor->performMadgwickQuaternionUpdate((uint8_t*) &(((float*) data_store)[3])); // float [3:6]
    }
}

static void mpu9250_task(void) {
    uint8_t byte_vals[4 * 7];
    float* vals;
    mpu9250_collect_data(motion_sensor, byte_vals);
    vals = (float*) byte_vals;
    printf("%s\r\n", "========================================================");
    printf("[ACCEL (m/s2)] x:%11.6f y:%11.6f z:%11.6f\r\n", vals[0], vals[1], vals[2]);
    printf("[GYRO (rad/s)] x:%11.6f y:%11.6f z:%11.6f\r\n", vals[3], vals[4], vals[5]);
    ak8963_collect_data(motion_sensor, byte_vals);
    vals = (float*) byte_vals;
    printf("[MAG (mG)    ] x:%11.6f y:%11.6f z:%11.6f\r\n", vals[0], vals[1], vals[2]);
    printf("[QUARTERNION ] w:%11.6f x:%11.6f y:%11.6f z:%f\r\n", vals[3], vals[4], vals[5], vals[6]);
}

void app_start(int, char**) {
    i2c.frequency(400000);
    motion_sensor = new MPU9250(&i2c, 1);
    minar::Scheduler::postCallback(mpu9250_task).period(minar::milliseconds(200));
    minar::Scheduler::postCallback(blinky).period(minar::milliseconds(1000));
}

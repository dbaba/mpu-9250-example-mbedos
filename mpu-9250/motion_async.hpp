#pragma once

#include "mbed-drivers/mbed.h"
#include "mpu-9250/MPU9250Async.hpp"

void mpu9250_async_task_init(void);

void mpu9250_async_task(void);

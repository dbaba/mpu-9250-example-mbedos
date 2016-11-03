#pragma once

#include "mbed.h"
#include "mpu-9250/MPU9250.hpp"

void mpu9250_sync_task_init(void);

void mpu9250_sync_task(void);

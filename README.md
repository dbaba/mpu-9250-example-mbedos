mpu-9250-example
====

[![GitHub release](https://img.shields.io/github/release/dbaba/mpu-9250-example-mbedos.svg)](https://github.com/dbaba/mpu-9250-example-mbedos/releases/latest)
[![License MIT](https://img.shields.io/github/license/dbaba/mpu-9250-example-mbedos.svg)](http://opensource.org/licenses/MIT)

InvenSense 9-Axis Sensor MPU-9250 Example on mbed OS

MPU9250.hpp is originally available as MPU9250.h at https://developer.mbed.org/users/onehorse/code/MPU9250AHRS/file/4e59a37182df/MPU9250.h.

I also referred to the following projects in order to append some functions to MPU9250.hpp:
1. https://github.com/kriswiner/MPU-6050/wiki/Simple-and-Effective-Magnetometer-Calibration and https://github.com/1994james/9250-code/blob/master/_9250_code.ino for calibration functions
1. https://github.com/kriswiner/MPU-9250 and https://github.com/kriswiner/MPU-6050 for Quarternion functions

# Setup instruction for ST-Microsystems Nucleo

    $ yt target st-nucleo-f401re-gcc
    $ yt install

# USB Serial Baud rate

Set 115200 bps in order to connect to the USB serial port. The baud rate is set in config.json file.

# Revision History
* 1.0.0
    - Initial Release

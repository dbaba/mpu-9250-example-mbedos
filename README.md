mpu-9250-example
====

[![GitHub release](https://img.shields.io/github/release/dbaba/mpu-9250-example-mbedos.svg)](https://github.com/dbaba/mpu-9250-example-mbedos/releases/latest)
[![License MIT](https://img.shields.io/github/license/dbaba/mpu-9250-example-mbedos.svg)](http://opensource.org/licenses/MIT)

InvenSense 9-Axis Sensor MPU-9250 Example on mbed OS 5

MPU9250.hpp is originally available as MPU9250.h at https://developer.mbed.org/users/onehorse/code/MPU9250AHRS/file/4e59a37182df/MPU9250.h.

I also referred to the following projects in order to append some functions to MPU9250.hpp:

1. https://github.com/kriswiner/MPU-6050/wiki/Simple-and-Effective-Magnetometer-Calibration and https://github.com/1994james/9250-code/blob/master/_9250_code.ino for calibration functions
1. https://github.com/kriswiner/MPU-9250 and https://github.com/kriswiner/MPU-6050 for Quarternion functions

# Setup instruction for ST-Microsystems Nucleo F411RE

    $ make
    $ make deploy # if you install st-link command or just copy NUCLEO_F411RE/GCC_ARM/mpu-9250-example-mbedos/bin to mbed USB folder

You can change a target and a toolchain by `mbed` CLI command though Nucleo F411RE and ARM GCC are chosen by default.

This project requires C11 and C++11, which are enabled by profile files under `profiles` directory. Specify `--profile profiles/default.json` (or `--profile profiles/debug.json`) when you run `mbed` CLI command directly.

# I2C slave address

According to `AD0` state on the sensor, the slave address differs. If you AD0=Low, you need to set `0` for the `AD0` definition in `mpu-9250/MPU9250-common.hpp` file.

# USB Serial Baud rate

Set 115200 bps in order to connect to the USB serial port. The baud rate is set in config.json file.

# Output Example

```
========================================================
[ACCEL (m/s2)] x:  -0.002394 y:   0.007183 z:  -7.371747
[GYRO (rad/s)] x:   0.228499 y:   0.251402 z:   6.455236
[MAG (mG)    ] x:  47.211525 y:   0.000001 z: -60.398273
[QUARTERNION ] w:  -0.129923 x:  -0.674243 y:   0.724474 z:-0.060444
========================================================
[ACCEL (m/s2)] x:   0.000000 y:   0.000000 z:  -9.806650
[GYRO (rad/s)] x:   0.000000 y:   0.000000 z:   0.000000
[MAG (mG)    ] x: -52.876907 y:  -1.888460 z:  49.557549
[QUARTERNION ] w:  -0.117985 x:  -0.668982 y:   0.730648 z:-0.068531
========================================================
[ACCEL (m/s2)] x:   0.000000 y:   0.000000 z:  -9.806650
[GYRO (rad/s)] x:   0.000000 y:   0.000000 z:   0.000000
[MAG (mG)    ] x:  56.653831 y:  -1.888460 z: -58.849602
[QUARTERNION ] w:  -0.123682 x:  -0.680730 y:   0.719802 z:-0.056525
========================================================
[ACCEL (m/s2)] x:   0.014365 y:   0.007183 z:  -7.331046
[GYRO (rad/s)] x:   0.184291 y:  -0.011185 z:   6.611297
[MAG (mG)    ] x:  49.099983 y:  -9.442303 z: -55.752254
[QUARTERNION ] w:  -0.127998 x:  -0.750919 y:   0.646543 z:-0.041458
```

# Revision History
* ?????
    - Rewrite the project for applying mbed OS 5
    - Async API is currently not available
    - mbed OS 3 source code is available on `mbed-os-3.0` branch

* 1.1.0
    - [I2C Asynchronous API](https://docs.mbed.com/docs/getting-started-mbed-os/en/latest/Full_Guide/I2C/) support
    - NOTICE) The mbed OS I2C Asynchronous API is still unstable, the project MAY not work on mbed OS in the future

* 1.0.0
    - Initial Release

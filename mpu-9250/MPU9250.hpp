// https://developer.mbed.org/users/onehorse/code/MPU9250AHRS/file/4e59a37182df/MPU9250.h
#ifndef MPU9250_H
#define MPU9250_H

#include "mbed-drivers/mbed.h"
#include <math.h>
#include "mpu-9250/MPU9250-common.hpp"

class MPU9250 {
    I2C* _i2c;
    uint8_t _busId;
    uint8_t _Ascale = AFS_2G;               // AFS_2G, AFS_4G, AFS_8G, AFS_16G
    uint8_t _Gscale = GFS_250DPS;           // GFS_250DPS, GFS_500DPS, GFS_1000DPS, GFS_2000DPS
    uint8_t _Mscale = MFS_16BITS;           // MFS_14BITS or MFS_16BITS, 14-bit or 16-bit magnetometer resolution
    uint8_t _Mmode = 0x06;                  // Either 8 Hz (0x02/Continuous measurement mode 1) or 100 Hz (0x06/Continuous measurement mode 2) magnetometer data ODR
    float _aRes, _gRes, _mRes;              // scale resolutions per LSB for the sensors

    float _a[3], _g[3], _m[3];              // variables to hold latest sensor data values

    float _deltat = 0.0f;                   // integration interval for both filter schemes
    uint32_t _lastUpdate = 0;
    float _q[4] = {1.0f, 0.0f, 0.0f, 0.0f}; // vector to hold quaternion (x, y , z, w) in NED
    float _eInt[3] = {0.0f, 0.0f, 0.0f};    // vector to hold integral error for Mahony method

    uint8_t _initialized = 0;
    float _magCalibration[3] = {0, 0, 0}; // (uT, mG = uT * 10)

    // Set the expected magnetic fields depending on your location
    // http://www.ngdc.noaa.gov/geomag-web/#igrfwmm (in nT, mG = nT / 100)
    float _magBias[3] = {
        0, // +North(-South) (mG)
        0, // +East(-West) (mG)
        0  // +Down(-Up) (mG)
    };

    float _magScale[3] = {0, 0, 0};
    float _gyroBias[3] = {0, 0, 0};     // (degree/sec)
    float _accelBias[3] = {0, 0, 0};    // (g)

    Timer _timer;

public:
    MPU9250(I2C* i2c, uint8_t busId): _i2c(i2c), _busId(busId) {
        getAres();
        getGres();
        getMres();
        _timer.start();
    }

    I2C* getI2C(void) {
      return _i2c;
    }

    void initAll(void) {
        if (_initialized) {
            return;
        }
        resetMPU9250();
        accelgyrocalMPU9250();
        initMPU9250();
        initAK8963();
        magcalMPU9250();
        setInitialized();
    }

    uint8_t isInitialized(void) {
        return _initialized;
    }
    void setInitialized(void) {
        _initialized = 1;
    }

    uint8_t whoAmI1(void) {
        uint8_t whoami = readByte(MPU9250_ADDRESS, WHO_AM_I_MPU9250);  // Read WHO_AM_I register for MPU-9250
        return whoami;
    }

    uint8_t whoAmI2(void) {
        uint8_t whoami = readByte(AK8963_ADDRESS, WHO_AM_I_AK8963);  // Read WHO_AM_I register for AK-8963
        return whoami;
    }

    uint8_t getBusId(void) {
        return _busId;
    }

    /*
     * Set magnetometer bias values prior to initAll() call
     * biasX ... +North(-South) (mG)
     * biasY ... +East(-West) (mG)
     * biasZ ... +Down(-Up) (mG)
     */
    void setMagBias(float biasX, float biasY, float biasZ) {
        _magBias[0] = biasX;
        _magBias[1] = biasY;
        _magBias[2] = biasZ;
    }

    //===================================================================================================================
    //====== Set of useful function to access acceleratio, gyroscope, and temperature data
    //===================================================================================================================

    void writeByte(uint8_t address, uint8_t subAddress, uint8_t data) {
        char data_write[2];
        data_write[0] = subAddress;
        data_write[1] = data;
        _i2c->write(address, data_write, 2, 0);
    }

    char readByte(uint8_t address, uint8_t subAddress) {
        char data[1]; // `data` will store the register data
        char data_write[1];
        data_write[0] = subAddress;
        _i2c->write(address, data_write, 1, 1); // no stop
        _i2c->read(address, data, 1, 0);
        return data[0];
    }

    void readBytes(uint8_t address, uint8_t subAddress, uint8_t count, uint8_t * dest) {
        char data[14];
        char data_write[1];
        data_write[0] = subAddress;
        _i2c->write(address, data_write, 1, 1); // no stop
        _i2c->read(address, data, count, 0);
        for(int ii = 0; ii < count; ii++) {
            dest[ii] = data[ii];
        }
    }

    void getMres() {
        switch (_Mscale)
        {
            // Possible magnetometer scales (and their register bit settings) are:
            // 14 bit resolution (0) and 16 bit resolution (1)
            case MFS_14BITS:
                _mRes = 10.0*4912.0/8190.0; // Proper scale to return milliGauss
                break;
            case MFS_16BITS:
                _mRes = 10.0*4912.0/32760.0; // Proper scale to return milliGauss
                break;
        }
    }

    void getGres() {
        switch (_Mscale)
        {
            // Possible gyro scales (and their register bit settings) are:
            // 250 DPS (00), 500 DPS (01), 1000 DPS (10), and 2000 DPS    (11).
                    // Here's a bit of an algorith to calculate DPS/(ADC tick) based on that 2-bit value:
            case GFS_250DPS:
                _gRes = 250.0/32768.0;
                break;
            case GFS_500DPS:
                _gRes = 500.0/32768.0;
                break;
            case GFS_1000DPS:
                _gRes = 1000.0/32768.0;
                break;
            case GFS_2000DPS:
                _gRes = 2000.0/32768.0;
                break;
        }
    }

    void getAres() {
        switch (_Ascale)
        {
            // Possible accelerometer scales (and their register bit settings) are:
            // 2 Gs (00), 4 Gs (01), 8 Gs (10), and 16 Gs    (11).
                    // Here's a bit of an algorith to calculate DPS/(ADC tick) based on that 2-bit value:
            case AFS_2G:
                _aRes = 2.0/32768.0;
                break;
            case AFS_4G:
                _aRes = 4.0/32768.0;
                break;
            case AFS_8G:
                _aRes = 8.0/32768.0;
                break;
            case AFS_16G:
                _aRes = 16.0/32768.0;
                break;
        }
    }

    void transformAccelGyro(int16_t* src, uint8_t* out) {
        int8_t base = 0;
        int8_t i;
        float f;
        float* out_data = (float *) out;

        for (i = 0; i < 3; i++) {
            f = (float) src[i] * _aRes - _accelBias[i];
            _a[i] = f;
            // g to m/s*s
            out_data[base + i] = G * f;
        }
        base += 3;
        for (i = 0; i < 3; i++) {
            // Degree to Radian
            f = DEG_TO_RAD * ((float) src[base + i] * _gRes - _gyroBias[i]);
            _g[i] = f;
            out_data[base + i] = f;
        }
    }

    /* uint8_t out[4 * 6] */
    void getAccelGyro(uint8_t *out) {
        int16_t data[6];
        readAccelGyroData(data);
        transformAccelGyro(data, out);
    }

    /* uint8_t out[4 * 3] */
    void getMag(uint8_t *out) {
        int16_t data[3];
        int8_t i;
        float f;
        float* out_data = (float *) out;
        readMagData(data);
        for (i = 0; i < 3; i++) {
            // micro Tesla to milliGauss (_mRes)
            f = (float) data[i] * _mRes * _magCalibration[i] - _magBias[i];
            f *= _magScale[i];
            _m[i] = f;
            out_data[i] = f;
        }
    }

    void readAccelGyroData(int16_t * destination) {
        uint8_t rawData[14];    // x/y/z accel register data stored here
        readBytes(MPU9250_ADDRESS, ACCEL_XOUT_H, 14, &rawData[0]);    // Read the six raw data registers into data array
        destination[0] = (int16_t)(((int16_t)rawData[0] << 8) | rawData[1]) ;    // Turn the MSB and LSB into a signed 16-bit value
        destination[1] = (int16_t)(((int16_t)rawData[2] << 8) | rawData[3]) ;
        destination[2] = (int16_t)(((int16_t)rawData[4] << 8) | rawData[5]) ;
        destination[3] = (int16_t)(((int16_t)rawData[8] << 8) | rawData[9]) ;    // Turn the MSB and LSB into a signed 16-bit value
        destination[4] = (int16_t)(((int16_t)rawData[10] << 8) | rawData[11]) ;
        destination[5] = (int16_t)(((int16_t)rawData[12] << 8) | rawData[13]) ;
    }

    void readAccelData(int16_t * destination) {
        uint8_t rawData[6];    // x/y/z accel register data stored here
        readBytes(MPU9250_ADDRESS, ACCEL_XOUT_H, 6, &rawData[0]);    // Read the six raw data registers into data array
        destination[0] = (int16_t)(((int16_t)rawData[0] << 8) | rawData[1]) ;    // Turn the MSB and LSB into a signed 16-bit value
        destination[1] = (int16_t)(((int16_t)rawData[2] << 8) | rawData[3]) ;
        destination[2] = (int16_t)(((int16_t)rawData[4] << 8) | rawData[5]) ;
    }

    void readGyroData(int16_t * destination) {
        uint8_t rawData[6];    // x/y/z gyro register data stored here
        readBytes(MPU9250_ADDRESS, GYRO_XOUT_H, 6, &rawData[0]);    // Read the six raw data registers sequentially into data array
        destination[0] = (int16_t)(((int16_t)rawData[0] << 8) | rawData[1]) ;    // Turn the MSB and LSB into a signed 16-bit value
        destination[1] = (int16_t)(((int16_t)rawData[2] << 8) | rawData[3]) ;
        destination[2] = (int16_t)(((int16_t)rawData[4] << 8) | rawData[5]) ;
    }

    void readMagData(int16_t * destination) {
        uint8_t rawData[7];    // x/y/z gyro register data, ST2 register stored here, must read ST2 at end of data acquisition
        if(((_Mmode & 0x01) == 0) || (readByte(AK8963_ADDRESS, AK8963_ST1) & 0x01)) { // wait for magnetometer data ready bit to be set
            readBytes(AK8963_ADDRESS, AK8963_XOUT_L, 7, &rawData[0]);    // Read the six raw data and ST2 registers sequentially into data array
            uint8_t c = rawData[6]; // End data read by reading ST2 register
            if(!(c & 0x08)) { // Check if magnetic sensor overflow set, if not then report data
                destination[0] = (int16_t)(((int16_t)rawData[1] << 8) | rawData[0]);    // Turn the MSB and LSB into a signed 16-bit value
                destination[1] = (int16_t)(((int16_t)rawData[3] << 8) | rawData[2]) ;    // Data stored as little Endian
                destination[2] = (int16_t)(((int16_t)rawData[5] << 8) | rawData[4]) ;
            }
        }
    }

    int16_t readTempData() {
        uint8_t rawData[2];    // x/y/z gyro register data stored here
        readBytes(MPU9250_ADDRESS, TEMP_OUT_H, 2, &rawData[0]);    // Read the two raw data registers sequentially into data array
        return (int16_t)(((int16_t)rawData[0]) << 8 | rawData[1]) ;    // Turn the MSB and LSB into a 16-bit value
    }

    void resetMPU9250() {
        // reset device
        writeByte(MPU9250_ADDRESS, PWR_MGMT_1, 0x80); // Write a one to bit 7 reset bit; toggle reset device
        wait(0.1);
    }

    void initAK8963(void) {
        float * destination = _magCalibration;
        // First extract the factory calibration for each magnetometer axis
        uint8_t rawData[3];    // x/y/z gyro calibration data stored here
        writeByte(AK8963_ADDRESS, AK8963_CNTL, 0x00); // Power down magnetometer
        wait(0.01);
        writeByte(AK8963_ADDRESS, AK8963_CNTL, 0x0F); // Enter Fuse ROM access mode
        wait(0.01);
        readBytes(AK8963_ADDRESS, AK8963_ASAX, 3, &rawData[0]);     // Read the x-, y-, and z-axis calibration values
        destination[0] = (float)(rawData[0] - 128)/256.0f + 1.0f;   // Return x-axis sensitivity adjustment values, etc.
        destination[1] = (float)(rawData[1] - 128)/256.0f + 1.0f;
        destination[2] = (float)(rawData[2] - 128)/256.0f + 1.0f;
        writeByte(AK8963_ADDRESS, AK8963_CNTL, 0x00); // Power down magnetometer
        wait(0.01);
        // Configure the magnetometer for continuous read and highest resolution
        // set Mscale bit 4 to 1 (0) to enable 16 (14) bit resolution in CNTL register,
        // and enable continuous mode data acquisition Mmode (bits [3:0]), 0010 for 8 Hz and 0110 for 100 Hz sample rates
        writeByte(AK8963_ADDRESS, AK8963_CNTL, _Mscale << 4 | _Mmode); // Set magnetometer data resolution and sample ODR
        wait(0.01);
    }

    void initMPU9250() {
        // Initialize MPU9250 device
        // wake up device
        writeByte(MPU9250_ADDRESS, PWR_MGMT_1, 0x00); // Clear sleep mode bit (6), enable all sensors
        wait(0.1); // Delay 100 ms for PLL to get established on x-axis gyro; should check for PLL ready interrupt

        // get stable time source
        writeByte(MPU9250_ADDRESS, PWR_MGMT_1, 0x01);    // Set clock source to be PLL with x-axis gyroscope reference, bits 2:0 = 001

        // Configure Gyro and Accelerometer
        // Disable FSYNC and set accelerometer and gyro bandwidth to 44 and 42 Hz, respectively;
        // DLPF_CFG = bits 2:0 = 010; this sets the sample rate at 1 kHz for both
        // Maximum delay is 4.9 ms which is just over a 200 Hz maximum rate
        writeByte(MPU9250_ADDRESS, CONFIG, 0x03);

        // Set sample rate = gyroscope output rate/(1 + SMPLRT_DIV)
        writeByte(MPU9250_ADDRESS, SMPLRT_DIV, 0x04);    // Use a 200 Hz rate; the same rate set in CONFIG above

        // Set gyroscope full scale range
        // Range selects FS_SEL and AFS_SEL are 0 - 3, so 2-bit values are left-shifted into positions 4:3
        uint8_t c = readByte(MPU9250_ADDRESS, GYRO_CONFIG);
        writeByte(MPU9250_ADDRESS, GYRO_CONFIG, c & ~0xE0); // Clear self-test bits [7:5]
        writeByte(MPU9250_ADDRESS, GYRO_CONFIG, c & ~0x18); // Clear AFS bits [4:3]
        writeByte(MPU9250_ADDRESS, GYRO_CONFIG, c | _Mscale << 3); // Set full scale range for the gyro

        // Set accelerometer configuration
        c = readByte(MPU9250_ADDRESS, ACCEL_CONFIG);
        writeByte(MPU9250_ADDRESS, ACCEL_CONFIG, c & ~0xE0); // Clear self-test bits [7:5]
        writeByte(MPU9250_ADDRESS, ACCEL_CONFIG, c & ~0x18); // Clear AFS bits [4:3]
        writeByte(MPU9250_ADDRESS, ACCEL_CONFIG, c | _Ascale << 3); // Set full scale range for the accelerometer

        // Set accelerometer sample rate configuration
        // It is possible to get a 4 kHz sample rate from the accelerometer by choosing 1 for
        // accel_fchoice_b bit [3]; in this case the bandwidth is 1.13 kHz
        c = readByte(MPU9250_ADDRESS, ACCEL_CONFIG2);
        writeByte(MPU9250_ADDRESS, ACCEL_CONFIG2, c & ~0x0F); // Clear accel_fchoice_b (bit 3) and A_DLPFG (bits [2:0])
        writeByte(MPU9250_ADDRESS, ACCEL_CONFIG2, c | 0x03); // Set accelerometer rate to 1 kHz and bandwidth to 41 Hz

        // The accelerometer, gyro, and thermometer are set to 1 kHz sample rates,
        // but all these rates are further reduced by a factor of 5 to 200 Hz because of the SMPLRT_DIV setting

        // Configure Interrupts and Bypass Enable
        // Set interrupt pin active high, push-pull, and clear on read of INT_STATUS, enable I2C_BYPASS_EN so additional chips
        // can join the I2C bus and all can be controlled by the Arduino as master
        writeByte(MPU9250_ADDRESS, INT_PIN_CFG, 0x22);
        writeByte(MPU9250_ADDRESS, INT_ENABLE, 0x01);    // Enable data ready (bit 0) interrupt
        wait(0.1); // wait for pass-through mode enabled
    }

    // Function which accumulates gyro and accelerometer data after device initialization. It calculates the average
    // of the at-rest readings and then loads the resulting offsets into accelerometer and gyro bias registers.
    void accelgyrocalMPU9250(void) {
        float * dest1 = _gyroBias;
        float * dest2 = _accelBias;

        uint8_t data[12]; // data array to hold accelerometer and gyro x, y, z, data
        uint16_t ii, packet_count, fifo_count;
        int32_t gyro_bias[3] = {0, 0, 0}, accel_bias[3] = {0, 0, 0};

        // reset device, reset all registers, clear gyro and accelerometer bias registers
        writeByte(MPU9250_ADDRESS, PWR_MGMT_1, 0x80); // Write a one to bit 7 reset bit; toggle reset device
        wait(0.1);

        // get stable time source
        // Set clock source to be PLL with x-axis gyroscope reference, bits 2:0 = 001
        writeByte(MPU9250_ADDRESS, PWR_MGMT_1, 0x01);
        writeByte(MPU9250_ADDRESS, PWR_MGMT_2, 0x00);
        wait(0.2);

        // Configure device for bias calculation
        writeByte(MPU9250_ADDRESS, INT_ENABLE, 0x00);     // Disable all interrupts
        writeByte(MPU9250_ADDRESS, FIFO_EN, 0x00);            // Disable FIFO
        writeByte(MPU9250_ADDRESS, PWR_MGMT_1, 0x00);     // Turn on internal clock source
        writeByte(MPU9250_ADDRESS, I2C_MST_CTRL, 0x00); // Disable I2C master
        writeByte(MPU9250_ADDRESS, USER_CTRL, 0x00);        // Disable FIFO and I2C master modes
        writeByte(MPU9250_ADDRESS, USER_CTRL, 0x0C);        // Reset FIFO and DMP
        wait(0.015);

        // Configure MPU9250 gyro and accelerometer for bias calculation
        writeByte(MPU9250_ADDRESS, CONFIG, 0x01);            // Set low-pass filter to 188 Hz
        writeByte(MPU9250_ADDRESS, SMPLRT_DIV, 0x00);    // Set sample rate to 1 kHz
        writeByte(MPU9250_ADDRESS, GYRO_CONFIG, 0x00);    // Set gyro full-scale to 250 degrees per second, maximum sensitivity
        writeByte(MPU9250_ADDRESS, ACCEL_CONFIG, 0x00); // Set accelerometer full-scale to 2 g, maximum sensitivity

        uint16_t gyrosensitivity  = 131;      // = 131 LSB/degrees/sec
        uint16_t accelsensitivity = 16384;    // = 16384 LSB/g

        // Configure FIFO to capture accelerometer and gyro data for bias calculation
        writeByte(MPU9250_ADDRESS, USER_CTRL, 0x40);     // Enable FIFO
        writeByte(MPU9250_ADDRESS, FIFO_EN, 0x78);         // Enable gyro and accelerometer sensors for FIFO (max size 512 bytes in MPU-9250)
        wait(0.04); // accumulate 40 samples in 80 milliseconds = 480 bytes

        // At end of sample accumulation, turn off FIFO sensor read
        writeByte(MPU9250_ADDRESS, FIFO_EN, 0x00);                // Disable gyro and accelerometer sensors for FIFO
        readBytes(MPU9250_ADDRESS, FIFO_COUNTH, 2, &data[0]); // read FIFO sample count
        fifo_count = ((uint16_t)data[0] << 8) | data[1];
        packet_count = fifo_count/12;// How many sets of full gyro and accelerometer data for averaging

        for (ii = 0; ii < packet_count; ii++) {
            int16_t accel_temp[3] = {0, 0, 0}, gyro_temp[3] = {0, 0, 0};
            readBytes(MPU9250_ADDRESS, FIFO_R_W, 12, &data[0]); // read data for averaging
            accel_temp[0] = (int16_t) (((int16_t)data[0] << 8) | data[1]    ) ;    // Form signed 16-bit integer for each sample in FIFO
            accel_temp[1] = (int16_t) (((int16_t)data[2] << 8) | data[3]    ) ;
            accel_temp[2] = (int16_t) (((int16_t)data[4] << 8) | data[5]    ) ;
            gyro_temp[0]  = (int16_t) (((int16_t)data[6] << 8) | data[7]    ) ;
            gyro_temp[1]  = (int16_t) (((int16_t)data[8] << 8) | data[9]    ) ;
            gyro_temp[2]  = (int16_t) (((int16_t)data[10] << 8) | data[11]) ;

            accel_bias[0] += (int32_t) accel_temp[0]; // Sum individual signed 16-bit biases to get accumulated signed 32-bit biases
            accel_bias[1] += (int32_t) accel_temp[1];
            accel_bias[2] += (int32_t) accel_temp[2];
            gyro_bias[0]  += (int32_t) gyro_temp[0];
            gyro_bias[1]  += (int32_t) gyro_temp[1];
            gyro_bias[2]  += (int32_t) gyro_temp[2];
        }

        accel_bias[0] /= (int32_t) packet_count; // Normalize sums to get average count biases
        accel_bias[1] /= (int32_t) packet_count;
        accel_bias[2] /= (int32_t) packet_count;
        gyro_bias[0]  /= (int32_t) packet_count;
        gyro_bias[1]  /= (int32_t) packet_count;
        gyro_bias[2]  /= (int32_t) packet_count;

        if(accel_bias[2] > 0L) {accel_bias[2] -= (int32_t) accelsensitivity;}    // Remove gravity from the z-axis accelerometer bias calculation
        else {accel_bias[2] += (int32_t) accelsensitivity;}

        // Construct the gyro biases for push to the hardware gyro bias registers, which are reset to zero upon device startup
        data[0] = (-gyro_bias[0]/4    >> 8) & 0xFF; // Divide by 4 to get 32.9 LSB per deg/s to conform to expected bias input format
        data[1] = (-gyro_bias[0]/4)         & 0xFF; // Biases are additive, so change sign on calculated average gyro biases
        data[2] = (-gyro_bias[1]/4    >> 8) & 0xFF;
        data[3] = (-gyro_bias[1]/4)         & 0xFF;
        data[4] = (-gyro_bias[2]/4    >> 8) & 0xFF;
        data[5] = (-gyro_bias[2]/4)         & 0xFF;

        /// Push gyro biases to hardware registers
        /*
            writeByte(MPU9250_ADDRESS, XG_OFFSET_H, data[0]);
            writeByte(MPU9250_ADDRESS, XG_OFFSET_L, data[1]);
            writeByte(MPU9250_ADDRESS, YG_OFFSET_H, data[2]);
            writeByte(MPU9250_ADDRESS, YG_OFFSET_L, data[3]);
            writeByte(MPU9250_ADDRESS, ZG_OFFSET_H, data[4]);
            writeByte(MPU9250_ADDRESS, ZG_OFFSET_L, data[5]);
        */
        dest1[0] = (float) gyro_bias[0]/(float) gyrosensitivity; // construct gyro bias in deg/s for later manual subtraction
        dest1[1] = (float) gyro_bias[1]/(float) gyrosensitivity;
        dest1[2] = (float) gyro_bias[2]/(float) gyrosensitivity;

        printf("GyroBias X: %f\r\n", dest1[0]);
        printf("GyroBias Y: %f\r\n", dest1[1]);
        printf("GyroBias Z: %f\r\n", dest1[2]);

        // Construct the accelerometer biases for push to the hardware accelerometer bias registers. These registers contain
        // factory trim values which must be added to the calculated accelerometer biases; on boot up these registers will hold
        // non-zero values. In addition, bit 0 of the lower byte must be preserved since it is used for temperature
        // compensation calculations. Accelerometer bias registers expect bias input as 2048 LSB per g, so that
        // the accelerometer biases calculated above must be divided by 8.

        int32_t accel_bias_reg[3] = {0, 0, 0}; // A place to hold the factory accelerometer trim biases
        readBytes(MPU9250_ADDRESS, XA_OFFSET_H, 2, &data[0]); // Read factory accelerometer trim values
        accel_bias_reg[0] = (int16_t) ((int16_t)data[0] << 8) | data[1];
        readBytes(MPU9250_ADDRESS, YA_OFFSET_H, 2, &data[0]);
        accel_bias_reg[1] = (int16_t) ((int16_t)data[0] << 8) | data[1];
        readBytes(MPU9250_ADDRESS, ZA_OFFSET_H, 2, &data[0]);
        accel_bias_reg[2] = (int16_t) ((int16_t)data[0] << 8) | data[1];

        uint32_t mask = 1uL; // Define mask for temperature compensation bit 0 of lower byte of accelerometer bias registers
        uint8_t mask_bit[3] = {0, 0, 0}; // Define array to hold mask bit for each accelerometer bias axis

        for(ii = 0; ii < 3; ii++) {
            if(accel_bias_reg[ii] & mask) mask_bit[ii] = 0x01; // If temperature compensation bit is set, record that fact in mask_bit
        }

        // Construct total accelerometer bias, including calculated average accelerometer bias from above
        accel_bias_reg[0] -= (accel_bias[0]/8); // Subtract calculated averaged accelerometer bias scaled to 2048 LSB/g (16 g full scale)
        accel_bias_reg[1] -= (accel_bias[1]/8);
        accel_bias_reg[2] -= (accel_bias[2]/8);

        data[0] = (accel_bias_reg[0] >> 8) & 0xFF;
        data[1] = (accel_bias_reg[0])      & 0xFF;
        data[1] = data[1] | mask_bit[0]; // preserve temperature compensation bit when writing back to accelerometer bias registers
        data[2] = (accel_bias_reg[1] >> 8) & 0xFF;
        data[3] = (accel_bias_reg[1])      & 0xFF;
        data[3] = data[3] | mask_bit[1]; // preserve temperature compensation bit when writing back to accelerometer bias registers
        data[4] = (accel_bias_reg[2] >> 8) & 0xFF;
        data[5] = (accel_bias_reg[2])      & 0xFF;
        data[5] = data[5] | mask_bit[2]; // preserve temperature compensation bit when writing back to accelerometer bias registers

        // Apparently this is not working for the acceleration biases in the MPU-9250
        // Are we handling the temperature correction bit properly?
        // Push accelerometer biases to hardware registers
        /*    writeByte(MPU9250_ADDRESS, XA_OFFSET_H, data[0]);
            writeByte(MPU9250_ADDRESS, XA_OFFSET_L, data[1]);
            writeByte(MPU9250_ADDRESS, YA_OFFSET_H, data[2]);
            writeByte(MPU9250_ADDRESS, YA_OFFSET_L, data[3]);
            writeByte(MPU9250_ADDRESS, ZA_OFFSET_H, data[4]);
            writeByte(MPU9250_ADDRESS, ZA_OFFSET_L, data[5]);
        */
        // Output scaled accelerometer biases for manual subtraction in the main program
        dest2[0] = (float)accel_bias[0]/(float)accelsensitivity;
        dest2[1] = (float)accel_bias[1]/(float)accelsensitivity;
        dest2[2] = (float)accel_bias[2]/(float)accelsensitivity;

        printf("AccelBias X: %f\r\n", dest1[0]);
        printf("AccelBias Y: %f\r\n", dest1[1]);
        printf("AccelBias Z: %f\r\n", dest1[2]);
    }


    // Accelerometer and gyroscope self test; check calibration wrt factory settings
    void MPU9250SelfTest(float * destination) // Should return percent deviation from factory trim values, +/- 14 or less deviation is a pass
    {
        uint8_t rawData[6] = {0, 0, 0, 0, 0, 0};
        uint8_t selfTest[6];
        int16_t gAvg[3], aAvg[3], aSTAvg[3], gSTAvg[3];
        float factoryTrim[6];
        uint8_t FS = 0;

        writeByte(MPU9250_ADDRESS, SMPLRT_DIV, 0x00); // Set gyro sample rate to 1 kHz
        writeByte(MPU9250_ADDRESS, CONFIG, 0x02); // Set gyro sample rate to 1 kHz and DLPF to 92 Hz
        writeByte(MPU9250_ADDRESS, GYRO_CONFIG, 1<<FS); // Set full scale range for the gyro to 250 dps
        writeByte(MPU9250_ADDRESS, ACCEL_CONFIG2, 0x02); // Set accelerometer rate to 1 kHz and bandwidth to 92 Hz
        writeByte(MPU9250_ADDRESS, ACCEL_CONFIG, 1<<FS); // Set full scale range for the accelerometer to 2 g

        for( int ii = 0; ii < 200; ii++) { // get average current values of gyro and acclerometer

            readBytes(MPU9250_ADDRESS, ACCEL_XOUT_H, 6, &rawData[0]); // Read the six raw data registers into data array
            aAvg[0] += (int16_t)(((int16_t)rawData[0] << 8) | rawData[1]) ; // Turn the MSB and LSB into a signed 16-bit value
            aAvg[1] += (int16_t)(((int16_t)rawData[2] << 8) | rawData[3]) ;
            aAvg[2] += (int16_t)(((int16_t)rawData[4] << 8) | rawData[5]) ;

            readBytes(MPU9250_ADDRESS, GYRO_XOUT_H, 6, &rawData[0]); // Read the six raw data registers sequentially into data array
            gAvg[0] += (int16_t)(((int16_t)rawData[0] << 8) | rawData[1]) ; // Turn the MSB and LSB into a signed 16-bit value
            gAvg[1] += (int16_t)(((int16_t)rawData[2] << 8) | rawData[3]) ;
            gAvg[2] += (int16_t)(((int16_t)rawData[4] << 8) | rawData[5]) ;
        }

        for (int ii =0; ii < 3; ii++) { // Get average of 200 values and store as average current readings
            aAvg[ii] /= 200;
            gAvg[ii] /= 200;
        }

        // Configure the accelerometer for self-test
        writeByte(MPU9250_ADDRESS, ACCEL_CONFIG, 0xE0); // Enable self test on all three axes and set accelerometer range to +/- 2 g
        writeByte(MPU9250_ADDRESS, GYRO_CONFIG, 0xE0); // Enable self test on all three axes and set gyro range to +/- 250 degrees/s
        wait_ms(25); // Delay a while to let the device stabilize

        for( int ii = 0; ii < 200; ii++) { // get average self-test values of gyro and acclerometer

            readBytes(MPU9250_ADDRESS, ACCEL_XOUT_H, 6, &rawData[0]); // Read the six raw data registers into data array
            aSTAvg[0] += (int16_t)(((int16_t)rawData[0] << 8) | rawData[1]) ; // Turn the MSB and LSB into a signed 16-bit value
            aSTAvg[1] += (int16_t)(((int16_t)rawData[2] << 8) | rawData[3]) ;
            aSTAvg[2] += (int16_t)(((int16_t)rawData[4] << 8) | rawData[5]) ;

            readBytes(MPU9250_ADDRESS, GYRO_XOUT_H, 6, &rawData[0]); // Read the six raw data registers sequentially into data array
            gSTAvg[0] += (int16_t)(((int16_t)rawData[0] << 8) | rawData[1]) ; // Turn the MSB and LSB into a signed 16-bit value
            gSTAvg[1] += (int16_t)(((int16_t)rawData[2] << 8) | rawData[3]) ;
            gSTAvg[2] += (int16_t)(((int16_t)rawData[4] << 8) | rawData[5]) ;
        }

        for (int ii =0; ii < 3; ii++) { // Get average of 200 values and store as average self-test readings
            aSTAvg[ii] /= 200;
            gSTAvg[ii] /= 200;
        }

        // Configure the gyro and accelerometer for normal operation
        writeByte(MPU9250_ADDRESS, ACCEL_CONFIG, 0x00);
        writeByte(MPU9250_ADDRESS, GYRO_CONFIG, 0x00);
        wait_ms(25); // Delay a while to let the device stabilize

        // Retrieve accelerometer and gyro factory Self-Test Code from USR_Reg
        selfTest[0] = readByte(MPU9250_ADDRESS, SELF_TEST_X_ACCEL); // X-axis accel self-test results
        selfTest[1] = readByte(MPU9250_ADDRESS, SELF_TEST_Y_ACCEL); // Y-axis accel self-test results
        selfTest[2] = readByte(MPU9250_ADDRESS, SELF_TEST_Z_ACCEL); // Z-axis accel self-test results
        selfTest[3] = readByte(MPU9250_ADDRESS, SELF_TEST_X_GYRO); // X-axis gyro self-test results
        selfTest[4] = readByte(MPU9250_ADDRESS, SELF_TEST_Y_GYRO); // Y-axis gyro self-test results
        selfTest[5] = readByte(MPU9250_ADDRESS, SELF_TEST_Z_GYRO); // Z-axis gyro self-test results

        // Retrieve factory self-test value from self-test code reads
        factoryTrim[0] = (float)(2620/1<<FS)*(pow( 1.01 , ((float)selfTest[0] - 1.0) )); // FT[Xa] factory trim calculation
        factoryTrim[1] = (float)(2620/1<<FS)*(pow( 1.01 , ((float)selfTest[1] - 1.0) )); // FT[Ya] factory trim calculation
        factoryTrim[2] = (float)(2620/1<<FS)*(pow( 1.01 , ((float)selfTest[2] - 1.0) )); // FT[Za] factory trim calculation
        factoryTrim[3] = (float)(2620/1<<FS)*(pow( 1.01 , ((float)selfTest[3] - 1.0) )); // FT[Xg] factory trim calculation
        factoryTrim[4] = (float)(2620/1<<FS)*(pow( 1.01 , ((float)selfTest[4] - 1.0) )); // FT[Yg] factory trim calculation
        factoryTrim[5] = (float)(2620/1<<FS)*(pow( 1.01 , ((float)selfTest[5] - 1.0) )); // FT[Zg] factory trim calculation

        // Report results as a ratio of (STR - FT)/FT; the change from Factory Trim of the Self-Test Response
        // To get percent, must multiply by 100
        for (int i = 0; i < 3; i++) {
         destination[i] = 100.0*((float)(aSTAvg[i] - aAvg[i]))/factoryTrim[i]; // Report percent differences
         destination[i+3] = 100.0*((float)(gSTAvg[i] - gAvg[i]))/factoryTrim[i+3]; // Report percent differences
        }

    }


    /* uint8_t out[4 * 4], Quaternion in NED(w,x,y,z) */
    void performMadgwickQuaternionUpdate(uint8_t *out) {
        uint32_t now = _timer.read_us();
        _deltat = ((now - _lastUpdate) / 1000000.0f); // set integration time by time elapsed since last filter update
        _lastUpdate = now;
        float* out_data = (float *) out;
        // Sensors x (y)-axis of the accelerometer/gyro is aligned with the y (x)axis of the magnetometer;
        // the magnetometer z-axis (+ down) is misaligned with z-axis (+ up) of accelerometer and gyro!
        // We have to make some allowance for this orientation mismatch in feeding the output to the quaternion filter.
        // We will assume that +y accel/gyro is North, then x accel/gyro is East. So if we want te quaternions properly aligned
        // we need to feed into the madgwick function Ay, Ax, -Az, Gy, Gx, -Gz, Mx, My, and Mz. But because gravity is by convention
        // positive down, we need to invert the accel data, so we pass -Ay, -Ax, Az, Gy, Gx, -Gz, Mx, My, and Mz into the Madgwick
        // function to get North along the accel +y-axis, East along the accel +x-axis, and Down along the accel -z-axis.
        // This orientation choice can be modified to allow any convenient (non-NED) orientation convention.
        // This is ok by aircraft orientation standards!
        // Pass gyro rate as rad/s
        // MadgwickQuaternionUpdate(-ay, -ax, az, gy*PI/180.0f, gx*PI/180.0f, -gz*PI/180.0f,  mx,  my, mz);
        // if(passThru)MahonyQuaternionUpdate(-ay, -ax, az, gy*PI/180.0f, gx*PI/180.0f, -gz*PI/180.0f,  mx,  my, mz);
        // NED:
        MadgwickQuaternionUpdate(-_a[1], -_a[0], _a[2], _g[1], _g[0], -_g[2], _m[0], _m[1], _m[2]);
        out_data[0] = _q[0];  // NED +W
        out_data[1] = _q[1];  // NED +X
        out_data[2] = _q[2];  // NED +Y
        out_data[3] = _q[3];  // NED +Z
    }

    // Implementation of Sebastian Madgwick's "...efficient orientation filter for... inertial/magnetic sensor arrays"
    // (see http://www.x-io.co.uk/category/open-source/ for examples and more details)
    // which fuses acceleration, rotation rate, and magnetic moments to produce a quaternion-based estimate of absolute
    // device orientation -- which can be converted to yaw, pitch, and roll. Useful for stabilizing quadcopters, etc.
    // The performance of the orientation filter is at least as good as conventional Kalman-based filtering algorithms
    // but is much less computationally intensive---it can be performed on a 3.3 V Pro Mini operating at 8 MHz!
    void MadgwickQuaternionUpdate(float ax, float ay, float az, float gx, float gy, float gz, float mx, float my, float mz)
    {
        float q1 = _q[0], q2 = _q[1], q3 = _q[2], q4 = _q[3];     // short name local variable for readability
        float norm;
        float hx, hy, _2bx, _2bz;
        float s1, s2, s3, s4;
        float qDot1, qDot2, qDot3, qDot4;

        // Auxiliary variables to avoid repeated arithmetic
        float _2q1mx;
        float _2q1my;
        float _2q1mz;
        float _2q2mx;
        float _4bx;
        float _4bz;
        float _2q1 = 2.0f * q1;
        float _2q2 = 2.0f * q2;
        float _2q3 = 2.0f * q3;
        float _2q4 = 2.0f * q4;
        float _2q1q3 = 2.0f * q1 * q3;
        float _2q3q4 = 2.0f * q3 * q4;
        float q1q1 = q1 * q1;
        float q1q2 = q1 * q2;
        float q1q3 = q1 * q3;
        float q1q4 = q1 * q4;
        float q2q2 = q2 * q2;
        float q2q3 = q2 * q3;
        float q2q4 = q2 * q4;
        float q3q3 = q3 * q3;
        float q3q4 = q3 * q4;
        float q4q4 = q4 * q4;

        // Normalise accelerometer measurement
        norm = sqrt(ax * ax + ay * ay + az * az);
        if (norm == 0.0f) return; // handle NaN
        norm = 1.0f/norm;
        ax *= norm;
        ay *= norm;
        az *= norm;

        // Normalise magnetometer measurement
        norm = sqrt(mx * mx + my * my + mz * mz);
        if (norm == 0.0f) return; // handle NaN
        norm = 1.0f/norm;
        mx *= norm;
        my *= norm;
        mz *= norm;

        // Reference direction of Earth's magnetic field
        _2q1mx = 2.0f * q1 * mx;
        _2q1my = 2.0f * q1 * my;
        _2q1mz = 2.0f * q1 * mz;
        _2q2mx = 2.0f * q2 * mx;
        hx = mx * q1q1 - _2q1my * q4 + _2q1mz * q3 + mx * q2q2 + _2q2 * my * q3 + _2q2 * mz * q4 - mx * q3q3 - mx * q4q4;
        hy = _2q1mx * q4 + my * q1q1 - _2q1mz * q2 + _2q2mx * q3 - my * q2q2 + my * q3q3 + _2q3 * mz * q4 - my * q4q4;
        _2bx = sqrt(hx * hx + hy * hy);
        _2bz = -_2q1mx * q3 + _2q1my * q2 + mz * q1q1 + _2q2mx * q4 - mz * q2q2 + _2q3 * my * q4 - mz * q3q3 + mz * q4q4;
        _4bx = 2.0f * _2bx;
        _4bz = 2.0f * _2bz;

        // Gradient decent algorithm corrective step
        s1 = -_2q3 * (2.0f * q2q4 - _2q1q3 - ax) + _2q2 * (2.0f * q1q2 + _2q3q4 - ay) - _2bz * q3 * (_2bx * (0.5f - q3q3 - q4q4) + _2bz * (q2q4 - q1q3) - mx) + (-_2bx * q4 + _2bz * q2) * (_2bx * (q2q3 - q1q4) + _2bz * (q1q2 + q3q4) - my) + _2bx * q3 * (_2bx * (q1q3 + q2q4) + _2bz * (0.5f - q2q2 - q3q3) - mz);
        s2 = _2q4 * (2.0f * q2q4 - _2q1q3 - ax) + _2q1 * (2.0f * q1q2 + _2q3q4 - ay) - 4.0f * q2 * (1.0f - 2.0f * q2q2 - 2.0f * q3q3 - az) + _2bz * q4 * (_2bx * (0.5f - q3q3 - q4q4) + _2bz * (q2q4 - q1q3) - mx) + (_2bx * q3 + _2bz * q1) * (_2bx * (q2q3 - q1q4) + _2bz * (q1q2 + q3q4) - my) + (_2bx * q4 - _4bz * q2) * (_2bx * (q1q3 + q2q4) + _2bz * (0.5f - q2q2 - q3q3) - mz);
        s3 = -_2q1 * (2.0f * q2q4 - _2q1q3 - ax) + _2q4 * (2.0f * q1q2 + _2q3q4 - ay) - 4.0f * q3 * (1.0f - 2.0f * q2q2 - 2.0f * q3q3 - az) + (-_4bx * q3 - _2bz * q1) * (_2bx * (0.5f - q3q3 - q4q4) + _2bz * (q2q4 - q1q3) - mx) + (_2bx * q2 + _2bz * q4) * (_2bx * (q2q3 - q1q4) + _2bz * (q1q2 + q3q4) - my) + (_2bx * q1 - _4bz * q3) * (_2bx * (q1q3 + q2q4) + _2bz * (0.5f - q2q2 - q3q3) - mz);
        s4 = _2q2 * (2.0f * q2q4 - _2q1q3 - ax) + _2q3 * (2.0f * q1q2 + _2q3q4 - ay) + (-_4bx * q4 + _2bz * q2) * (_2bx * (0.5f - q3q3 - q4q4) + _2bz * (q2q4 - q1q3) - mx) + (-_2bx * q1 + _2bz * q3) * (_2bx * (q2q3 - q1q4) + _2bz * (q1q2 + q3q4) - my) + _2bx * q2 * (_2bx * (q1q3 + q2q4) + _2bz * (0.5f - q2q2 - q3q3) - mz);
        norm = sqrt(s1 * s1 + s2 * s2 + s3 * s3 + s4 * s4);        // normalise step magnitude
        norm = 1.0f/norm;
        s1 *= norm;
        s2 *= norm;
        s3 *= norm;
        s4 *= norm;

        // Compute rate of change of quaternion
        qDot1 = 0.5f * (-q2 * gx - q3 * gy - q4 * gz) - BETA * s1;
        qDot2 = 0.5f * (q1 * gx + q3 * gz - q4 * gy) - BETA * s2;
        qDot3 = 0.5f * (q1 * gy - q2 * gz + q4 * gx) - BETA * s3;
        qDot4 = 0.5f * (q1 * gz + q2 * gy - q3 * gx) - BETA * s4;

        // Integrate to yield quaternion
        q1 += qDot1 * _deltat;
        q2 += qDot2 * _deltat;
        q3 += qDot3 * _deltat;
        q4 += qDot4 * _deltat;
        norm = sqrt(q1 * q1 + q2 * q2 + q3 * q3 + q4 * q4);        // normalise quaternion
        norm = 1.0f/norm;
        _q[0] = q1 * norm;
        _q[1] = q2 * norm;
        _q[2] = q3 * norm;
        _q[3] = q4 * norm;

    }

    // Similar to Madgwick scheme but uses proportional and integral filtering on the error between estimated reference vectors and
    // measured ones.
    void MahonyQuaternionUpdate(float ax, float ay, float az, float gx, float gy, float gz, float mx, float my, float mz)
    {
        float q1 = _q[0], q2 = _q[1], q3 = _q[2], q4 = _q[3];     // short name local variable for readability
        float norm;
        float hx, hy, bx, bz;
        float vx, vy, vz, wx, wy, wz;
        float ex, ey, ez;
        float pa, pb, pc;

        // Auxiliary variables to avoid repeated arithmetic
        float q1q1 = q1 * q1;
        float q1q2 = q1 * q2;
        float q1q3 = q1 * q3;
        float q1q4 = q1 * q4;
        float q2q2 = q2 * q2;
        float q2q3 = q2 * q3;
        float q2q4 = q2 * q4;
        float q3q3 = q3 * q3;
        float q3q4 = q3 * q4;
        float q4q4 = q4 * q4;

        // Normalise accelerometer measurement
        norm = sqrt(ax * ax + ay * ay + az * az);
        if (norm == 0.0f) return; // handle NaN
        norm = 1.0f / norm;                // use reciprocal for division
        ax *= norm;
        ay *= norm;
        az *= norm;

        // Normalise magnetometer measurement
        norm = sqrt(mx * mx + my * my + mz * mz);
        if (norm == 0.0f) return; // handle NaN
        norm = 1.0f / norm;                // use reciprocal for division
        mx *= norm;
        my *= norm;
        mz *= norm;

        // Reference direction of Earth's magnetic field
        hx = 2.0f * mx * (0.5f - q3q3 - q4q4) + 2.0f * my * (q2q3 - q1q4) + 2.0f * mz * (q2q4 + q1q3);
        hy = 2.0f * mx * (q2q3 + q1q4) + 2.0f * my * (0.5f - q2q2 - q4q4) + 2.0f * mz * (q3q4 - q1q2);
        bx = sqrt((hx * hx) + (hy * hy));
        bz = 2.0f * mx * (q2q4 - q1q3) + 2.0f * my * (q3q4 + q1q2) + 2.0f * mz * (0.5f - q2q2 - q3q3);

        // Estimated direction of gravity and magnetic field
        vx = 2.0f * (q2q4 - q1q3);
        vy = 2.0f * (q1q2 + q3q4);
        vz = q1q1 - q2q2 - q3q3 + q4q4;
        wx = 2.0f * bx * (0.5f - q3q3 - q4q4) + 2.0f * bz * (q2q4 - q1q3);
        wy = 2.0f * bx * (q2q3 - q1q4) + 2.0f * bz * (q1q2 + q3q4);
        wz = 2.0f * bx * (q1q3 + q2q4) + 2.0f * bz * (0.5f - q2q2 - q3q3);

        // Error is cross product between estimated direction and measured direction of gravity
        ex = (ay * vz - az * vy) + (my * wz - mz * wy);
        ey = (az * vx - ax * vz) + (mz * wx - mx * wz);
        ez = (ax * vy - ay * vx) + (mx * wy - my * wx);
        if (Ki > 0.0f)
        {
            _eInt[0] += ex;            // accumulate integral error
            _eInt[1] += ey;
            _eInt[2] += ez;
        }
        else
        {
            _eInt[0] = 0.0f;         // prevent integral wind up
            _eInt[1] = 0.0f;
            _eInt[2] = 0.0f;
        }

        // Apply feedback terms
        gx = gx + Kp * ex + Ki * _eInt[0];
        gy = gy + Kp * ey + Ki * _eInt[1];
        gz = gz + Kp * ez + Ki * _eInt[2];

        // Integrate rate of change of quaternion
        pa = q2;
        pb = q3;
        pc = q4;
        q1 = q1 + (-q2 * gx - q3 * gy - q4 * gz) * (0.5f * _deltat);
        q2 = pa + (q1 * gx + pb * gz - pc * gy) * (0.5f * _deltat);
        q3 = pb + (q1 * gy - pa * gz + pc * gx) * (0.5f * _deltat);
        q4 = pc + (q1 * gz + pa * gy - pb * gx) * (0.5f * _deltat);

        // Normalise quaternion
        norm = sqrt(q1 * q1 + q2 * q2 + q3 * q3 + q4 * q4);
        norm = 1.0f / norm;
        _q[0] = q1 * norm;
        _q[1] = q2 * norm;
        _q[2] = q3 * norm;
        _q[3] = q4 * norm;

    }

    // https://github.com/kriswiner/MPU-6050/wiki/Simple-and-Effective-Magnetometer-Calibration
    // https://github.com/1994james/9250-code/blob/master/_9250_code.ino#L746
    void magcalMPU9250(void)
    {
        float *dest1 = _magBias;
        float *dest2 = _magScale;

        uint16_t ii = 0, sample_count = 0;
        int32_t mag_bias[3] = {0, 0, 0}, mag_scale[3] = {0, 0, 0};
        int16_t mag_max[3] = { (int16_t) 0x8000, (int16_t) 0x8000, (int16_t) 0x8000};
        int16_t mag_min[3] = { (int16_t) 0x7FFF, (int16_t) 0x7FFF, (int16_t) 0x7FFF};
        int16_t mag_temp[3] = {0, 0, 0};

        sample_count = 128;
        uint8_t wait_millis;
        if(_Mmode == 0x02) wait_millis=135;  // at 8 Hz ODR, new mag data is available every 125 ms
        if(_Mmode == 0x06) wait_millis=12;   // at 100 Hz ODR, new mag data is available every 10 ms
        for(ii = 0; ii < sample_count; ii++) {
            readMagData(mag_temp);  // Read the mag data
            for (int jj = 0; jj < 3; jj++) {
                if(mag_temp[jj] > mag_max[jj]) mag_max[jj] = mag_temp[jj];
                if(mag_temp[jj] < mag_min[jj]) mag_min[jj] = mag_temp[jj];
            }
            wait_ms(wait_millis);
        }

        // Get hard iron correction
        mag_bias[0]  = (mag_max[0] + mag_min[0])/2;  // get average x mag bias in counts
        mag_bias[1]  = (mag_max[1] + mag_min[1])/2;  // get average y mag bias in counts
        mag_bias[2]  = (mag_max[2] + mag_min[2])/2;  // get average z mag bias in counts

        dest1[0] = (float) mag_bias[0]*_mRes*_magCalibration[0];  // save mag biases in G for main program
        dest1[1] = (float) mag_bias[1]*_mRes*_magCalibration[1];
        dest1[2] = (float) mag_bias[2]*_mRes*_magCalibration[2];

        printf("MagBias X: %f\r\n", dest1[0]);
        printf("MagBias Y: %f\r\n", dest1[1]);
        printf("MagBias Z: %f\r\n", dest1[2]);

        if (!dest2) {
            return;
        }

        // Get soft iron correction estimate
        mag_scale[0]  = (mag_max[0] - mag_min[0])/2;  // get average x axis max chord length in counts
        mag_scale[1]  = (mag_max[1] - mag_min[1])/2;  // get average y axis max chord length in counts
        mag_scale[2]  = (mag_max[2] - mag_min[2])/2;  // get average z axis max chord length in counts

        float avg_rad = mag_scale[0] + mag_scale[1] + mag_scale[2];
        avg_rad /= 3.0;

        dest2[0] = avg_rad/((float)mag_scale[0]);
        dest2[1] = avg_rad/((float)mag_scale[1]);
        dest2[2] = avg_rad/((float)mag_scale[2]);

        printf("MagScale X: %f\r\n", dest2[0]);
        printf("MagScale Y: %f\r\n", dest2[1]);
        printf("MagScale Z: %f\r\n", dest2[2]);
    }
};
#endif

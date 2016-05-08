#include "mpu9250.h"
#include <Arduino.h>

//extern void delay(uint32_t dwMs);

MPU9250::MPU9250()
{
    mI2C = new I2C(MPU9250_ADDRESS);
}


bool MPU9250::available(void)
{
    return (mI2C->read(MPU9250_WHO_AM_I) == 0x71);
}

void MPU9250::gyro_data(int16_t* dest)
{
    this->read_data(MPU9250_GYRO_XOUT_H, dest);
}

void MPU9250::accel_data(int16_t* dest)
{
    this->read_data(MPU9250_ACCEL_XOUT_H, dest);
}

void MPU9250::read_data(uint8_t address, int16_t* dest)
{
    uint8_t raw[6] = {0};
    mI2C->read(address, 6, &raw[0]);        // Read the six raw data registers into data array
    dest[0] = (int16_t)(((int16_t)raw[0] << 8) | raw[1]) ;  // Turn the MSB and LSB into a signed 16-bit value
    dest[1] = (int16_t)(((int16_t)raw[2] << 8) | raw[3]) ;  
    dest[2] = (int16_t)(((int16_t)raw[4] << 8) | raw[5]) ; 
}

void MPU9250::read_data_sum(uint8_t address, int16_t* dest)
{
    uint8_t raw[6] = {0};
    mI2C->read(address, 6, &raw[0]);        // Read the six raw data registers into data array
    dest[0] += (int16_t)(((int16_t)raw[0] << 8) | raw[1]) ;  // Turn the MSB and LSB into a signed 16-bit value
    dest[1] += (int16_t)(((int16_t)raw[2] << 8) | raw[3]) ;  
    dest[2] += (int16_t)(((int16_t)raw[4] << 8) | raw[5]) ; 
}


void MPU9250::init(void)
{
    // FIMXE: implement
}

void MPU9250::calibrate(void)
{
    // FIMXE: implement
}
        

void MPU9250::self_test(float* dest)
{
    uint8_t self_test[6];
    int16_t gyro_avg[3], accel_avg[3], accel_STAvg[3], gyro_STAvg[3];
    float factory_trim[6];
    uint8_t FS = 0;
    
    mI2C->write(MPU9250_SMPLRT_DIV,     0x00);  // Set gyro sample rate to 1 kHz
    mI2C->write(MPU9250_CONFIG,         0x02);  // Set gyro sample rate to 1 kHz and DLPF to 92 Hz
    mI2C->write(MPU9250_GYRO_CONFIG,    1<<FS); // Set full scale range for the gyro to 250 dps
    mI2C->write(MPU9250_ACCEL_CONFIG2,  0x02);  // Set accelerometer rate to 1 kHz and bandwidth to 92 Hz
    mI2C->write(MPU9250_ACCEL_CONFIG,   1<<FS); // Set full scale range for the accelerometer to 2 g

    for( int i = 0; i < mNrOfSelfTestSamples; i++) {
        // Read and sum gyro and accelerometer values in arrays for calculating
        // average afterwards
        this->read_data_sum(MPU9250_ACCEL_XOUT_H, accel_avg);
        this->read_data_sum(MPU9250_GYRO_XOUT_H, gyro_avg);
    }

    // calculate average
    for (int i =0; i < 3; i++) {
        accel_avg[i] /= mNrOfSelfTestSamples;
        gyro_avg[i] /= mNrOfSelfTestSamples;
    }

    // Configure the accelerometer for self-test
    mI2C->write(MPU9250_ACCEL_CONFIG, 0xE0); // Enable self test on all three axes and set accelerometer range to +/- 2 g
    mI2C->write(MPU9250_GYRO_CONFIG,  0xE0); // Enable self test on all three axes and set gyro range to +/- 250 degrees/s
    delay(25);  // Delay a while to let the device stabilize
    
    for( int i = 0; i < mNrOfSelfTestSamples; i++) {  // get average self-test values of gyro and accelerometer
        // Read the six raw data registers into data array
        this->read_data_sum(MPU9250_ACCEL_XOUT_H, accel_STAvg);
        this->read_data_sum(MPU9250_GYRO_XOUT_H, gyro_STAvg);
    }

    for (int i =0; i < 3; i++) {
        accel_STAvg[i] /= mNrOfSelfTestSamples;
        gyro_STAvg[i] /= mNrOfSelfTestSamples;
    }   

    // Configure the gyro and accelerometer for normal operation
    mI2C->write(MPU9250_ACCEL_CONFIG, 0x00);  
    mI2C->write(MPU9250_GYRO_CONFIG,  0x00);  
    delay(25);  // Delay a while to let the device stabilize
    
    for (int i=0; i < 6; i++) {
        // Retrieve accelerometer and gyro factory Self-Test Code from USR Register
        self_test[i] = mI2C->read(mSelfTestAddr[i]);
        // Retrieve factory self-test value from self-test code reads
        factory_trim[i] = (float) (2620 / 1<<FS) * (pow( 1.01 , ((float)self_test[i] - 1.0) ));
    }
    
    // Report results as a ratio of (STR - FT)/FT; the change from Factory Trim of the Self-Test Response
    // To get percent, must multiply by 100
    for (int i = 0; i < 3; i++) {
        dest[i]   = 100.0*((float)(accel_STAvg[i] - accel_avg[i]))/factory_trim[i];   // Report percent differences
        dest[i+3] = 100.0*((float)(gyro_STAvg[i] - gyro_avg[i]))/factory_trim[i+3]; // Report percent differences
    }
}

void MPU9250::self_test_dump(float* selftest)
{
    Serial.print("x-axis self test: acceleration trim within : "); Serial.print(selftest[0],1); Serial.println("% of factory value");
    Serial.print("y-axis self test: acceleration trim within : "); Serial.print(selftest[1],1); Serial.println("% of factory value");
    Serial.print("z-axis self test: acceleration trim within : "); Serial.print(selftest[2],1); Serial.println("% of factory value");
    Serial.print("x-axis self test: gyration trim within : "); Serial.print(selftest[3],1); Serial.println("% of factory value");
    Serial.print("y-axis self test: gyration trim within : "); Serial.print(selftest[4],1); Serial.println("% of factory value");
    Serial.print("z-axis self test: gyration trim within : "); Serial.print(selftest[5],1); Serial.println("% of factory value");
}

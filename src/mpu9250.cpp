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


float MPU9250::calc_gyro_resolution(int gyroResolution)
{
    switch (gyroResolution) {
    // Possible gyro scales (and their register bit settings) are:
    // 250 DPS (00), 500 DPS (01), 1000 DPS (10), and 2000 DPS  (11). 
    // Here's a bit of an algorithm to calculate DPS/(ADC tick) based on that 2-bit value:
        case GFS_500DPS:
              return 500.0/32768.0;
        case GFS_1000DPS:
              return 1000.0/32768.0;
        case GFS_2000DPS:
              return 2000.0/32768.0;
        case GFS_250DPS:
        default:
            return 250.0/32768.0;
    }   
}

float MPU9250::calc_accel_resolution(int accelResolution)
{
    switch (accelResolution) {
    // Possible accelerometer scales (and their register bit settings) are:
    // 2 Gs (00), 4 Gs (01), 8 Gs (10), and 16 Gs  (11). 
    // Here's a bit of an algorith to calculate DPS/(ADC tick) based on that 2-bit value:
    case AFS_4G:
        return 4.0/32768.0;
    case AFS_8G:
        return 8.0/32768.0;
    case AFS_16G:
        return 16.0/32768.0;
    case AFS_2G:
    default:
        return 2.0/32768.0;
    }
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

void MPU9250::receive(void)
{
    if (mI2C->read(MPU9250_INT_STATUS) & 0x01) {  // check if data ready interrupt
        
        this->accel_data(mAccelCurrent);  // Read the x/y/z adc values

        // Now we'll calculate the accleration value into actual g's
        this->acc_x = (float)mAccelCurrent[0]*this->mAccelResolution - mAccelCalibration[0];  // get actual g value, this depends on scale being set
        this->acc_y = (float)mAccelCurrent[1]*this->mAccelResolution - mAccelCalibration[1];   
        this->acc_z = (float)mAccelCurrent[2]*this->mAccelResolution- mAccelCalibration[2];  

        this->gyro_data(mGyroCurrent);  // Read the x/y/z adc values

        // Calculate the gyro value into actual degrees per second
        this->gyro_x = (float)mGyroCurrent[0]*this->mGyroResolution;  // get actual gyro value, this depends on scale being set
        this->gyro_y = (float)mGyroCurrent[1]*this->mGyroResolution;  
        this->gyro_z = (float)mGyroCurrent[2]*this->mGyroResolution;   
    }
}

void MPU9250::init(void)
{
    this->calibrate();
    
    // wake up device
    mI2C->write(MPU9250_PWR_MGMT_1, 0x00); // Clear sleep mode bit (6), enable all sensors 
    delay(100); // Wait for all registers to reset 

    // get stable time source
    mI2C->write(MPU9250_PWR_MGMT_1, 0x01);  // Auto select clock source to be PLL gyroscope reference if ready else
    delay(200); 

    // Configure Gyro and Thermometer
    // Disable FSYNC and set thermometer and gyro bandwidth to 41 and 42 Hz, respectively; 
    // minimum delay time for this setting is 5.9 ms, which means sensor fusion update rates cannot
    // be higher than 1 / 0.0059 = 170 Hz
    // DLPF_CFG = bits 2:0 = 011; this limits the sample rate to 1000 Hz for both
    // With the MPU9250, it is possible to get gyro sample rates of 32 kHz (!), 8 kHz, or 1 kHz
    mI2C->write(MPU9250_CONFIG, 0x03);  

    // Set sample rate = gyroscope output rate/(1 + SMPLRT_DIV)
    mI2C->write(MPU9250_SMPLRT_DIV, 0x04);  // Use a 200 Hz rate; a rate consistent with the filter update rate 
                                    // determined inset in CONFIG above

    // Set gyroscope full scale range
    // Range selects FS_SEL and AFS_SEL are 0 - 3, so 2-bit values are left-shifted into positions 4:3
    uint8_t c = mI2C->read(MPU9250_GYRO_CONFIG); // get current GYRO_CONFIG register value
    // c = c & ~0xE0; // Clear self-test bits [7:5] 
    c = c & ~0x02; // Clear Fchoice bits [1:0] 
    c = c & ~0x18; // Clear AFS bits [4:3]
    c = c | mGyroScale << 3; // Set full scale range for the gyro
    // c =| 0x00; // Set Fchoice for the gyro to 11 by writing its inverse to bits 1:0 of GYRO_CONFIG
    mI2C->write(MPU9250_GYRO_CONFIG, c ); // Write new GYRO_CONFIG value to register

    // Set accelerometer full-scale range configuration
    c = mI2C->read(MPU9250_ACCEL_CONFIG); // get current ACCEL_CONFIG register value
    // c = c & ~0xE0; // Clear self-test bits [7:5] 
    c = c & ~0x18;  // Clear AFS bits [4:3]
    c = c | mAccelScale << 3; // Set full scale range for the accelerometer 
    mI2C->write(MPU9250_ACCEL_CONFIG, c); // Write new ACCEL_CONFIG register value

    // Set accelerometer sample rate configuration
    // It is possible to get a 4 kHz sample rate from the accelerometer by choosing 1 for
    // accel_fchoice_b bit [3]; in this case the bandwidth is 1.13 kHz
    c = mI2C->read(MPU9250_ACCEL_CONFIG2); // get current ACCEL_CONFIG2 register value
    c = c & ~0x0F; // Clear accel_fchoice_b (bit 3) and A_DLPFG (bits [2:0])  
    c = c | 0x03;  // Set accelerometer rate to 1 kHz and bandwidth to 41 Hz
    mI2C->write(MPU9250_ACCEL_CONFIG2, c); // Write new ACCEL_CONFIG2 register value

    // The accelerometer, gyro, and thermometer are set to 1 kHz sample rates, 
    // but all these rates are further reduced by a factor of 5 to 200 Hz because of the SMPLRT_DIV setting

    // Configure Interrupts and Bypass Enable
    // Set interrupt pin active high, push-pull, hold interrupt pin level HIGH until interrupt cleared,
    // clear on read of INT_STATUS, and enable I2C_BYPASS_EN so additional chips 
    // can join the I2C bus and all can be controlled by the Arduino as master
    mI2C->write(MPU9250_INT_PIN_CFG, 0x22);    
    mI2C->write(MPU9250_INT_ENABLE, 0x01);  // Enable data ready (bit 0) interrupt
    delay(100);
   
    this->mGyroResolution = this->calc_gyro_resolution(mGyroScale);
    this->mAccelResolution = this->calc_accel_resolution(mAccelScale);
}

void MPU9250::calibrate()
{   
    uint8_t data[12]; // data array to hold accelerometer and gyro x, y, z, data
    uint16_t packet_count, fifo_count;
    int32_t gyro_bias[3]  = {0, 0, 0}, accel_bias[3] = {0, 0, 0};

    // reset device
    mI2C->write(MPU9250_PWR_MGMT_1, 0x80); // Write a one to bit 7 reset bit; toggle reset device
    delay(100);

    // get stable time source; Auto select clock source to be PLL gyroscope reference if ready 
    // else use the internal oscillator, bits 2:0 = 001
    mI2C->write(MPU9250_PWR_MGMT_1, 0x01);  
    mI2C->write(MPU9250_PWR_MGMT_2, 0x00);
    delay(200);                                    

    // Configure device for bias calculation
    mI2C->write(MPU9250_INT_ENABLE, 0x00);   // Disable all interrupts
    mI2C->write(MPU9250_FIFO_EN, 0x00);      // Disable FIFO
    mI2C->write(MPU9250_PWR_MGMT_1, 0x00);   // Turn on internal clock source
    mI2C->write(MPU9250_I2C_MST_CTRL, 0x00); // Disable I2C master
    mI2C->write(MPU9250_USER_CTRL, 0x00);    // Disable FIFO and I2C master modes
    mI2C->write(MPU9250_USER_CTRL, 0x0C);    // Reset FIFO and DMP
    delay(15);

    // Configure MPU9250 gyro and accelerometer for bias calculation
    mI2C->write(MPU9250_CONFIG, 0x01);      // Set low-pass filter to 188 Hz
    mI2C->write(MPU9250_SMPLRT_DIV, 0x00);  // Set sample rate to 1 kHz
    mI2C->write(MPU9250_GYRO_CONFIG, 0x00);  // Set gyro full-scale to 250 degrees per second, maximum sensitivity
    mI2C->write(MPU9250_ACCEL_CONFIG, 0x00); // Set accelerometer full-scale to 2 g, maximum sensitivity

    uint16_t  gyrosensitivity  = 131;   // = 131 LSB/degrees/sec
    uint16_t  accelsensitivity = 16384;  // = 16384 LSB/g

    // Configure FIFO to capture accelerometer and gyro data for bias calculation
    mI2C->write(MPU9250_USER_CTRL, 0x40);   // Enable FIFO  
    mI2C->write(MPU9250_FIFO_EN, 0x78);     // Enable gyro and accelerometer sensors for FIFO  (max size 512 bytes in MPU-9150)
    delay(40); // accumulate 40 samples in 40 milliseconds = 480 bytes

    // At end of sample accumulation, turn off FIFO sensor read
    mI2C->write(MPU9250_FIFO_EN, 0x00);        // Disable gyro and accelerometer sensors for FIFO
    mI2C->read(MPU9250_FIFO_COUNTH, 2, &data[0]); // read FIFO sample count
    fifo_count = ((uint16_t)data[0] << 8) | data[1];
    packet_count = fifo_count/12;// How many sets of full gyro and accelerometer data for averaging

    for (int i = 0; i < packet_count; i++) {
        int16_t accel_temp[3] = {0, 0, 0}, gyro_temp[3] = {0, 0, 0};
        mI2C->read(MPU9250_FIFO_R_W, 12, &data[0]); // read data for averaging
        accel_temp[0] = (int16_t) (((int16_t)data[0] << 8) | data[1]  ) ;  // Form signed 16-bit integer for each sample in FIFO
        accel_temp[1] = (int16_t) (((int16_t)data[2] << 8) | data[3]  ) ;
        accel_temp[2] = (int16_t) (((int16_t)data[4] << 8) | data[5]  ) ;    
        gyro_temp[0]  = (int16_t) (((int16_t)data[6] << 8) | data[7]  ) ;
        gyro_temp[1]  = (int16_t) (((int16_t)data[8] << 8) | data[9]  ) ;
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

    if (accel_bias[2] > 0L) {
        accel_bias[2] -= (int32_t) accelsensitivity;   // Remove gravity from the z-axis accelerometer bias calculation
    }
    else {
        accel_bias[2] += (int32_t) accelsensitivity;
    }

    // Construct the gyro biases for push to the hardware gyro bias registers, which are reset to zero upon device startup
    data[0] = (-gyro_bias[0]/4  >> 8) & 0xFF; // Divide by 4 to get 32.9 LSB per deg/s to conform to expected bias input format
    data[1] = (-gyro_bias[0]/4)       & 0xFF; // Biases are additive, so change sign on calculated average gyro biases
    data[2] = (-gyro_bias[1]/4  >> 8) & 0xFF;
    data[3] = (-gyro_bias[1]/4)       & 0xFF;
    data[4] = (-gyro_bias[2]/4  >> 8) & 0xFF;
    data[5] = (-gyro_bias[2]/4)       & 0xFF;

    // Push gyro biases to hardware registers
    mI2C->write(MPU9250_XG_OFFSET_H, data[0]);
    mI2C->write(MPU9250_XG_OFFSET_L, data[1]);
    mI2C->write(MPU9250_YG_OFFSET_H, data[2]);
    mI2C->write(MPU9250_YG_OFFSET_L, data[3]);
    mI2C->write(MPU9250_ZG_OFFSET_H, data[4]);
    mI2C->write(MPU9250_ZG_OFFSET_L, data[5]);

    // Output scaled gyro biases for display in the main program
    mGyroCalibration[0] = (float) gyro_bias[0]/(float) gyrosensitivity;  
    mGyroCalibration[1] = (float) gyro_bias[1]/(float) gyrosensitivity;
    mGyroCalibration[2] = (float) gyro_bias[2]/(float) gyrosensitivity;

    // Construct the accelerometer biases for push to the hardware accelerometer bias registers. These registers contain
    // factory trim values which must be added to the calculated accelerometer biases; on boot up these registers will hold
    // non-zero values. In addition, bit 0 of the lower byte must be preserved since it is used for temperature
    // compensation calculations. Accelerometer bias registers expect bias input as 2048 LSB per g, so that
    // the accelerometer biases calculated above must be divided by 8.

    int32_t accel_bias_reg[3] = {0, 0, 0}; // A place to hold the factory accelerometer trim biases
    mI2C->read(MPU9250_XA_OFFSET_H, 2, &data[0]); // Read factory accelerometer trim values
    accel_bias_reg[0] = (int32_t) (((int16_t)data[0] << 8) | data[1]);
    mI2C->read(MPU9250_YA_OFFSET_H, 2, &data[0]);
    accel_bias_reg[1] = (int32_t) (((int16_t)data[0] << 8) | data[1]);
    mI2C->read(MPU9250_ZA_OFFSET_H, 2, &data[0]);
    accel_bias_reg[2] = (int32_t) (((int16_t)data[0] << 8) | data[1]);

    uint32_t mask = 1uL; // Define mask for temperature compensation bit 0 of lower byte of accelerometer bias registers
    uint8_t mask_bit[3] = {0, 0, 0}; // Define array to hold mask bit for each accelerometer bias axis

    for (int i = 0; i < 3; i++) {
        if((accel_bias_reg[i] & mask)) mask_bit[i] = 0x01; // If temperature compensation bit is set, record that fact in mask_bit
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
    /*  writeByte(MPU9250_ADDRESS, XA_OFFSET_H, data[0]);
    writeByte(MPU9250_ADDRESS, XA_OFFSET_L, data[1]);
    writeByte(MPU9250_ADDRESS, YA_OFFSET_H, data[2]);
    writeByte(MPU9250_ADDRESS, YA_OFFSET_L, data[3]);
    writeByte(MPU9250_ADDRESS, ZA_OFFSET_H, data[4]);
    writeByte(MPU9250_ADDRESS, ZA_OFFSET_L, data[5]);
    */
    // Output scaled accelerometer biases for display in the main program
    mAccelCalibration[0] = (float)accel_bias[0]/(float)accelsensitivity; 
    mAccelCalibration[1] = (float)accel_bias[1]/(float)accelsensitivity;
    mAccelCalibration[2] = (float)accel_bias[2]/(float)accelsensitivity;
   
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

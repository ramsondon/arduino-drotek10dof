#include "mpu9250.h"
#include <stdint.h>

MPU9250::MPU9250()
{
    mI2C = new I2C(MPU9250_ADDRESS);
}


bool MPU9250::available(void)
{
    return (mI2C->read(MPU9250_WHO_AM_I) == 0x71);
}
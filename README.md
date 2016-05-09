# Breakout Board library MPU9250 and MS5611 delivered by drotek
### under development


	this library has been tested with arduino due
	for testing purposes the breakout board has been connected 
	directly to the 3.3v output pin of the arduino due.

	10DOF ----> Arduino
	SDA   ----> SDA (20)
	SCL   ----> SCL (21)
	VDD   ----> 3.3v
	GND   ----> GND


	Requirements: https://github.com/ramsondon/arduino-i2c


```c
#include "mpu9250.h"

MPU9250* mpu;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  
  mpu = new MPU9250();
  
  if (true == mpu->available()) {
    Serial.println("mpu init success");
    mpu->init();
    
  } else {
    Serial.println("mpu init with failures");
    while(1) ; // Loop forever if communication doesn't happen
  }
}

void loop() {
  mpu->receive();
  Serial.print("gyro: X-Axis: "); Serial.print(mpu->gyro_x);
  Serial.print(" Y-Axis: "); Serial.print(mpu->gyro_y);
  Serial.print(" Z-Axis: "); Serial.println(mpu->gyro_z);
  delay(100);
}

```

References:
https://github.com/kriswiner/MPU-9250/blob/master/MPU9250_MS5611_BasicAHRS_t3.ino

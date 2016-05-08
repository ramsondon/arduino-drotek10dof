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

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  
  MPU9250* mpu = new MPU9250();
  if (true == mpu->available()) {
    Serial.println("mpu init success");
  } else {
    Serial.println("mpu init with failures");
  }
}

void loop() {}
```

#pragma once

#include <globals.h>        // Global libraries and variables
#include <Button.h>         // Simple button library
#include <MPU6050_tockn.h>  // MPU6050 Gyro + Accelometer
#include <Wire.h>           // I2C device support

namespace sensor
{
Button button(BUTTON_PIN);

MPU6050 mpu6050(Wire);

bool useIMU = true;

constexpr int8_t X_AXIS = 0;
constexpr int8_t Y_AXIS = 1;
constexpr int8_t Z_AXIS = 2;

void initGyro(bool calcOffsets = false)
{
  if (!useIMU) return;

#if defined ARDUINO_ESP8266_WEMOS_OAK
  Wire.begin(P0, P5);  // SDA, SCL
#elif defined ARDUINO_ESP8266_WEMOS_D1MINI
  Wire.begin(D2, D1);  // SDA, SCL
#endif

  mpu6050.begin();
  if (calcOffsets) mpu6050.calcGyroOffsets(false, 100, 100);
}

void gyroUpdate()
{
  if (useIMU) mpu6050.update();
}

float gyroGetValue(int axis = Y_AXIS, bool multipleReadings = false)
{
  if (!useIMU) return 0;

  if (multipleReadings) {
    for (int i = 0; i < 10; i++) {
      sensor::gyroUpdate();
      delay(5);
    }
  }
  if (axis == X_AXIS) return mpu6050.getAngleX();
  if (axis == Y_AXIS) return mpu6050.getAngleY();
  if (axis == Z_AXIS) return mpu6050.getAngleZ();
  return 0;
}
}  // namespace sensor
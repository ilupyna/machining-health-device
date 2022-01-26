#include "I2Cdev.h"

//#include "Wire.h"
//
//uint8_t i2cData[14]; // Buffer for I2C data
//
#define MPU6050_RA_ACCEL_XOUT_H 0x3B
#define IMUAddress 0x68

#define ACCEL_BUFFER_SIZE 6
uint8_t buffer[ACCEL_BUFFER_SIZE];
//const uint16_t I2C_TIMEOUT = 1000; // Used to check for errors in I2C communication
//
//
//
//uint8_t i2cWrite(uint8_t registerAddress, uint8_t data, bool sendStop) {
//  return i2cWrite(registerAddress, &data, 1, sendStop); // Returns 0 on success
//}
//
//uint8_t i2cWrite(uint8_t registerAddress, uint8_t *data, uint8_t length, bool sendStop) {
//  Wire.beginTransmission(IMUAddress);
//  Wire.write(registerAddress);
//  Wire.write(data, length);
//  uint8_t rcode = Wire.endTransmission(sendStop); // Returns 0 on success
//  if (rcode) {
//    Serial.print(F("i2cWrite failed: "));
//    Serial.println(rcode);
//  }
//  return rcode; // See: http://arduino.cc/en/Reference/WireEndTransmission
//}
//
//uint8_t i2cRead(uint8_t registerAddress, uint8_t *data, uint8_t nbytes) {
//  uint32_t timeOutTimer;
//  Wire.beginTransmission(IMUAddress);
//  Wire.write(registerAddress);
//  uint8_t rcode = Wire.endTransmission(false); // Don't release the bus
//  if (rcode) {
//    Serial.print(F("i2cRead failed: "));
//    Serial.println(rcode);
//    return rcode; // See: http://arduino.cc/en/Reference/WireEndTransmission
//  }
//  Wire.requestFrom(IMUAddress, nbytes, (uint8_t)true); // Send a repeated start and then release the bus after reading
//  for (uint8_t i = 0; i < nbytes; i++) {
//    if (Wire.available())
//      data[i] = Wire.read();
//    else {
//      timeOutTimer = micros();
//      while (((micros() - timeOutTimer) < I2C_TIMEOUT) && !Wire.available());
//      if (Wire.available())
//        data[i] = Wire.read();
//      else {
//        Serial.println(F("i2cRead timeout"));
//        return 5; // This error value is not already taken by endTransmission
//      }
//    }
//  }
//  return 0; // Success
//}

void getAccel(int16_t* x, int16_t* y, int16_t* z) {
//  I2Cdev::readBytes(devAddr, MPU6050_RA_ACCEL_XOUT_H, ACCEL_BUFFER_SIZE, buffer);
  I2Cdev::readBytes(IMUAddress, MPU6050_RA_ACCEL_XOUT_H, ACCEL_BUFFER_SIZE, buffer);
  *x = (((int16_t)buffer[0]) << 8) | buffer[1];
  *y = (((int16_t)buffer[2]) << 8) | buffer[3];
  *z = (((int16_t)buffer[4]) << 8) | buffer[5];

//  while (i2cRead(0x3B, i2cData, 6));  //14
//  accX = ((i2cData[0] << 8) | i2cData[1]);
//  accY = ((i2cData[2] << 8) | i2cData[3]);
//  accZ = ((i2cData[4] << 8) | i2cData[5]);
////  tempRaw = (i2cData[6] << 8) | i2cData[7];
}

void getAccelInString(String& s) {
//  I2Cdev::readBytes(devAddr, MPU6050_RA_ACCEL_XOUT_H, ACCEL_BUFFER_SIZE, buffer);
  I2Cdev::readBytes(IMUAddress, MPU6050_RA_ACCEL_XOUT_H, ACCEL_BUFFER_SIZE, buffer);
  for (int i=0; i<ACCEL_BUFFER_SIZE; i++) {
    s += String(buffer[i]);
  }
//  s += ';';
}

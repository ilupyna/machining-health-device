#include <LittleFS.h>
#include "I2Cdev.h"
#include "MPU6050.h"

#include <time.h>
#include <String>

#define LED_BUILTIN 2
#define MAX_DATA_SIZE 1000

bool led_state = true;  //set off by default
bool SensorLive = false;


unsigned long Start, Finish, s2, f2, savetime = 0;
size_t iterations;


MPU6050 accelgyro;
int16_t ax, ay, az;
int16_t gx, gy, gz;


struct data {
  time_t timeStomp;
  int16_t ax, ay, az;
};

data dataStorage[MAX_DATA_SIZE];



void SensorMeasurements() {
  accelgyro.getAcceleration(&ax, &ay, &az);
  time_t now = time(nullptr);
  
  dataStorage[iterations] = {now, ax, ay, az};

  ++iterations;
  
  Serial.print(ax);  Serial.print(' ');
  Serial.print(ay);  Serial.print(' ');
  Serial.print(az);  Serial.println();
  Serial.flush();
//  Serial.println(accelgyro.getMotionStatus());
//  Serial.println(accelgyro.getZeroMotionDetected());
}



void ParseConsole() {
  if (Serial.available() > 0) {
    const String last_console_text = Serial.readStringUntil('\n');
    
  if (last_console_text == "stats") {
      const unsigned long MeasureTime = Finish - Start;
      Serial.print(F("WorkTime: "));
      Serial.print((MeasureTime)/1000.0);
      Serial.println(F("ms;"));
      Serial.print(F("V = "));
      Serial.print((MeasureTime/MAX_DATA_SIZE)/1000.0);
//      Serial.print((MeasureTime/iterations)/1000.0);
      Serial.println(F("kHz;"));
      
      FSInfo filesystem_info;
      LittleFS.info(filesystem_info);
      Serial.println(filesystem_info.usedBytes);
      Serial.println(filesystem_info.totalBytes);
      
      Serial.print(F("SaveTime: "));
      Serial.print(savetime/1000.0);
      Serial.println(F("ms;"));
      
//      Serial.print(F("Size of time_t : "));
//      Serial.println(sizeof(time_t));
      //8
    }
    else if (last_console_text == "msrmnt") {
      SensorLive = !SensorLive;
      if (SensorLive) {
        Serial.println(F("ax ay az"));
        Start = micros();
      }
      else {
        Finish = micros();
      }
    }
    Serial.flush();
  }
}



void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, led_state);
//  system_update_cpu_freq(160);
  
  Serial.begin(1000000);
  delay(2500);
  
  Serial.println(F("Initializing GY-87..."));
  Serial.flush();

  Wire.begin();
//  Wire.setClock(400000);
  accelgyro.initialize();
  Serial.println(accelgyro.testConnection() ? "MPU6050 connection successful" : "MPU6050 connection failed");
  
//  Serial.print(F("DLPFMode:"));
//  Serial.println(accelgyro.getDLPFMode());
  //0
//  accelgyro.setDLPFMode(1);

//  Serial.print(F("DHPFMode: "));
//  Serial.println(accelgyro.getDHPFMode());
  //0

//  Serial.print(F("ScaleAccel: "));
//  Serial.println(accelgyro.getFullScaleAccelRange());
  //0
  
  Serial.println(F("\nTest: "));
//  accelgyro.CalibrateAccel(1);
  accelgyro.PrintActiveOffsets();
  

//  Serial.println(F("\nTest2: "));
//  Serial.print(F("MotionDetectionThreshold: "));
//  Serial.println(accelgyro.getMotionDetectionThreshold());
  //0
//  Serial.print(F("MotionDetectionDuration: "));
//  Serial.println(accelgyro.getMotionDetectionDuration());
  //0
//  Serial.print(F("SleepEnabled: "));
//  Serial.println(accelgyro.getSleepEnabled());  
  //0
//  Serial.print(F("WakeCycleEnabled: "));
//  Serial.println(accelgyro.getWakeCycleEnabled());  
  //0
//  Serial.print(F("TempSensorEnabled: "));
//  Serial.println(accelgyro.getTempSensorEnabled());
  //1
//  Serial.print(F("ClockSource: "));
//  Serial.println(accelgyro.getClockSource());
  //1
//  Serial.print(F("WakeFrequency: "));
//  Serial.println(accelgyro.getWakeFrequency());
  
//  Serial.println(F("\nTest3: "));
//  Serial.print(F("DeviceID: "));
//  Serial.println(accelgyro.getDeviceID(), HEX);
  //0x34
  
  
  Serial.print(F("Trying to filesystem: "));
  if(LittleFS.begin()) {
    Serial.println(F("success!"));
    FSInfo filesystem_info;
    LittleFS.info(filesystem_info);
    Serial.println(filesystem_info.totalBytes);
    Serial.println(filesystem_info.usedBytes);
    
    const String filename2 = "/SensorData.txt";
    if(LittleFS.exists(filename2)) {
      LittleFS.remove(filename2);
    }
  }
  else {
    Serial.println(F("fail."));
    return;
  }
  Serial.println();
}



void loop() {
  ParseConsole();
  if (SensorLive) {
    SensorMeasurements();
//    if(iterations==MAX_DATA_SIZE) {
//  //    SensorLive = false;
//      Finish = micros();
//      
//      s2 = micros();
//      FSInfo filesystem_info;
//      LittleFS.info(filesystem_info);
//      if(filesystem_info.usedBytes < filesystem_info.totalBytes/3) {
//        File currentFile = LittleFS.open(F("/SensorData.txt"), "a");
//        for (int i = 0; i<iterations; i++) {
//          currentFile.println(ctime(&dataStorage[i].timeStomp));
//          currentFile.print(dataStorage[i].ax); currentFile.print(',');
//          currentFile.print(dataStorage[i].ay); currentFile.print(',');
//          currentFile.print(dataStorage[i].az); currentFile.println(';');
//        }
//        currentFile.close();
//  //      Serial.println(F("keks"));
//      }
//      f2 = micros();
//      savetime += f2-s2;
//      iterations = 0;
//    }
  }
}

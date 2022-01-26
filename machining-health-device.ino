#include <LittleFS.h>
#include "I2Cdev.h"
#include "MPU6050.h"

#include <time.h>
#include <String>

#define LED_BUILTIN 2
#define MAX_ITERATIONS 10000

bool led_state = true;  //set off by default
bool SensorLive = false;


unsigned long Start, Finish, WorkTime = 0;
size_t iterations;

File currentFile;


MPU6050 accelgyro;
int16_t ax, ay, az;



void SensorMeasurements() {
  accelgyro.getAcceleration(&ax, &ay, &az);
  
  currentFile.println(time(nullptr));
  currentFile.print(ax); currentFile.print(',');
  currentFile.print(ay); currentFile.print(',');
  currentFile.print(az); currentFile.println(';');
  
  ++iterations;
}



void ParseConsole() {
  if (Serial.available() > 0) {
    const String last_console_text = Serial.readStringUntil('\n');
    
    if (last_console_text == "stats") {
      Serial.print(F("WorkTime: "));
      Serial.print(WorkTime/1000.0);
      Serial.println(F("ms;"));
      Serial.print(F("V = "));
      Serial.print(1000.0*iterations/WorkTime);
      Serial.println(F("kHz;"));
      
      FSInfo filesystem_info;
      LittleFS.info(filesystem_info);
      Serial.println(filesystem_info.totalBytes);
      Serial.println(filesystem_info.usedBytes);
    }
    else if (last_console_text == "msrmnt") {
      SensorLive = !SensorLive;
      if (SensorLive) {
        Serial.println(F("start"));
        Start = micros();
        currentFile = LittleFS.open(F("/SensorData.txt"), "a");
      }
      else {
        currentFile.close();
        SensorLive = false;
        Finish = micros();
        
        WorkTime = Finish - Start;
        Serial.println(F("finish"));
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
  accelgyro.initialize();
  Serial.println(accelgyro.testConnection() ? "MPU6050 connection successful" : "MPU6050 connection failed");
  
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
    
    if(iterations==MAX_DATA_SIZE) {
      currentFile.close();
      SensorLive = false;
      Finish = micros();
      
      WorkTime = Finish - Start;
      Serial.println(F("keks"));
    }
  }
}

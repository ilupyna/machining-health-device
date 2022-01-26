
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include "I2Cdev.h"
#include "MPU6050.h"

//#include <time.h>
#include <String>

#include "help.h"

#define LED_BUILTIN 2

const char* ssid = "yourNetwork";
const char* password = "secretPassword";

bool led_state = true;  //set off by default
bool SensorLive = false;

ESP8266WebServer server(80);

MPU6050 accelgyro;
int16_t ax, ay, az;

#define MAX_RESPONSE_SIZE 65536 //1048576 65536
#define MAX_SINGLE_RESPONSE_SIZE 65472 //MAX_RESPONSE_SIZE-64
#define CSV_HEADER "" //"time,ax,ay,az;"

String MeasurementsForResponse = CSV_HEADER;
//String MeasurementsForResponse2 = CSV_HEADER;
//bool useFirstResponseString = true;


#define TIMER_INTERVAL_MICRO 1001

unsigned long MeasureStartTime, MeasureFinTime, MesInterruptStartTime;
unsigned long SumWorkTimeMs = 0, SumResponseTime = 0;
unsigned long SumWriteMeasureTime = 0, SumGetMeasureTime = 0, SumInterruptDiffTime = 0;
size_t measure_iterations = 0, response_iterations = 0;
size_t SumResponseSize = 0;



void StartMeasurement() {
  SensorLive = true;
  MeasureStartTime = micros();
  MesInterruptStartTime = micros();
  Serial.println(F("Start measure"));
  AttachTimerInterrupt(TIMER_INTERVAL_MICRO, SensorMeasurements);
}

void StopMeasurement() {
  SensorLive = false;
  MeasureFinTime = micros();
  SumWorkTimeMs += (MeasureFinTime - MeasureStartTime)/1000.0;
  ax = ay = az = 0;
  Serial.println(F("Stop measure"));
  TimerInterruptDisable();
}



void SensorMeasurements() {
  noInterrupts();
  
  SumInterruptDiffTime += micros() - MesInterruptStartTime;
  MesInterruptStartTime = micros();
  
  accelgyro.getAcceleration(&ax, &ay, &az);
  const auto mesGet = micros();
  MeasurementsForResponse += String(ax) +','+ String(ay) +','+ String(az) +';'; //String(time(nullptr)) +','+
  const auto mesFin = micros();
  
  SumGetMeasureTime += (mesGet - MesInterruptStartTime);
  SumWriteMeasureTime += (mesFin - mesGet);
  ++measure_iterations;
  
//  if(MeasurementsForResponse.length() > MAX_SINGLE_RESPONSE_SIZE) {
//    StopMeasurement();
//  }
  interrupts();
}



void ParseConsole() {
  if (Serial.available() > 0) {
    const String last_console_text = Serial.readStringUntil('\n');
    
    if (last_console_text == "mem") {
      Serial.println(F("~Memory Stats~"));
      Serial.print(F("FreeHeap: "));
      Serial.println(ESP.getFreeHeap(), DEC);
      Serial.print(F("HeapFragmentation: "));
      Serial.println(ESP.getHeapFragmentation());
      Serial.print(F("FlashChipSize: "));
      Serial.println(ESP.getFlashChipSize(), DEC);
      Serial.println();
    }
    else if (last_console_text == "stats") {
      unsigned long WorkTime = SumWorkTimeMs;
      if (SensorLive) {
        WorkTime += (micros()-MeasureStartTime)/1000.0;
      }
      Serial.print(F("WorkTime: "));
      Serial.print(WorkTime);
      Serial.println(F("ms;"));
      Serial.print(F("V = "));
      Serial.print(1.0*measure_iterations/WorkTime);
      Serial.println(F("kHz;"));
      
      Serial.print(F("GetMeasureTime: "));
      Serial.print(SumGetMeasureTime/measure_iterations);
      Serial.println(F("µs;"));
      
      Serial.print(F("WriteMeasureTime: "));
      Serial.print(SumWriteMeasureTime/measure_iterations);
      Serial.println(F("µs;"));

      Serial.print(F("InterruptDiffTime: "));
      Serial.print(SumInterruptDiffTime/measure_iterations);
      Serial.println(F("µs;"));
      
      Serial.print(F("ResponseTime: "));
      Serial.print(SumResponseTime/response_iterations);
      Serial.println(F("µs;"));
      
      Serial.print(F("ResponseSize: "));
      Serial.print(SumResponseSize/response_iterations);
      Serial.println(F("b;"));
      Serial.println();
    }
    else if (last_console_text == "clear") {
      MeasureStartTime = MeasureFinTime = 0;
      SumWorkTimeMs = SumResponseTime = SumGetMeasureTime = SumWriteMeasureTime = 0;
      response_iterations = measure_iterations = 0;
      SumResponseSize = 0;
      Serial.println(F("Cleared"));
    }
    Serial.flush();
  }
}



void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, led_state);
  
  Serial.begin(1000000);
  delay(2500);
  
  Serial.println(F("Initializing GY-87..."));
  Serial.flush();
  
  Wire.begin();
  Wire.setClock(400000);
  accelgyro.initialize();
  
  if( accelgyro.testConnection() ) {
    Serial.println(F("MPU6050 connection successful"));
    readyBlinking();
  }
  else {
    Serial.println(F("MPU6050 connection failed"));
    delay(2500);
    return;
  }
  
  Serial.print(F("DLPFMode:"));
  Serial.print(accelgyro.getDLPFMode());
//  accelgyro.setDLPFMode(0);  //1
  Serial.print(',');
  Serial.println(accelgyro.getDLPFMode());
//  
  Serial.print(F("getRate:"));
  Serial.print(accelgyro.getRate());
//  accelgyro.setRate(0);  //4
  Serial.print(',');
  Serial.print(accelgyro.getRate());
  
  // Connect to WiFi network
  Serial.println();
  Serial.println();
  Serial.print(F("Connecting to "));
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();
  Serial.println(F("WiFi connected"));
  
  
  Serial.println(F("Syncing time"));
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov"); //ua.pool.ntp.org
  while (!time(nullptr)) {
    Serial.print('.');
    delay(100);
  }
  
  
  server.on("/", [](){
    server.send(200, "text/html", preparedHtmlPage);
  });
  
  server.on("/req/post", HTTP_POST, [](){
    String msg = "Number of args received:";
    msg += server.args();      // получить количество параметров
    msg += "\n";               // переход на новую строку
    
    for (int i = 0; i < server.args(); i++) {
      msg += "Arg #" + (String)i + " –> "; // добавить текущее значение счетчика
      msg += server.argName(i) + ": ";      // получить имя параметра
      msg += server.arg(i) + "\n";          // получить значение параметра
    }
    
    Serial.print(msg);
    
    if (server.arg("Led") == "ON") {
      Serial.println(F("Led go ON"));
      led_state = LOW;
      digitalWrite(LED_BUILTIN, led_state);
    }
    else if (server.arg("Led") == "OFF") {
      Serial.println(F("Led go OFF"));
      led_state = HIGH;
      digitalWrite(LED_BUILTIN, led_state);
    }
    else if (server.arg("Msrmnt") == "ON") {
      Serial.println(F("Msrmnt start"));
      StartMeasurement();
    }
    else if (server.arg("Msrmnt") == "OFF") {
      Serial.println(F("Msrmnt stop"));
      StopMeasurement();
    }
    
    Serial.flush();
    Serial.println();
    
    server.send(204);
  });
  
  
  MeasurementsForResponse.reserve(MAX_RESPONSE_SIZE);
  server.on("/msrmnt", HTTP_GET, [](){
    MeasurementsForResponse = CONST_STRING;//"1,1,1;";//CONST_STRING;
    SumResponseSize += MeasurementsForResponse.length();
    
    auto SendingStartTime = micros();
//    server.addHeader("Connection", "Keep-Alive");
    server.send(200, "text/csv", MeasurementsForResponse);  // arraybuffer/blob text/plain
    MeasurementsForResponse = CSV_HEADER;
    auto SendingFinTime = micros();
    
    SumResponseTime += SendingFinTime-SendingStartTime;
    ++response_iterations;
  });
  server.enableCORS(true);
  
  
  // Start the server
  server.begin();
  Serial.println(F("Server started"));
  
  // Print the IP address
  Serial.print(F("Use this URL to connect: "));
  Serial.print(F("http://"));
  Serial.print(WiFi.localIP());
  Serial.println('/');
  Serial.println(F("~~~~~~~~"));
}



void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("*** Disconnected from AP so rebooting ***"));
    ESP.reset();
  }
  
  ParseConsole();
  server.handleClient();
  
//  if (SensorLive) {
//    SensorMeasurements();
//  }
}

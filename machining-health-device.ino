#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include "I2Cdev.h"
#include "MPU6050.h"

//#include <time.h>
#include <String>

#include "help.h"


#include "arduinoFFT.h"
arduinoFFT FFT = arduinoFFT();


#define LED_BUILTIN 2

bool led_state = true;  //OFF by default
bool SensorLive = false;
bool MeasureWithTemperature = false;
bool WITH_ACCEL_RMS = false;
bool SEND_FFT_RES = true;


const char* ssid = "yourNetwork";
const char* password = "secretPassword";

ESP8266WebServer server(80);


#define MAX_RESPONSE_SIZE 65536 //1048576 65536
#define MAX_SINGLE_RESPONSE_SIZE 65472 //MAX_RESPONSE_SIZE-64
#define CSV_ACCEL_HEADER "" //"time,ax,ay,az;"
#define CSV_FFT_HEADER "" //"fft result for "+String(FFT_ITERATION)+" intervals";

String MeasurementsForResponse = CSV_ACCEL_HEADER;
String FFTResultsForResponse = CSV_FFT_HEADER;
String allertText;

uint8_t REQ_FORMAT = 1;
uint8_t FFT_TYPE = 5; 


#define MAX_MEASUREMENT_FREQ 1000
#define FFT_MULTIPLIER 2.56 //3.15
float FREQ_TO_MEASURE = 2.5; //10.666667
float SAMPLING_FREQ = 1000;  //FREQ_TO_MEASURE * FFT_MULTIPLIER;
float TIMER_INTERVAL_MICRO = 1001; //1000.0*1000.0/SAMPLING_FREQ;


MPU6050 accelgyro;
int16_t ax, ay, az;
int16_t temperature;
uint8_t ACCEL_RANGE = 0; // 0: +-2g; 1: +-4g; 2: +-8g; 3: +-16g


const uint16_t FFT_ITERATION = 256; //4 8 16 32 64 128 256 512 1024

struct AccelForVibrationDataChunk {
  float a_inG_arr[FFT_ITERATION];
  float v_inMMpS_arr[FFT_ITERATION];
  
  uint16_t current_size = 0;
  
  bool wasFFT = false;
  bool full = false;
  
  void add(const int16_t ax, const int16_t ay, const int16_t az) {
    const auto a_in_vec = accelToG<float>(getRMS(ax, ay, az), ACCEL_RANGE)*9.8;  //sqrt(sq(ax) + sq(ay) + sq(az));
    a_inG_arr[current_size] = a_in_vec;
    v_inMMpS_arr[current_size] = a_in_vec/(2.0*PI*SAMPLING_FREQ);
    
    ++current_size;
    wasFFT = false;
    
    if(current_size==FFT_ITERATION) {
//      Serial.println(F("InSizeCheck"));
      full = true;
      current_size = 0;
    }
  }
  
  float getPrevA() const {
    if(current_size==0) {
      return a_inG_arr[FFT_ITERATION-1];
    }
    return a_inG_arr[current_size-1];
  }
  
  float getPrevV() const {
    if(current_size==0) {
      return v_inMMpS_arr[FFT_ITERATION-1];
    }
    return v_inMMpS_arr[current_size-1];
  }
  
  template <typename T>
  void add(const T measurement) {
    a_inG_arr[current_size] = measurement;
    
    ++current_size;
    wasFFT = false;
    
    if(current_size==FFT_ITERATION) {
//      Serial.println(F("InSizeCheck"));
      full = true;
      current_size = 0;
    }
  }
  
  float getAccelRMS() const { //root mean squere value
    float sum_a = 0;
    for(uint16_t i=0; i<FFT_ITERATION; i++) {
      sum_a += sq(a_inG_arr[i]);
    }
    return sqrt(sum_a/FFT_ITERATION);
  }
  
  float getVelRMS() const { //root mean squere value
    float sum_v = 0;
    for(uint16_t i=0; i<FFT_ITERATION; i++) {
      sum_v += sq(v_inMMpS_arr[i]);
    }
    return sqrt(sum_v/FFT_ITERATION);
  }
  
  void clear() {
    current_size = 0;
    full = false;
  }
  
  uint16_t size() const {
    return current_size;
  }
};


AccelForVibrationDataChunk MPU_measurements;
float VibrationComparision[FFT_ITERATION];
float TemreratureComparision = 44;  //random number
float AccelRMSComparision = 12; //random number
float VelocityRMSComparision = 22;  //random number
float VelocityFromFFTComparision = 45; //random number

unsigned long SumFFTTime = 0;
size_t fft_iteration = 0;


unsigned long MeasureStartTime, MeasureFinTime, MesInterruptStartTime;
unsigned long SumWorkTimeMs = 0, SumResponseTime = 0;
unsigned long SumWriteMeasureTime = 0, SumGetMeasureTime = 0, SumInterruptDiffTime = 0;
size_t measure_iterations = 0, response_iterations = 0;
size_t SumResponseSize = 0;



void SetNewFreq(float new_freq) {
  if(new_freq > MAX_MEASUREMENT_FREQ) {
    Serial.println(F("out of range freq"));
    new_freq = MAX_MEASUREMENT_FREQ;
  }
  SAMPLING_FREQ = new_freq;
  TIMER_INTERVAL_MICRO = 1000.0*1000.0/SAMPLING_FREQ;
}

void StartMeasurement() {
  if( accelgyro.testConnection() ) {
    Serial.println(F("MPU6050 connection successful"));
  }
  else {
    Serial.println(F("MPU6050 connection failed"));
    return;
  }
  Serial.print(F("Start measure with "));
  Serial.print(TIMER_INTERVAL_MICRO);
  Serial.println(F("mks interval"));
  
  SensorLive = true;
  MeasureStartTime = micros();
  MesInterruptStartTime = micros();
  
  AttachTimerInterrupt(TIMER_INTERVAL_MICRO, GetSensorMeasurements);
}

void StopMeasurement() {
  Serial.println(F("Stop measure"));
  
  TimerInterruptDisable();
  
  MeasureFinTime = micros();
  SumWorkTimeMs += (MeasureFinTime - MeasureStartTime)/1000.0;
  SensorLive = false;
  ax = ay = az = 0;
  
  for(uint16_t i=0; i<FFT_ITERATION; i++) {
    Serial.println(MPU_measurements.a_inG_arr[i]);
  }
  Serial.println();
}



void Allert (const String text = "") {
  digitalWrite(LED_BUILTIN, LOW);
  allertText += text;
}



void GetSensorMeasurements() {
  noInterrupts();
  
  SumInterruptDiffTime += micros() - MesInterruptStartTime;
  MesInterruptStartTime = micros();
  
  accelgyro.getAcceleration(&ax, &ay, &az);
  if(MeasureWithTemperature) {
    digitalWrite(LED_BUILTIN, HIGH);
    temperature = accelgyro.getTemperature();
    if(temperature > TemreratureComparision) {
      Allert(F("Temperature ")+String(temperature)+(";"));
    }
  }
  
  const auto mesGet = micros();
  
  const float accel = accelToG<float>(getRMS<float>(ax, ay, az), ACCEL_RANGE)*9.8;
  MPU_measurements.add(accel);
//  MPU_measurements.add(ax,ay,az);
  
  if (REQ_FORMAT == 1) {
    MeasurementsForResponse += (ax & 0xFF) + ax >> 8;
  }
  else if(REQ_FORMAT == 2) {
    MeasurementsForResponse += (ax & 0xFF) + ax >> 8;
    MeasurementsForResponse += (temperature & 0xFF) + temperature >> 8;
  }
  else if(REQ_FORMAT == 3) {
    MeasurementsForResponse += (ax & 0xFF) + ax >> 8;
    MeasurementsForResponse += (ay & 0xFF) + az >> 8;
    MeasurementsForResponse += (az & 0xFF) + ay >> 8;
  }
  else if(REQ_FORMAT == 4) {
    MeasurementsForResponse += (ax & 0xFF) + ax >> 8;
    MeasurementsForResponse += (ay & 0xFF) + az >> 8;
    MeasurementsForResponse += (az & 0xFF) + ay >> 8;
    MeasurementsForResponse += (temperature & 0xFF) + temperature >> 8;
  }
  else if(REQ_FORMAT == 5) {
    MeasurementsForResponse += String((accel));
  }
  else if(REQ_FORMAT == 6) {
    MeasurementsForResponse += String((accel));
    MeasurementsForResponse += ','+ String(temperature);
  }
  else if(REQ_FORMAT == 7) {
    MeasurementsForResponse += String(ax) +','+ String(ay) +','+ String(az);
  }
  else if(REQ_FORMAT == 8) {
    MeasurementsForResponse += String(ax) +','+ String(ay) +','+ String(az);
    MeasurementsForResponse += ','+ String(temperature);
  }
  
  if(MeasurementsForResponse.length() > MAX_SINGLE_RESPONSE_SIZE) {
  //    StopMeasurement();
    MeasurementsForResponse = REQ_FORMAT ? CSV_ACCEL_HEADER : "";
    Serial.println(F("Response cleared"));
  }
  
  const auto mesFin = micros();
  
  SumGetMeasureTime += (mesGet - MesInterruptStartTime);
  SumWriteMeasureTime += (mesFin - mesGet);
  ++measure_iterations;
  
  interrupts();
}



void VibroMonitor(AccelForVibrationDataChunk& measurements) {
  digitalWrite(LED_BUILTIN, HIGH);
  if(FFT_TYPE != 0) {
    Serial.println(F("in FFT"));
    const auto fftStartTime = micros();
    
    float ARR[FFT_ITERATION];
    noInterrupts();
    if(FFT_TYPE == 1) {
      for(uint16_t i=0; i<FFT_ITERATION; i++) {
        ARR[i] = measurements.a_inG_arr[i];
      }
    }
    else if (FFT_TYPE == 2) {
      for(uint16_t i=0; i<FFT_ITERATION; i++) {
        ARR[i] = measurements.v_inMMpS_arr[i];
      }
    }
    interrupts();
    
    float vImag[FFT_ITERATION] = {0};
//    FFT.DCRemoval(ARR, FFT_ITERATION);
    FFT.Windowing(ARR, FFT_ITERATION, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.Compute(ARR, vImag, FFT_ITERATION, FFT_FORWARD);
    FFT.ComplexToMagnitude(ARR, vImag, FFT_ITERATION);
    
    const auto fftFinTime = micros();
    
    SumFFTTime += fftFinTime - fftStartTime;
    ++fft_iteration;

    Serial.println(FFT.MajorPeak(ARR, FFT_ITERATION, SAMPLING_FREQ), 3);
    
  //  Serial.print(F("SumFFTTime: "));
  //  Serial.print(SumFFTTime/fft_iteration);
  //  Serial.println(F("mks;"));
  //  Serial.flush();
    
    measurements.wasFFT = true;

    float v_rms = 0;
    FFTResultsForResponse = CSV_FFT_HEADER;
    for(uint16_t i=0; i<FFT_ITERATION; i++) {
      const float AccelAfterFFT = ARR[i];
      if(SEND_FFT_RES != 0) {
        FFTResultsForResponse += String(AccelAfterFFT)+';';
      }
      
      if(FFT_TYPE == 2) {
        const float freq_i = (i*1.0*SAMPLING_FREQ)/FFT_ITERATION;
        v_rms += sq(ARR[i]/freq_i);
      }
      
      if(FFT_TYPE == 1 && AccelAfterFFT > VibrationComparision[i]) {
        /*
        const String allert_text = F("Alert in ") 
                                  + String((i*1.0*SAMPLING_FREQ)/FFT_ITERATION) 
                                  + F(" Hz with accel ") 
                                  + String(AccelAfterFFT) 
                                  + F(" value;");
        Allert(allert_text);
        */
        Allert();
      }
    }
    
    if(FFT_TYPE == 2) {
      v_rms = sqrt(v_rms);
      if(v_rms > VelocityFromFFTComparision) {
        /*
        const String allert_text = F("Alert in ") 
                                  + String((i*1.0*SAMPLING_FREQ)/FFT_ITERATION) 
                                  + F(" Hz with velocity ") 
                                  + String(AccelAfterFFT) 
                                  + F(" value;");
        Allert(allert_text);
        */
        Allert();
      }
    }
  }
  
  if(WITH_ACCEL_RMS) {
    const auto accel_rms = measurements.getAccelRMS();
    if(accel_rms > AccelRMSComparision) {
      /*
      const String allert_text = F("Alert because accel RMS ") 
                                + String(accel_rms) 
                                + F(" value;");
      Allert(allert_text);
      */
      Allert();
    }
  }
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
      Serial.println(F("~Speed Stats~"));
      unsigned long WorkTime = SumWorkTimeMs;
      if (SensorLive) {
        WorkTime += (micros()-MeasureStartTime)/1000.0;
      }
      
      if(measure_iterations) {
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
        
        if(fft_iteration) {
          Serial.print(F("FFTTime: "));
          Serial.print(SumFFTTime/fft_iteration*3.0);
          Serial.println(F("µs;"));
        }
      }
      
      if(response_iterations) {
        Serial.print(F("ResponseTime: "));
        Serial.print(SumResponseTime/response_iterations);
        Serial.println(F("µs;"));
        
        Serial.print(F("ResponseSize: "));
        Serial.print(SumResponseSize/response_iterations);
        Serial.println(F("b;"));
      }
      
      Serial.println();
    }
    else if (last_console_text == "clear") {
      MeasurementsForResponse = CSV_ACCEL_HEADER;
      FFTResultsForResponse = CSV_FFT_HEADER;
      
      MPU_measurements.clear();

      SumFFTTime = 0;
      fft_iteration = 0;
      
      MeasureStartTime = MeasureFinTime = MesInterruptStartTime = 0;
      SumWorkTimeMs = SumResponseTime = 0;
      SumGetMeasureTime = SumWriteMeasureTime = SumInterruptDiffTime = 0;
      measure_iterations = response_iterations = 0;
      SumResponseSize = 0;
      
      Serial.println(F("Cleared"));
    }
    else if (last_console_text == "-r") {
      Serial.println(F("Restart"));
      ESP.restart();
    }
    else if (last_console_text == "start") {
      Serial.println(F("Measurement started"));
      StartMeasurement();
    }
    else if (last_console_text == "stop") {
      Serial.println(F("Measurement stopped"));
      StopMeasurement();
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
  Wire.setClock(500000);
  accelgyro.initialize();
  
  if( accelgyro.testConnection() ) {
    Serial.println(F("MPU6050 connection successful"));
//    readyBlinking();
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
  
  Serial.print(F("getRate:"));
  Serial.print(accelgyro.getRate());
//  accelgyro.setRate(0);  //4 7
  Serial.print(',');
  Serial.println(accelgyro.getRate());
  
  Serial.print(F("getAccelScale:"));
  Serial.print(accelgyro.getFullScaleAccelRange());
//  accelgyro.setFullScaleAccelRange(ACCEL_RANGE);  //0
  Serial.print(',');
  Serial.println(accelgyro.getFullScaleAccelRange());

  /*
    const auto accelRange = accelgyro.getFullScaleAccelRange();
    const float ACCEL_TO_G_DIVIDER = accelRange == 0 ? 16384.0 :
                                      accelRange == 1 ? 8192.0 :
                                      accelRange == 2 ? 4096.0 : 2048.0;
  */
  Serial.print(F("CalibrateAccel:"));
  accelgyro.CalibrateAccel();
  Serial.println(F("Fin"));
  
  accelgyro.PrintActiveOffsets();
  
  //GET SOME RATING TO COMPARE VIBRATION AFTER FFT
  for(uint16_t i=0; i<FFT_ITERATION; i++) {
    VibrationComparision[i] = 2.0;
  }
  
  
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
      led_state = LOW;
      digitalWrite(LED_BUILTIN, led_state);
      Serial.println(F("Led go ON"));
    }
    else if (server.arg("Led") == "OFF") {
      led_state = HIGH;
      digitalWrite(LED_BUILTIN, led_state);
      Serial.println(F("Led go OFF"));
    }
    
    if (server.arg("Msrmnt") == "ON") {
      Serial.println(F("Msrmnt start"));
      StartMeasurement();
    }
    else if (server.arg("Msrmnt") == "OFF") {
      StopMeasurement();
      Serial.println(F("Msrmnt stop"));
    }
    
    if (server.arg("TempMeasurement") == "ON") {
      accelgyro.setTempSensorEnabled(0);
      Serial.print(F("Temperature measurement on: "));
      Serial.println(accelgyro.getTempSensorEnabled());
      MeasureWithTemperature = true;
    }
    else if (server.arg("TempMeasurement") == "OFF") {
      MeasureWithTemperature = false;
      accelgyro.setTempSensorEnabled(1);
      Serial.print(F("Temperature measurement off:"));
      Serial.println(accelgyro.getTempSensorEnabled());
    }
    
    if (server.arg("ReqFormat") == "0") {
      REQ_FORMAT = 0;
      MeasurementsForResponse = "";
      Serial.println(F("no send"));
    }
    else if (server.arg("ReqFormat") == "blob1") {
      REQ_FORMAT = 1;
      MeasurementsForResponse = "";
      Serial.println(F("blob vector accel format"));
    }
    else if (server.arg("ReqFormat") == "blob2") {
      REQ_FORMAT = 2;
      MeasurementsForResponse = "";
      Serial.println(F("blob vector accel format"));
    }
    else if (server.arg("ReqFormat") == "blob3") {
      REQ_FORMAT = 3;
      MeasurementsForResponse = "";
      Serial.println(F("csv 3 accel format"));
    }
    else if (server.arg("ReqFormat") == "blob4") {
      REQ_FORMAT = 4;
      MeasurementsForResponse = "";
      Serial.println(F("csv 3 accel format"));
    }
    else if (server.arg("ReqFormat") == "csv1") {
      REQ_FORMAT = 5;
      MeasurementsForResponse = "";
      Serial.println(F("csv vector accel format"));
    }
    else if (server.arg("ReqFormat") == "csv2") {
      REQ_FORMAT = 6;
      MeasurementsForResponse = "";
      Serial.println(F("csv vector accel format"));
    }
    else if (server.arg("ReqFormat") == "csv3") {
      REQ_FORMAT = 7;
      MeasurementsForResponse = "";
      Serial.println(F("csv vector accel format"));
    }
    else if (server.arg("ReqFormat") == "csv4") {
      REQ_FORMAT = 8;
      MeasurementsForResponse = "";
      Serial.println(F("csv 3 accel format"));
    }
    
    if (server.arg("WithFFT") == "OFF") {
      FFT_TYPE = 0;
      Serial.println(F("FFT off"));
    }
    else if (server.arg("WithFFT") == "ACCEL") {
      FFT_TYPE = 1;
      Serial.println(F("FFT on accel"));
    }
    else if (server.arg("WithFFT") == "VEL") {
      FFT_TYPE = 2;
      Serial.println(F("FFT on velocity"));
    }
    
    if (server.arg("AccelRMS") == "ON") {
      WITH_ACCEL_RMS = true;
      Serial.println(F("Accel RMS on"));
    }
    else if (server.arg("AccelRMS") == "OFF") {
      WITH_ACCEL_RMS = false;
      Serial.println(F("Accel RMS off"));
    }

    if (server.arg("SendFFTRes") == "ON") {
      SEND_FFT_RES = true;
      Serial.println(F("Send FFT on"));
    }
    else if (server.arg("SendFFTRes") == "OFF") {
      SEND_FFT_RES = false;
      Serial.println(F("Send FFT off"));
    }

    
    if (server.hasArg("NewAccelRange")) {
      const String new_range = server.arg("NewAccelRange");
      ACCEL_RANGE = new_range == "16g" ? 3 :
                    new_range == "8g" ? 2 :
                    new_range == "4g" ? 1 : 0 ;
      accelgyro.setFullScaleAccelRange(ACCEL_RANGE);
      Serial.println(F("New accel range"));
      Serial.println(accelgyro.getFullScaleAccelRange());
    }
    if (server.hasArg("NewMeasurementFreq")) {
      SetNewFreq(server.arg("NewMeasurementFreq").toFloat());
      Serial.println(F("New measurement freq"));
      Serial.println(SAMPLING_FREQ);
    }
    if (server.hasArg("NewAccelRMS")) {
      AccelRMSComparision = server.arg("NewAccelRMS").toFloat();
      Serial.println(F("New accel RMS"));
      Serial.println(AccelRMSComparision);
    }
    if (server.hasArg("NewVeloRMS")) {
      VelocityRMSComparision = server.arg("NewVeloRMS").toFloat();
      Serial.println(F("New velocity RMS"));
      Serial.println(VelocityRMSComparision);
    }
    if (server.hasArg("NewTempLimit")) {
      TemreratureComparision = server.arg("NewLimits").toFloat();
      Serial.println(F("New temperature limits"));
      Serial.println(TemreratureComparision);
    }
    if (server.hasArg("NewIntervalCount")) {
      const float new_interval_count = server.arg("NewIntervalCount").toInt();
//      FFT_ITERATION = new_interval_count;
      Serial.println(F("New interval count NOT WORKING NOW"));
      Serial.println(FFT_ITERATION);
    }
    if (server.hasArg("NewFreqLimits")) {
      Serial.println(F("New freq limits"));
      const String new_freq_values = server.arg("NewFreqLimits");
      const uint16_t last_index = new_freq_values.length();
      for(uint16_t curr_pos = 0, prev_pos = 0, freq_it = 0; 
          curr_pos <= last_index && freq_it < FFT_ITERATION; 
          curr_pos++) 
      {
        if(new_freq_values[curr_pos]==',' || curr_pos==last_index) {
          Serial.print(curr_pos);
          Serial.print(',');
          Serial.print(freq_it);
          Serial.print(':');
          
          const float new_limit_tmp = new_freq_values.substring(prev_pos, curr_pos).toFloat();
          VibrationComparision[freq_it++] = new_limit_tmp;
          prev_pos = curr_pos+1;
          
          Serial.print(new_limit_tmp, 3);
          Serial.print(',');
          Serial.println(VibrationComparision[freq_it-1], 3);
        }
      }
    }
    
    Serial.flush();
    Serial.println();
    
    server.send(204);
  });
  
  server.on("/req/get", HTTP_GET, [](){
    String msg = "Number of args received:";
    msg += server.args();      // получить количество параметров
    msg += "\n";               // переход на новую строку
    
    for (int i = 0; i < server.args(); i++) {
      msg += "Arg #" + (String)i + " –> "; // добавить текущее значение счетчика
      msg += server.argName(i) + ": ";      // получить имя параметра
      msg += server.arg(i) + "\n";          // получить значение параметра
    }
    
    Serial.print(msg);
    
    if (server.hasArg("MeasurementFreq")) {
      server.send(200, "text/plain", String(SAMPLING_FREQ));
    }
    if (server.hasArg("FFTintervals")) {
      server.send(200, "text/plain", String(FFT_ITERATION));
    }
    if (server.hasArg("FFTlimits")) {
      String payload;
      for(uint16_t i=0; i<FFT_ITERATION; i++) {
        payload += String(VibrationComparision[i])+',';
      }
      server.send(200, "text/plain", payload);
    }
    if (server.hasArg("AccelRange")) {
      const String payload = ACCEL_RANGE == 0 ? "+-2g" :
                              ACCEL_RANGE == 1 ? "+-4g" :
                              ACCEL_RANGE == 2 ? "+-8g" : "+-16g";
      server.send(200, "text/plain", payload);
    }
    if (server.hasArg("GetAlerts")) {
      server.send(200, "text/plain", allertText);
      allertText = "";
    }
  });
  
  
  MeasurementsForResponse.reserve(MAX_RESPONSE_SIZE);
  server.on("/msrmnt", HTTP_GET, [](){
//    MeasurementsForResponse = CONST_STRING;//"1,1,1;";//CONST_STRING;
    SumResponseSize += MeasurementsForResponse.length();
    
    auto SendingStartTime = micros();
    if(REQ_FORMAT > 4) {
      server.send(200, "text/csv", MeasurementsForResponse);
    }
    else {
      server.send(200, "arraybuffer/blob", MeasurementsForResponse);
    }
    
    MeasurementsForResponse = "";
    auto SendingFinTime = micros();
    
    SumResponseTime += SendingFinTime-SendingStartTime;
    ++response_iterations;
  });
  
  FFTResultsForResponse.reserve(MAX_RESPONSE_SIZE);
  server.on("/fft", HTTP_GET, [](){
//    SumResponseSize += FFTResultsForResponse.length();
//    
//    auto SendingStartTime = micros();
    server.send(200, "text/csv", FFTResultsForResponse);  // arraybuffer/blob text/plain  text/csv
    MeasurementsForResponse = "";
//    auto SendingFinTime = micros();
//    
//    SumResponseTime += SendingFinTime-SendingStartTime;
//    ++response_iterations;
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

//  if(((MPU_measurements.size()+1)%(FFT_ITERATION/4)==0) 
  if(MPU_measurements.size()==FFT_ITERATION-1 
      && MPU_measurements.full 
      && !MPU_measurements.wasFFT 
      && SensorLive) {
//    Serial.print(F("InFFTcheck with "));
//    Serial.println(MPU_measurements.size());
    VibroMonitor(MPU_measurements);
  }
}

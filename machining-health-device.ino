
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include "I2Cdev.h"
#include "MPU6050.h"

//#include <time.h>
#include <String>

#include "help.h"
#include "sensor_part.h"
//#include "fft.h"


#include "arduinoFFT.h"
arduinoFFT FFT = arduinoFFT();


#define LED_BUILTIN 2

bool led_state = true;  //OFF by default
bool SensorLive = false;
bool MeasureToTransmitWireless = true;


const char* ssid = "yourNetwork";
const char* password = "secretPassword";

ESP8266WebServer server(80);


const uint8_t TEMP_SENSOR_COUNT = 1;
const uint8_t ACCEL_SENSOR_COUNT = 1;


MPU6050 accelgyro;
int16_t ax, ay, az;


const uint8_t FFT_ITERATION = 64; //4 8 16 32 64 128 256 512 1024

struct AccelForVibrationDataChunk {
  double ax_arr[FFT_ITERATION];
//  double ax_sum;
  double ay_arr[FFT_ITERATION];
//  double ay_sum;
  double az_arr[FFT_ITERATION];
//  double az_sum;
  
  size_t current_size = 0;
  
  bool wasFFT = false;
  bool full = false;
  
  void add(const int16_t ax, const int16_t ay, const int16_t az) {
//    ax_sum -= ax_arr[current_size];
    ax_arr[current_size] = accelToG_double(ax);
//    ax_sum += ax_arr[current_size];
    
//    ay_sum -= ay_arr[current_size];
    ay_arr[current_size] = accelToG_double(ay);
//    ay_sum += ay_arr[current_size];
    
//    az_sum -= az_arr[current_size];
    az_arr[current_size] = accelToG_double(az);
//    az_sum += az_arr[current_size];
    
    ++current_size;
    wasFFT = false;
    
    if(current_size==FFT_ITERATION) {
//      Serial.println(F("InSizeCheck"));
      full = true;
      current_size = 0;
    }
  }

//  double getMean() const {
//    return (ax_sum + ay_sum + az_sum)/FFT_ITERATION;
//  }

  double getRMS() const { //root mean squere value
    double sum_ax = 0, sum_ay = 0, sum_az = 0;
    for(uint8_t i=0; i<FFT_ITERATION; i++) {
      sum_ax += sq(ax_arr[i]);
      sum_ay += sq(ay_arr[i]);
      sum_az += sq(az_arr[i]);
    }
    sum_ax = sqrt(sum_ax/FFT_ITERATION);
    sum_ay = sqrt(sum_ay/FFT_ITERATION);
    sum_az = sqrt(sum_az/FFT_ITERATION);
    return sum_ax + sum_ay + sum_az;
  }
  
  void clear() {
    current_size = 0;
    full = false;
  }

  size_t size() const {
    return current_size;
  }
};


AccelForVibrationDataChunk MPU_measurements;
double VibrationComparision[FFT_ITERATION];
double TemreratureComparision = 44;

unsigned long SumFFTTime = 0;
size_t fft_iteration = 0;



#define MAX_RESPONSE_SIZE 65536 //1048576 65536
#define MAX_SINGLE_RESPONSE_SIZE 65472 //MAX_RESPONSE_SIZE-64
#define CSV_ACCEL_HEADER "" //"time,ax,ay,az;"
#define CSV_FFT_HEADER "" //"fft result for 64 intervals";

String MeasurementsForResponse = CSV_ACCEL_HEADER;
String FFTResultsForResponse = CSV_FFT_HEADER;


//#define TIMER_INTERVAL_MICRO 1001
#define FFT_MULTIPLIER 2.56 //2
double FREQ_TO_MEASURE = 200; //10.666667
double MEASUREMENT_FREQ = FREQ_TO_MEASURE * FFT_MULTIPLIER;
double TIMER_INTERVAL_MICRO = 1000.0*1000.0/MEASUREMENT_FREQ;


unsigned long MeasureStartTime, MeasureFinTime, MesInterruptStartTime;
unsigned long SumWorkTimeMs = 0, SumResponseTime = 0;
unsigned long SumWriteMeasureTime = 0, SumGetMeasureTime = 0, SumInterruptDiffTime = 0;
size_t measure_iterations = 0, response_iterations = 0;
size_t SumResponseSize = 0;



void SetNewFreq(const double NEW_FREQ) {
  FREQ_TO_MEASURE = NEW_FREQ;
  MEASUREMENT_FREQ = FREQ_TO_MEASURE * FFT_MULTIPLIER;
  TIMER_INTERVAL_MICRO = 1000.0*1000.0/MEASUREMENT_FREQ;
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
  
  AttachTimerInterrupt(TIMER_INTERVAL_MICRO, SensorMeasurements);
}

void StopMeasurement() {
  Serial.println(F("Stop measure"));
  
  TimerInterruptDisable();
  
  MeasureFinTime = micros();
  SumWorkTimeMs += (MeasureFinTime - MeasureStartTime)/1000.0;
  SensorLive = false;
  ax = ay = az = 0;
  
  for(uint8_t i=0; i<FFT_ITERATION; i++) {
    Serial.print(MPU_measurements.ax_arr[i]);
    Serial.print('\t');
    Serial.print(MPU_measurements.ay_arr[i]);
    Serial.print('\t');
    Serial.print(MPU_measurements.az_arr[i]);
    Serial.println();
  }
  Serial.println();
}


void SensorMeasurements() {
  noInterrupts();
  
  SumInterruptDiffTime += micros() - MesInterruptStartTime;
  MesInterruptStartTime = micros();
  
  accelgyro.getAcceleration(&ax, &ay, &az);
  const auto mesGet = micros();
  MPU_measurements.add(ax, ay, az);

  if (MeasureToTransmitWireless) {
    // accelToG CONVERTATION accelToG_double(ax)
    MeasurementsForResponse += String(ax) +','  
                            + String(ay) +','
                            + String(az) +';';
                            //String(time(nullptr)) +','+
    if(MeasurementsForResponse.length() > MAX_SINGLE_RESPONSE_SIZE) {
  //    StopMeasurement();
      MeasurementsForResponse = "";
      Serial.println(F("Too Long Response String so cleared"));
    }
  }
  
  const auto mesFin = micros();
  
  SumGetMeasureTime += (mesGet - MesInterruptStartTime);
  SumWriteMeasureTime += (mesFin - mesGet);
  ++measure_iterations;
  
  interrupts();
}



void makeFFT(double ARR[FFT_ITERATION]) {
  const auto fftStartTime = micros();
//  const float fastFFT = Q_FFT(fftArrX, FFT_ITERATION, MEASUREMENT_FREQ);
//  const auto fftFinTime = micros();
//  SumFFTTime += fftFinTime - fftStartTime;
//  ++fft_iteration;
//  Serial.println(fastFFT);
//  return;
  
  double vImag[FFT_ITERATION] = {0};
  
  FFT.DCRemoval(ARR, FFT_ITERATION);
  
  const auto windowingStartTime = micros();
  FFT.Windowing(ARR, FFT_ITERATION, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  
  const auto computingStartTime = micros();
  FFT.Compute(ARR, vImag, FFT_ITERATION, FFT_FORWARD);
  
  const auto toMagnitudeStartTime = micros();
  FFT.ComplexToMagnitude(ARR, vImag, FFT_ITERATION);
  
  const auto fftFinTime = micros();
  
//  for(uint8_t i=0; i<FFT_ITERATION; i++) {
//    Serial.print((i*1.0*MEASUREMENT_FREQ)/FFT_ITERATION);
//    Serial.print(F(" Hz \t"));
//    Serial.println(ARR[i]);
//  }
  
//  const auto majorPeakStartTime = micros();
//  double x = FFT.MajorPeak(ARR, samples, MEASUREMENT_FREQ);
  
//  const auto fftFinTime = micros();
  SumFFTTime += fftFinTime - fftStartTime;
  ++fft_iteration;
  
//  Serial.print(windowingStartTime - mesGet);
//  Serial.print('\t');
//  Serial.print(computingStartTime - windowingStartTime);
//  Serial.print('\t');
//  Serial.print(toMagnitudeStartTime - computingStartTime);
//  Serial.print('\t');
//  Serial.print(majorPeakStartTime - toMagnitudeStartTime);
//  Serial.print('\t');
//  Serial.print(fftFinTime-majorPeakStartTime);
//  Serial.println(" mks");
  
//  Serial.print(F("SumFFTTime: "));
//  Serial.print(SumFFTTime/fft_iteration);
//  Serial.println(F("mks;"));
//  Serial.flush();
}


void VibroMonitor(AccelForVibrationDataChunk& AccelARR) {
  noInterrupts();
//  StopMeasurement();  //!
//  const double arrCopyStartTime = micros();

  /*
  double fftArrX[FFT_ITERATION], fftArrY[FFT_ITERATION], fftArrZ[FFT_ITERATION];
  for(uint8_t i=0; i<FFT_ITERATION; i++) {
    fftArrX[i] = AccelARR.ax_arr[i];
    fftArrY[i] = AccelARR.ay_arr[i];
    fftArrZ[i] = AccelARR.az_arr[i];
  }
  */
  double fftArr[FFT_ITERATION];
  for(uint8_t i=0; i<FFT_ITERATION; i++) {
    fftArr[i] = sqrt(sq(AccelARR.ax_arr[i]) + sq(AccelARR.ay_arr[i]) + sq(AccelARR.az_arr[i]));
  }
  
//  const double arrCopyFinTime = micros();
  
  interrupts();
  
//  Serial.print(F("arrCopyTime: "));
//  Serial.print(arrCopyFinTime - arrCopyStartTime);
//  Serial.println(F("mks;"));
//  
//  makeFFT(fftArrX);
//  makeFFT(fftArrY);
//  makeFFT(fftArrZ);
  makeFFT(fftArr);

  AccelARR.wasFFT = true;
  
  FFTResultsForResponse = CSV_FFT_HEADER;
  for(uint8_t i=0; i<FFT_ITERATION; i++) {
//    const double AccelAfterFFT = fftArrX[i] + fftArrY[i] + fftArrZ[i];  // ()/3 ?
    const double AccelAfterFFT = fftArr[i];
    if(MeasureToTransmitWireless) {
      FFTResultsForResponse += String(AccelAfterFFT) +',';
    }
    
    if(AccelAfterFFT > VibrationComparision[i]) {
      Serial.print(F("Alert in "));
      Serial.print((i*1.0*MEASUREMENT_FREQ)/FFT_ITERATION);
      Serial.print(F(" Hz because "));
      Serial.print(AccelAfterFFT);
      Serial.println(F(" value"));
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
  Wire.setClock(400000);
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
//  accelgyro.setRate(0);  //4
  Serial.print(',');
  Serial.println(accelgyro.getRate());
  
  Serial.print(F("getAccelScale:"));
  Serial.print(accelgyro.getFullScaleAccelRange());
//  accelgyro.setFullScaleAccelRange(1);  //0
  Serial.print(',');
  Serial.println(accelgyro.getFullScaleAccelRange());

  /*
    const auto accelRange = accelgyro.getFullScaleAccelRange();
    const double ACCEL_TO_G_DIVIDER = accelRange == 0 ? 16384.0 :
                                      accelRange == 1 ? 8192.0 :
                                      accelRange == 2 ? 4096.0 : 2048.0;
  */
  Serial.print(F("CalibrateAccel:"));
  accelgyro.CalibrateAccel();
  Serial.println(F("Fin"));
  
  accelgyro.PrintActiveOffsets();
  
  //GET SOME RATING TO COMPARE VIBRATION AFTER FFT
  for(uint8_t i; i<FFT_ITERATION; i++) {
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
      Serial.println(F("Led go ON"));
      led_state = LOW;
      digitalWrite(LED_BUILTIN, led_state);
    }
    else if (server.arg("Led") == "OFF") {
      Serial.println(F("Led go OFF"));
      led_state = HIGH;
      digitalWrite(LED_BUILTIN, led_state);
    }
    if (server.arg("Msrmnt") == "ON") {
      Serial.println(F("Msrmnt start"));
      StartMeasurement();
    }
    else if (server.arg("Msrmnt") == "OFF") {
      Serial.println(F("Msrmnt stop"));
      StopMeasurement();
    }
    if (server.arg("WirelessTransmit") == "ON") {
      Serial.println(F("WirelessTransmit on"));
      MeasureToTransmitWireless = true;
    }
    else if (server.arg("WirelessTransmit") == "OFF") {
      Serial.println(F("WirelessTransmit off"));
      MeasureToTransmitWireless = false;
    }
    if (server.hasArg("NewAccelRange")) {
      Serial.println(F("New accel range"));
      Serial.print(accelgyro.getFullScaleAccelRange());
      const String new_range = server.arg("NewMeasurementFreq");
      const uint8_t accelRange = new_range == "+-16g" ? 3 :
                                  new_range == "+-8g" ? 2 :
                                  new_range == "+-4g" ? 1 : 0 ;
      accelgyro.setFullScaleAccelRange(accelRange);
      Serial.print(',');
      Serial.println(accelgyro.getFullScaleAccelRange());
    }
    if (server.hasArg("NewMeasurementFreq")) {
      Serial.println(F("New measurement freq"));
      Serial.print(FREQ_TO_MEASURE);
      Serial.print(',');
      SetNewFreq(server.arg("NewMeasurementFreq").toFloat());
      Serial.println(FREQ_TO_MEASURE);
    }
    if (server.hasArg("NewTempLimit")) {
      Serial.println(F("New temperature limits"));
      Serial.print(TemreratureComparision);
      Serial.print(',');
      const double new_TemreratureComparision = server.arg("NewLimits").toFloat();
//      TemreratureComparision = new_TemreratureComparision;
      Serial.println(new_TemreratureComparision); //TemreratureComparision
    }
    if (server.hasArg("NewIntervalCount")) {
      Serial.println(F("New interval count"));
      Serial.print(FFT_ITERATION);
      Serial.print(',');
      const double new_interval_count = server.arg("NewIntervalCount").toInt();
//      FFT_ITERATION = new_interval_count;
      Serial.println(new_interval_count); //FFT_ITERATION
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
          
          const double new_limit_tmp = new_freq_values.substring(prev_pos, curr_pos).toFloat();
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
    
    if (server.hasArg("TempSensorsCount")) {
      server.send(200, "text/plain", String(TEMP_SENSOR_COUNT));
    }
    if (server.hasArg("AccelSensorsCount")) {
      server.send(200, "text/plain", String(ACCEL_SENSOR_COUNT));
    }
    if (server.hasArg("MeasurementFreq")) {
      server.send(200, "text/plain", String(FREQ_TO_MEASURE));
    }
    if (server.hasArg("FFTintervals")) {
      server.send(200, "text/plain", String(FFT_ITERATION));
    }
    if (server.hasArg("FFTlimits")) {
      String payload;
      for(uint8_t i=0; i<FFT_ITERATION; i++) {
        payload += String(VibrationComparision[i])+',';
      }
      server.send(200, "text/plain", payload);
    }
    if (server.hasArg("AccelRange")) {
      const auto accelRange = accelgyro.getFullScaleAccelRange();
      const String payload = accelRange == 0 ? "+-2g" :
                              accelRange == 1 ? "+-4g" :
                              accelRange == 2 ? "+-8g" : "+-16g";
      server.send(200, "text/plain", payload);
    }
//    if (server.hasArg("GetAlerts")) {
//      String Alert_str = "";
//      server.send(200, "text/plain", Alert_str);
//    }
  });
  
  
  MeasurementsForResponse.reserve(MAX_RESPONSE_SIZE);
  server.on("/msrmnt", HTTP_GET, [](){
//    MeasurementsForResponse = CONST_STRING;//"1,1,1;";//CONST_STRING;
    SumResponseSize += MeasurementsForResponse.length();
    
    auto SendingStartTime = micros();
    server.send(200, "text/csv", MeasurementsForResponse);  // arraybuffer/blob text/plain  text/csv
    MeasurementsForResponse = CSV_ACCEL_HEADER;
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
    MeasurementsForResponse = CSV_FFT_HEADER;
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

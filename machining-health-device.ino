
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include "I2Cdev.h"
#include "MPU6050.h"

#include <time.h>
#include <String>

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
#define CSV_HEADER "" //"time,ax,ay,az"
String MeasurementsForResponse = CSV_HEADER;
String MeasurementsForResponse2 = CSV_HEADER;

bool useFirstResponseString = true;


unsigned long MeasureStartTime, MeasureFinTime;
unsigned long SumWorkTimeMs = 0, SumWriteMeasureTime = 0, SumGetMeasureTime = 0, SumResponseTime = 0;
size_t measure_iterations = 0, response_iterations = 0;
size_t SumResponseSize = 0;


//#define TIMER_INTERVAL_MS 1
//ESP8266Timer ITimer;


const String preparedHtmlPage = 
  "<!DOCTYPE html>"\
    "<html>"\
      "<head>"\
        "<title>ESP8266 Demo</title>"\
        "<link rel=\"icon\" href=\"data:,\">"\
        "<style>"\
          "body {background-color:#2c2c34; font-family:Arial, Helvetica, Sans-Serif; Color:#519cd4; text-align:center}"\
        "</style>"\
      "</head>"\
      "<body>"\
        "<div id=\"ax\"></div>"\
        "<div id=\"ay\"></div>"\
        "<div id=\"az\"></div>"\
        "<script type=\"text/javascript\" src=\"https://www.gstatic.com/charts/loader.js\"></script>"\
        "<div id=\"chart_div\"></div>"\
        "<div>"\
          "<p>Measurements Control Panel</p>"\
          "<p>"\
            "<form method=\"post\" action=\"/req/post\">"\
              "<button name=\"Msrmnt\" value=\"ON\" onclick=\"online()\">Start Measurements</button>"\
              "<button name=\"Msrmnt\" value=\"OFF\" onclick=\"offline()\">Stop Measurements</button>"\
            "</form>"\
            "<script>"\
              "var live=false;"\
              "function online(){live=true;}"\
              "function offline(){live=false; forceDownloadDataFile();}"\
            "</script>"\
          "</p>"\
        "</div>"\
    "<script>"\
      "google.charts.load('current',{packages:['corechart','line']});"\
      "var queue=[];"\
      "var virtualFile=\"\";"\
      "var chart;"\
      "function forceDownloadDataFile(){"\
        "const a=document.createElement('a');"\
        "a.download='data.csv';"\
        "a.href=URL.createObjectURL(new Blob([virtualFile],{type:'text/plain'}));"\
        "a.click();"\
        "virtualFile=\"\";"\
      "}"\
      "setInterval(function(){"\
        "if(!live){return;}"\
        "getMeasurement();"\
        "drawAxisTickColors();"\
        "if (virtualFile.length>64*1024*1024) {"\
          "forceDownloadDataFile();"\
        "}"\
      "},100);"\
      "function drawAxisTickColors(){"\
        "if(chart){chart.clearChart();}"\
        "var data=new google.visualization.DataTable();"\
        "data.addColumn('number','X');"\
        "data.addColumn('number','ax');"\
        "data.addColumn('number','ay');"\
        "data.addColumn('number','az');"\
        "for(const [key, value] of queue.entries()){"\
          "data.addRow([key,value[0],value[1],value[2]]);"\
        "}"\
        "var options={"\
          "hAxis:{title:'Time'},"\
          "vAxis:{title:'Acceleration'},"\
          "colors:['#a52714','#14a527','#2714a5']"\
        "};"\
        "chart=new google.visualization.LineChart(document.getElementById('chart_div'));"\
        "chart.draw(data,options);"\
      "};"\
      "function getMeasurement(){"\
        "var xhttp=new XMLHttpRequest();"\
        "xhttp.open(\"GET\",\"/msrmnt\",true);"\
        "xhttp.send();"\
        "xhttp.onload=function(){"\
          "if(this.status==200) {"\
            "virtualFile+=this.response+'\\n';"\
            "const arr=this.response.split(';');"\
            "for(const line of arr){"\
              "if(!line)continue;"\
              "queue.push(line.split(',').map(Number));"\
            "}"\
            "while(queue.length>1000){"\
              "queue.shift();"\
            "}"\
          "}"\
          "else{"\
            "console.log('keks');"\
          "}"\
        "};"\
      "}"\
    "</script>"\
    "</body>"\
  "</html>";



void readyBlinking() {
  digitalWrite(LED_BUILTIN, LOW);   delay(250);
  digitalWrite(LED_BUILTIN, HIGH);  delay(250);
  digitalWrite(LED_BUILTIN, LOW);   delay(250);
  digitalWrite(LED_BUILTIN, HIGH);  delay(250);
  digitalWrite(LED_BUILTIN, LOW);   delay(250);
  digitalWrite(LED_BUILTIN, HIGH);  delay(250);
}



void StartMeasurement() {
  SensorLive = true;
  MeasureStartTime = micros();
  Serial.println(F("Start measure"));
}

void StopMeasurement() {
  SensorLive = false;
  MeasureFinTime = micros();
  SumWorkTimeMs += (MeasureFinTime - MeasureStartTime)/1000.0;
  ax = ay = az = 0;
  Serial.println(F("Stop measure"));
}


const String toString(int16_t i) {
  if(i < 0) {
    return '-'+String(abs(i));
  }
  return String(abs(i));
}


void SensorMeasurements() {
  const auto mesStart = micros();
  accelgyro.getAcceleration(&ax, &ay, &az);
  const auto mesGet = micros();
  MeasurementsForResponse += String(ax) +','+ String(ay) +','+ String(az) +';'; //String(time(nullptr)) +','+
  const auto mesFin = micros();
  
  SumGetMeasureTime += (mesGet - mesStart);
  SumWriteMeasureTime += (mesFin - mesGet);
  ++measure_iterations;
  
//  if(MeasurementsForResponse.length() > MAX_SINGLE_RESPONSE_SIZE) {
//    StopMeasurement();
//  }
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
  accelgyro.initialize();
  
//  accelgyro.setI2CMasterModeEnabled(false);
//  accelgyro.setI2CBypassEnabled(true) ;
//  accelgyro.setSleepEnabled(false);
  
  if( accelgyro.testConnection() ) {
    Serial.println(F("MPU6050 connection successful"));
    readyBlinking();
  }
  else {
    Serial.println(F("MPU6050 connection failed"));
    delay(2500);
    return;
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
  
//  timer1_attachInterrupt(TimerISR);
//  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE); //3: TIM_LOOP
//  timer1_write(5000);
  
//  Serial.println(ESP8266_TIMER_INTERRUPT_VERSION);
//  if (ITimer.attachInterruptInterval(TIMER_INTERVAL_MS * 1000, TimerHandler)) {
//    Serial.print(F("Starting  ITimer OK, millis() = ")); Serial.println(millis());
//  }
//  else {
//    Serial.println(F("Can't set ITimer. Select another freq. or timer"));
//  }
  
//  timer = timerBegin(0, 160000, true);
//  timerAttachInterrupt(timer, &onTimer, true);
//  timerAlarmWrite(timer, 1000, true);
//  timerAlarmEnable(timer);
}



void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("*** Disconnected from AP so rebooting ***"));
    ESP.reset();
  }
  
  ParseConsole();
  server.handleClient();
  
  if (SensorLive) {
    SensorMeasurements();
  }
}

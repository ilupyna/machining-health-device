
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>

#include "I2Cdev.h"
#include "MPU6050.h"

#include <time.h>
#include <String>

#define LED_BUILTIN 2
#define SEND_DATA_EVERY 100
#define DATA_SIZE_MAX 400


const char* ssid = "yourNetwork";
const char* password = "secretPassword";

bool led_state = true;  //set off by default
bool SensorLive = false;

ESP8266WebServer server(80);

MPU6050 accelgyro;
int16_t ax, ay, az;

struct dataObj {
//  time_t timeStomp;
  int16_t ax, ay, az;
};

dataObj database[DATA_SIZE_MAX];

//#define RESPONSE_SIZE 1024
//#define MAX_SINGLE_RESPONSE_SIZE 1006 //1024-18
String MeasurementsForResponse;


//File currentFile;

unsigned long Start, Finish, WorkTime = 0;
unsigned long MeanResponseTime = 1, MeanMeasureTime = 1;
size_t iterations = 0;
uint8_t measurement_pos = 0;


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
      
        "<div><p>Data: "\
          "<span id=\"ax\">-/-</span>"\
          "<span id=\"ay\">-/-</span>"\
          "<span id=\"az\">-/-</span>"\
        "</p></div>"\
      /**/
        "<div><p>"\
          "<form method=\"post\" action=\"/req/post\">"\
            "<p>Buttons on POST form</p>"\
            "<button name=\"Led\" value=\"ON\">Turn On</button>"\
            "<button name=\"Led\" value=\"OFF\">Turn Off</button>"\
          "</form>"\
        "</p></div>"\
        "<div>"\
          "<p>Measurements Control Panel</p>"\
          "<p>"\
            "<form method=\"post\" action=\"/req/post\">"\
              "<button name=\"Msrmnt\" value=\"ON\">Start Measurements</button>"\
              "<button name=\"Msrmnt\" value=\"OFF\">Stop Measurements</button>"\
            "</form>"\
          "</p>"\
          "<p>"\
            "<form method=\"get\" action=\"/req/get\">"\
              "<button name=\"Download\" value=\"/SensorData.txt\">Download SensorData.txt button</button>"\
            "</form>"\
          "</p>"\
        "</div>"\
    
    "<script>"\
      "setInterval(function() {getMeasurement();}, 5000);"\
      
      "function getMeasurement() {"\
        "var xhttp = new XMLHttpRequest();"\
        "xhttp.onreadystatechange = function() {"\
          "if (this.readyState == 4 && this.status == 200) {"\
            "document.getElementById(\"post_here\").innerHTML += this.responseText + ';';"\
          "}"\
        "};"\
        "xhttp.open(\"GET\", \"/msrmnt\", true);"\
        "xhttp.send();"\
      "}"\
      
    "</script>"\
    /**/
    "</body>"\
  "</html>";


void SensorMeasurements() {
  const auto mesStart = micros();
  accelgyro.getAcceleration(&ax, &ay, &az);
  database[measurement_pos++] = {ax, ay, az};
//  MeasurementsForResponse += String(ax) +','+ String(ay) +','+ String(az) +';';
//  const auto mesTime = micros() - mesStart;
//  MeanMeasureTime = (MeanMeasureTime + mesTime)/2;
  
//  currentFile.println(time(nullptr));
//  currentFile.print(ax); currentFile.print(',');
//  currentFile.print(ay); currentFile.print(',');
//  currentFile.print(az); currentFile.println(';');
  
  ++iterations;
  if(measurement_pos > DATA_SIZE_MAX) {
//  if(MeasurementsForResponse.size() > 1024) {
    SensorLive = false;
    Finish = micros();
    WorkTime = Finish - Start;
    ax = ay = az = 0;
    Serial.println(F("Stop measure"));
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
      if (SensorLive) {
        WorkTime = micros()-Start;
      }
      Serial.print(F("WorkTime: "));
      Serial.print(WorkTime/1000.0);
      Serial.println(F("ms;"));
      Serial.print(F("V = "));
      Serial.print(1000.0*iterations/WorkTime);
      Serial.println(F("kHz;"));
      
      Serial.print(F("ResponseTime: "));
      Serial.print(MeanResponseTime);
      Serial.println(F("ms;"));

      /*
      FSInfo filesystem_info;
      LittleFS.info(filesystem_info);
      Serial.println(filesystem_info.totalBytes);
      Serial.println(filesystem_info.usedBytes);
      */
    }
    else if (last_console_text == "clear") {
      Start = micros();
      Finish = 0;
      WorkTime = 0;
      iterations = 0;
      MeanResponseTime = 1;
      Serial.println(F("Cleared"));
    }
    Serial.flush();
  }
}


void readyBlinking() {
  digitalWrite(LED_BUILTIN, LOW);   delay(250);
  digitalWrite(LED_BUILTIN, HIGH);  delay(250);
  digitalWrite(LED_BUILTIN, LOW);   delay(250);
  digitalWrite(LED_BUILTIN, HIGH);  delay(250);
  digitalWrite(LED_BUILTIN, LOW);   delay(250);
  digitalWrite(LED_BUILTIN, HIGH);  delay(250);
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
  
  
  Serial.print(F("Trying to filesystem: "));
  if(LittleFS.begin()) {
    Serial.println(F("success!"));
    
    const String filename = "/SensorData.txt";
    if(LittleFS.exists(filename)) {
      LittleFS.remove(filename);
    }
  }
  else {
    Serial.println(F("fail."));
    return;
  }
  Serial.println();
  
  
  Serial.println(F("Syncing time"));
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov"); //ua.pool.ntp.org
  while (!time(nullptr)) {
    Serial.print('.');
    delay(100);
  }
  
  
  server.on("/", [](){
    server.send(200, "text/html", preparedHtmlPage);
  });
  
  /**/
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

    if (server.hasArg("Download")) {
      const String filename = server.arg(F("Download"));
      Serial.println(F("Downloading"));
      
      File download = LittleFS.open(filename, "r");
      if (download) {
        server.sendHeader(F("Content-Type"), F("text/text"));
        String str = "attachment; filename=";
        str += filename;
        server.sendHeader(F("Content-Disposition"), str);
        server.sendHeader(F("Connection"), F("close"));
        server.streamFile(download, F("application/octet-stream"));
        download.close();
      }
      else {
        server.send(501);
      }
    }
    else if (server.arg(F("Led")) == F("OFF")) {
      Serial.println(F("Led go OFF but on GET"));
      led_state = HIGH;
      digitalWrite(LED_BUILTIN, led_state);
    }
    else if (server.hasArg(F("Measurement"))) {
      Serial.println(F("GET measurement"));
//      server.sendHeader(F("Content-Type"), F("text/plain"));
//      server.sendHeader(F("Connection"), F("Close"));
//      server.sendHeader("Connection", "Keep-Alive");
//      String payload = "{ax:" + String(ax) +", ay:" + String(ay) +", az:" + String(az) + "}";
//      String payload = "data: " + String(rand()%200);
    }
    
    Serial.flush();
    Serial.println();
    
    server.send(204);
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
      
      Start = micros();
      SensorLive = true;
//      currentFile = LittleFS.open(F("/SensorData.txt"), "a");
    }
    else if (server.arg("Msrmnt") == "OFF") {
      Serial.println(F("Msrmnt stop"));
      
//      currentFile.close();
      SensorLive = false;
      Finish = micros();
      ax = ay = az = 0;
      
      WorkTime = Finish - Start;
    }
    
    Serial.flush();
    Serial.println();
    
    server.send(204);
  });

  MeasurementsForResponse.reserve(1024);
  server.on("/msrmnt", HTTP_GET, [](){
    auto start_sending_time = micros();
//    Serial.println(server.readStringUntil("\n\r"));
    MeasurementsForResponse = "";
    for (uint8_t i=0; i<measurement_pos; i++) {
      MeasurementsForResponse += String(database[i].ax) +',' + String(database[i].ay) +','+ String(database[i].az) +';';
    }
    measurement_pos = 0;
    
    server.send(200, "text/plain", MeasurementsForResponse);  // arraybuffer/blob text/plain
    auto ResponseTime = (micros()-start_sending_time)/1000.0;
    Serial.print(F("GET measurement: "));
    Serial.println(ResponseTime);
    MeanResponseTime = (MeanResponseTime+ResponseTime)/2.0;
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

//  timer = timerBegin(0, 2, true); // use a prescaler of 2
//  timerAttachInterrupt(timer, &onTimer, true);
//  timerAlarmWrite(timer, 5000, true);
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
    
//    if (iteration%SEND_DATA_EVERY == 0) {
//      sendMeasurements();
//    }
//    if (iteration == DATA_SIZE_MAX) {
//      iteration = 0;
//    }
  }
}

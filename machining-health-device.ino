#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>

//#include <ESP8266mDNS.h>

#include <Adafruit_BMP085_U.h>
#include <Adafruit_HMC5883_U.h>
#include "I2Cdev.h"
#include "MPU6050.h"

#include <time.h>
#include <String>

#define LED_BUILTIN 2


const char* ssid = "yourNetwork";
const char* password = "secretPassword";

String last_console_text = "";
bool led_state = true;  //set off by default

bool SensorEmulatingLive = false;
bool SensorLive = false;
bool SensorToConsole = false;


ESP8266WebServer server(80);

MPU6050 accelgyro;
int16_t ax, ay, az;
int16_t gx, gy, gz;
Adafruit_BMP085_Unified bmp = Adafruit_BMP085_Unified(10085);
Adafruit_HMC5883_Unified mag = Adafruit_HMC5883_Unified(12345);


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
      /*
        "<div><p>Led pin is now: "\
          "<span id=\"led_state\">NE YASNO</span>"\
        "</p></div>"\
      */
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
    /*
    "<script>"\
      "setInterval(function() {getLedState();}, 2000);"\
      
      "function getLedState() {"\
        "var xhttp = new XMLHttpRequest();"\
        "xhttp.onreadystatechange = function() {"\
          "if (this.readyState == 4 && this.status == 200) {"\
            "document.getElementById(\"led_state\").innerHTML = this.responseText;"\
          "}"\
        "};"\
        "xhttp.open(\"GET\", \"/led_state\", true);"\
        "xhttp.send();"\
      "}"\
    "</script>"\
    */
    /*
    "<script>"\
      "var ESPurl = \"/rest/events/subscribe\";"\
      "if(typeof(EventSource) !== \"undefined\") {"\
        "var source = new EventSource(ESPurl);"\
        
         * source.addEventListener('console', function(e) {\
            "document.getElementById(\"console_text\").innerHTML += event.data + \"; \";"\
           }, false);\
        
        "source.onmessage = function(event) {"\
          "document.getElementById(\"console_text\").innerHTML += event.data + \"; \";"\
        "};"\
      "}"\
      "else {"\
        "document.write(\"Your browser does not support EventSource, buy a better one! \");"\
        //"document.write(window.location.url);"
      "}"\
    "</script>"\
    */
    "</body>"\
  "</html>";



String get_led_state_in_string() {
  if (led_state) {
    return "NE GORIT";
  }
  return "GORIT";
}



void FileSytemTest(const String& filename = "/f.txt") {
  File f = LittleFS.open(filename, "a+");
  if (f) {
    Serial.println("Filedata: ");
    while (f.available()) {
      Serial.print(f.readStringUntil('\n'));
    }
    Serial.println();
    Serial.flush();
    
    //f.println("+1");
    f.flush();
    f.close();
  }
  else {
    Serial.println(F("file openning error"));
  }
}



void EmulateSensor() {
  //Serial.println("SensorEmulator");
  const String filename = "/DummyData.txt";
  File f = LittleFS.open(filename, "a");
  if (f) {
    const int tmp = rand() % 100;
    time_t now = time(nullptr);
    
    f.print(ctime(&now));
    f.print(" - ");
    f.print(tmp);
    f.println(";");
    f.flush();
    
    f.close();
  }
  else {
    Serial.println(F("file openning error"));
  }
}



void SensorMeasurementsToConsole() {
  accelgyro.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  Serial.print(ax); Serial.print(' ');
  Serial.print(ay); Serial.print(' ');
  Serial.print(az); Serial.println(' ');
  Serial.flush();
  return;
  
  File f = LittleFS.open("/SensorData.txt", "a");
  if (f) {
    time_t now = time(nullptr);
    f.print(ctime(&now));
    f.print(F(" - "));
    
    // display tab-separated accel x/y/z values
    accelgyro.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
    Serial.print(ax); Serial.print(' ');
    Serial.print(ay); Serial.print(' ');
    Serial.println(az);
    
    f.print("a: ");
    f.print(ax); f.print(", ");
    f.print(ay); f.print(", ");
    f.print(az); f.println(';');
    f.flush();
      
    f.close();
  }
  else {
    Serial.println(F("file openning error"));
  }
}


void SensorMeasurements() {
  const String filename = "/SensorData.txt";
  File f = LittleFS.open(filename, "a");
  if (f) {
    time_t now = time(nullptr);
    f.println(ctime(&now));
    f.print(" - ");
    
    sensors_event_t event;
    bmp.getEvent(&event);
  
    if (event.pressure) {
      Serial.print("Pressure:    ");
      Serial.print(event.pressure);
      Serial.println(" hPa");
      
      float temperature;
      bmp.getTemperature(&temperature);
      Serial.print("Temperature: ");
      Serial.print(temperature);
      Serial.println(" C");
      
      float seaLevelPressure = SENSORS_PRESSURE_SEALEVELHPA;
      Serial.print("Altitude:    "); 
      Serial.print(bmp.pressureToAltitude(seaLevelPressure, event.pressure)); 
      Serial.println(" m");

      f.print("P: ");   f.print(event.pressure);    f.println(" hPa");
      f.print("T: ");   f.print(temperature);       f.println(" C");
      f.print("A: ");   f.print(bmp.pressureToAltitude(seaLevelPressure, event.pressure));  f.println(" m");
    }
    else {
      Serial.println("Sensor1 error");
    }
    
    
    // display tab-separated accel/gyro x/y/z values
    accelgyro.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
    Serial.print("a/g:\t");
    Serial.print(ax); Serial.print(", ");
    Serial.print(ay); Serial.print(", ");
    Serial.print(az); Serial.print(", ");
    Serial.print(gx); Serial.print(", ");
    Serial.print(gy); Serial.print(", ");
    Serial.println(gz);
    
    mag.getEvent(&event);
    /* Display the results (magnetic vector values are in micro-Tesla (uT)) */
    Serial.print("X: "); Serial.print(event.magnetic.x); Serial.print(", ");
    Serial.print("Y: "); Serial.print(event.magnetic.y); Serial.print(", ");
    Serial.print("Z: "); Serial.print(event.magnetic.z); Serial.print(" ");Serial.println("uT");
    
    f.print("a/g:\t");
    f.print(ax); f.print(", ");
    f.print(ay); f.print(", ");
    f.print(az); f.print(", ");
    f.print(gx); f.print(", ");
    f.print(gy); f.print(", ");
    f.println(gz);

//    f.print("X: "); f.print(event.magnetic.x); f.print(", ");
//    f.print("Y: "); f.print(event.magnetic.y); f.print(", ");
//    f.print("Z: "); f.print(event.magnetic.z); f.print("  "); f.println("uT");

    
    float heading = atan2(event.magnetic.y, event.magnetic.x);
    
    float declinationAngle = +5.22;
    heading += declinationAngle;
    
    if (heading < 0)
      heading += 2*PI;
    
    if (heading > 2*PI)
      heading -= 2*PI;
    
    float headingDegrees = heading * 180/M_PI;
    Serial.print("Heading (degrees): "); Serial.println(headingDegrees);
    Serial.println();
    
    f.print("Heading (degrees): "); f.println(headingDegrees);
    f.println(";");
    f.flush();
      
    f.close();
    delay(5); 
  }
  else {
    Serial.println(F("file openning error"));
  }
}


void ParseConsole() {
  if (Serial.available() > 0) {
    last_console_text = Serial.readStringUntil('\n');
    Serial.print(F("console: \'"));
    Serial.print(last_console_text);
    Serial.println(F("\'"));
    
    if (last_console_text == "stop") {  // wi-fi
      last_console_text.clear();
      Serial.println(F("stopping wi-fi module"));
      server.stop();
      Serial.println(F("~~~"));
      Serial.println();
      return;
    }
    else if (last_console_text == "stats") {
      last_console_text.clear();
      Serial.println(F("~Memory Stats~"));
      Serial.print(F("FreeHeap: "));
      Serial.println(ESP.getFreeHeap(), DEC);
      Serial.print(F("HeapFragmentation: "));
      Serial.println(ESP.getHeapFragmentation());
      Serial.print(F("FlashChipSize: "));
      Serial.println(ESP.getFlashChipSize(), DEC);
      Serial.println();
      return;
    }
    else if (last_console_text == "fstest") {
      last_console_text.clear();
      Serial.println(F("File system check"));
      
      time_t now = time(nullptr);
      Serial.println(ctime(&now));
      
      //FileSytemTest();
      FileSytemTest(F("/DummyData.txt"));
      Serial.println();
    }
    else if (last_console_text == "emul") {
      SensorEmulatingLive = !SensorEmulatingLive;
      Serial.print(F("Emulating is "));
      Serial.print(SensorEmulatingLive);
      Serial.println(F(" now"));
    }
    else if (last_console_text == "measure") {
      SensorLive = !SensorLive;
      Serial.print(F("Measuring is "));
      Serial.print(SensorEmulatingLive);
      Serial.println(F(" now"));
    }
    else if (last_console_text == "measuretoconsole") {
      SensorToConsole = !SensorToConsole;
      led_state = !led_state;
      digitalWrite(LED_BUILTIN, led_state);
      
      Serial.println(F("ax ay az"));
//      Serial.print(F("Measuring is "));
//      Serial.print(SensorToConsole);
//      Serial.println(F(" now"));
    }
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
  
  Serial.begin(115200);
  delay(2500);
  
  Serial.println(F("Initializing GY-87..."));
  Serial.flush();

  Wire.begin();
  accelgyro.initialize();
  
  accelgyro.setI2CMasterModeEnabled(false);
  accelgyro.setI2CBypassEnabled(true) ;
  accelgyro.setSleepEnabled(false);
  
  Serial.println(accelgyro.testConnection() ? "MPU6050 connection successful" : "MPU6050 connection failed");

  if(!bmp.begin()) {
    /* There was a problem detecting the BMP085 ... check your connections */
    Serial.println(F("Ooops, no BMP085 detected ... Check your wiring or I2C ADDR!"));
    while(1);
  }
  else {
    Serial.println(F("Yees!"));
  }
  
  if(!mag.begin()) {
    /* There was a problem detecting the HMC5883 ... check your connections */
    Serial.println(F("Ooops, no HMC5883 detected ... Check your wiring!"));
    while(1);
  }
  else {
    Serial.println(F("Yees!Yeeeeees!"));
  }

  if( accelgyro.testConnection() && bmp.begin() && mag.begin() ) {
    readyBlinking();
  }

  //ya sohranil
  Serial.print(accelgyro.getXAccelOffset());  Serial.print("\t"); // -3572
  Serial.print(accelgyro.getYAccelOffset());  Serial.print("\t"); // -1871
  Serial.print(accelgyro.getZAccelOffset());  Serial.print("\t"); // 1486
  Serial.print(accelgyro.getXGyroOffset());   Serial.print("\t"); // 0
  Serial.print(accelgyro.getYGyroOffset());   Serial.print("\t"); // 0
  Serial.print(accelgyro.getZGyroOffset());   Serial.print("\t"); // 0
  Serial.print("\n");
//  accelgyro.setXGyroOffset(220);
//  accelgyro.setYGyroOffset(76);
//  accelgyro.setZGyroOffset(-85);

//  accelgyro.setXAccelOffset(0);
//  accelgyro.setYAccelOffset(0);
//  accelgyro.setZAccelOffset(0);
  
//  delay(2500);

  
  // Connect to WiFi network
  Serial.println();
  Serial.println();
  Serial.print(F("Connecting to "));
  Serial.println(ssid);

  
//  if (WiFi.softAP("ESPsoftAP_test", "tester123")) {
//    readyBlinking();
//  }
  
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
    FSInfo filesystem_info;
    Serial.println(LittleFS.info(filesystem_info));
    Serial.println(filesystem_info.totalBytes);
    Serial.println(filesystem_info.usedBytes);

    const String filename1 = "/DummyData.txt";
    if(LittleFS.exists(filename1)) {
      LittleFS.remove(filename1);
    }
    else {
      Serial.println(F("Wow, really?"));
    }
    
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
  
  
  Serial.println(F("Syncing time"));
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov"); //ua.pool.ntp.org
  while (!time(nullptr)) {
    Serial.print('.');
    delay(100);
  }

  
  server.on("/", [](){
    server.send(200, "text/html", preparedHtmlPage);
  });
  
  
  server.on("/led-state-ping", [](){
    const String str = "data: "+ get_led_state_in_string() +"\n\n";
    server.sendHeader("Cache-Control", "no-cache");
    server.send(200, "text/event-stream", str);
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
      const String filename = server.arg("Download");
      Serial.println(F("Downloading"));
      
      File download = LittleFS.open(filename, "r");
      if (download) {
        server.sendHeader("Content-Type", "text/text");
        String str = "attachment; filename=";
        str += filename;
        server.sendHeader("Content-Disposition", str);
        server.sendHeader("Connection", "close");
        server.streamFile(download, "application/octet-stream");
        download.close();
      }
      else {
        server.send(501);
      }
    }
    else if (server.arg("Led") == "OFF") {
      Serial.println(F("Led go OFF but on GET"));
      led_state = HIGH;
      digitalWrite(LED_BUILTIN, led_state);
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
//      SensorEmulatingLive = true;
        SensorLive = true;
    }
    else if (server.arg("Msrmnt") == "OFF") {
      Serial.println(F("Msrmnt stop"));
//      SensorEmulatingLive = false;
        SensorLive = false;
    }
    
    Serial.flush();
    Serial.println();
    
    server.send(204);
  });

  
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
  ParseConsole();
  server.handleClient();
  if (SensorEmulatingLive) {
    EmulateSensor();
  }
  if (SensorLive) {
    SensorMeasurements();
  }
  if (SensorToConsole) {
    SensorMeasurementsToConsole();
  }
}

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>

//#include <ESP8266mDNS.h>

#include <Adafruit_BMP085_U.h>
#include <Adafruit_HMC5883_U.h>
#include "I2Cdev.h"
#include "MPU6050.h"
//#include <Vector.h>

#include <time.h>
#include <String>

#define LED_BUILTIN 2
#define MAX_DATASIZE 1500


const char* ssid = "yourNetwork";
const char* password = "secretPassword";

String last_console_text = "";
bool led_state = true;  //set off by default

bool SensorLive = false;


struct dataItem {
  time_t timeStomp;
  int16_t ax, ay, az;
  int16_t gx, gy, gz;
};

//Vector<dataItem> dataStorage;
dataItem dataStorage[MAX_DATASIZE];
size_t storageSize = 0;

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



void SensorMeasurements() {
  accelgyro.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  dataStorage[storageSize++] = {time(nullptr), ax, ay, az, gx, gy, gz};
//  dataStorage.push_back(dataItem{ax, ay, az, gx, gy, gz}); //time(nullptr), 
  Serial.println(ax);
//    time_t now = time(nullptr);
//    currentFile.println(ctime(&now));
//    currentFile.print(" - ");
//    
//    // display tab-separated accel/gyro x/y/z values
//    accelgyro.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
//    
//    currentFile.print("a/g:\t");
//    currentFile.print(ax);  currentFile.print(", ");
//    currentFile.print(ay);  currentFile.print(", ");
//    currentFile.print(az);  currentFile.print(", ");
//    currentFile.print(gx);  currentFile.print(", ");
//    currentFile.print(gy);  currentFile.print(", ");
//    currentFile.print(gz);  currentFile.println(";");
//    currentFile.flush();
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
    FSInfo filesystem_info;
    Serial.println(LittleFS.info(filesystem_info));
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
      
      File currentFile;
      currentFile = LittleFS.open(F("/SensorData.txt"), "a");
      
      Serial.print(F("dataStorage size = "));
      Serial.println(storageSize);
      for (int i = 0; i<storageSize; i++) {
        currentFile.println(ctime(&dataStorage[i].timeStomp));
        currentFile.print(dataStorage[i].ax); currentFile.print(',');
        currentFile.print(dataStorage[i].ay); currentFile.print(',');
        currentFile.print(dataStorage[i].az); currentFile.print(',');
        currentFile.print(dataStorage[i].gx); currentFile.print(',');
        currentFile.print(dataStorage[i].gy); currentFile.print(',');
        currentFile.print(dataStorage[i].gz); currentFile.println(';');
      }
      storageSize = 0;
//      Serial.println(dataStorage.size());
//      
//      for (const auto& el:dataStorage) {
////        currentFile.println(ctime(&el.timeStomp));
//        currentFile.print(el.ax); currentFile.print(',');
//        currentFile.print(el.ay); currentFile.print(',');
//        currentFile.print(el.az); currentFile.print(',');
//        currentFile.print(el.gx); currentFile.print(',');
//        currentFile.print(el.gy); currentFile.print(',');
//        currentFile.print(el.gz); currentFile.println(';');
//      }
//      dataStorage.clear();
      
      currentFile.close();
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
  if (SensorLive) {
    SensorMeasurements();
  }
}

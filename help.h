const String CONST_STRING = "-99999,-99999,-99999;-99999,-99999,-99999;-99999,-99999,-99999;-99999,-99999,-99999;-99999,-99999,-99999;"\
      "-99999,-99999,-99999;-99999,-99999,-99999;-99999,-99999,-99999;-99999,-99999,-99999;-99999,-99999,-99999;"\
      "-99999,-99999,-99999;-99999,-99999,-99999;-99999,-99999,-99999;-99999,-99999,-99999;-99999,-99999,-99999;"\
      "-99999,-99999,-99999;-99999,-99999,-99999;-99999,-99999,-99999;-99999,-99999,-99999;-99999,-99999,-99999;"\
      "-99999,-99999,-99999;-99999,-99999,-99999;-99999,-99999,-99999;-99999,-99999,-99999;";



const String toString(int16_t i) {
  if(i < 0) {
    return '-'+String(abs(i));
  }
  return String(abs(i));
}



void readyBlinking() {
  digitalWrite(LED_BUILTIN, LOW);   delay(250);
  digitalWrite(LED_BUILTIN, HIGH);  delay(250);
  digitalWrite(LED_BUILTIN, LOW);   delay(250);
  digitalWrite(LED_BUILTIN, HIGH);  delay(250);
  digitalWrite(LED_BUILTIN, LOW);   delay(250);
  digitalWrite(LED_BUILTIN, HIGH);  delay(250);
}



//typedef void (*timer_callback)  ();
template <typename timer_callback> //class T
void AttachTimerInterrupt(const unsigned long interval, timer_callback callback) {
  #define TIM_SELECTED TIM_DIV1 //TIM_DIV1 TIM_DIV16 TIM_DIV256
  #define DIV_SELECTED 1
  
  timer1_isr_init();
  timer1_attachInterrupt(callback);
  
  #define MAX_ESP8266_COUNT 8388607
  const float frequency = 1000000.0f / interval;
  const float _frequency  = 80000000 / DIV_SELECTED;
  uint32_t _timerCount = (uint32_t) _frequency / frequency;
  if ( _timerCount > MAX_ESP8266_COUNT) {
    Serial.println('!');
    _timerCount = MAX_ESP8266_COUNT;
  }
  timer1_write(_timerCount);
  
  timer1_enable(TIM_SELECTED, TIM_EDGE, TIM_LOOP);

}



void TimerInterruptDisable() {
  timer1_disable();
}



double accelToG_double(const int16_t a, const uint8_t accelRange = 0) {
  const double ACCEL_TO_G_DIVIDER = accelRange == 0 ? 16384.0 :
                                      accelRange == 1 ? 8192.0 :
                                      accelRange == 2 ? 4096.0 : 2048.0;
  return double(a)/ACCEL_TO_G_DIVIDER;
}


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

//const String CONST_STRING = "-99999,-99999,-99999;-99999,-99999,-99999;-99999,-99999,-99999;-99999,-99999,-99999;-99999,-99999,-99999;"\
//      "-99999,-99999,-99999;-99999,-99999,-99999;-99999,-99999,-99999;-99999,-99999,-99999;-99999,-99999,-99999;"\
//      "-99999,-99999,-99999;-99999,-99999,-99999;-99999,-99999,-99999;-99999,-99999,-99999;-99999,-99999,-99999;"\
//      "-99999,-99999,-99999;-99999,-99999,-99999;-99999,-99999,-99999;-99999,-99999,-99999;-99999,-99999,-99999;"\
//      "-99999,-99999,-99999;-99999,-99999,-99999;-99999,-99999,-99999;-99999,-99999,-99999;";



const String toString(int16_t i) {
  if(i < 0) {
    return '-'+String(abs(i));
  }
  return String(abs(i));
}


template <typename T>
float getRMS(const T tmp1, const T tmp2, const T tmp3) {
  const float sq1 = float(tmp1)*float(tmp1);
  const float sq2 = float(tmp2)*float(tmp2);
  const float sq3 = float(tmp3)*float(tmp3);
  return sqrt(sq1 + sq2 + sq3);
}

//template <typename T1, typename T2>
//T2 getRMS(const T1 tmp1, const T1 tmp2, const T1 tmp3) {
//  return sqrt(tmp1*tmp1 + tmp2*tmp2 + tmp3*tmp3);
//}



template <typename T1, typename T2>
T1 accelToG(const T2 a, const uint8_t accelRange = 0) {
  const T1 ACCEL_TO_G_DIVIDER = accelRange == 0 ? 16384.0 :
                                accelRange == 1 ? 8192.0 :
                                accelRange == 2 ? 4096.0 : 2048.0;
  return T1(a)/ACCEL_TO_G_DIVIDER;
}

template <typename T1, typename T2>
T1 accelToMMpS(const T2 a, const uint8_t accelRange = 0) {
  const T1 ACCEL_TO_G_DIVIDER = accelRange == 0 ? 16384.0 :
                                accelRange == 1 ? 8192.0 :
                                accelRange == 2 ? 4096.0 : 2048.0;
  return T1(a)*1000.0/ACCEL_TO_G_DIVIDER;
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
  const double frequency = 1000000.0f / interval;
  const double _frequency  = 80000000 / DIV_SELECTED;
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



const String preparedHtmlPage PROGMEM = 
  "<!DOCTYPE html>"\
    "<html>"\
    "<head>"\
    "<title>ESP8266 Demo</title>"\
    "<link rel=\"icon\" href=\"data:,\">"\
    "<style>"\
     " body {background-color:#2c2c34; font-family:Arial, Helvetica, Sans-Serif; Color:#519cd4; text-align:center}"\
    "</style>"\
  "</head>"\
  "<body>"\
    "<script>"\
      "const hostIp='http://192.168.0.100:80';"\
      "var accel_range='+-2g';"\
      "var measurement_freq=100;//10*2.56;"\
      "var fft_interval_count=256;"\
      "var fft_limits=[];"\
      "document.addEventListener('DOMContentLoaded', async function() {"\
        "fetch(hostIp+'/req/get'+'?AccelRange')"\
          ".then(r => r.text())"\
            ".then(text =>{"\
              "accel_range = text;"\
              "document.getElementById('accel_range').value = text;"\
            "});"\
        "fetch(hostIp+'/req/get'+'?MeasurementFreq')"\
          ".then(r => r.text())"\
            ".then(text =>{"\
              "measurement_freq = parseFloat(text);"\
              "document.getElementById('freq').value = measurement_freq;"\
            "});"\
        "fetch(hostIp+'/req/get'+'?FFTintervals')"\
          ".then(r => r.text())"\
            ".then(text =>{"\
              "fft_interval_count = parseFloat(text);"\
              "document.getElementById('interval_count').value=fft_interval_count;"\
            "});"\
        "fetch(hostIp+'/req/get'+'?FFTlimits')"\
          ".then(r => r.text())"\
            ".then(text =>{"\
              "fft_limits=text.split(',').map(Number);"\
            "});"\
      "});"\
    "</script>"\
    "<h2>Master's dissertation project by Ihor Lupyna - Machine Health/Condition Monitoring</h2>"\
    "<div>"\
      "<h3>Measurements Control Panel</h3>"\
      "<p>"\
        "<form method='post' action='/req/post'>"\
          "<button name='Msrmnt' value='ON' onclick='online()'>Start Measurements</button>"\
          "<button name='Msrmnt' value='OFF' onclick='offline()'>Stop Measurements</button>"\
        "</form>"\
        "<script>"\
          "var live = false;"\
          "unction online(){live=true;}"\
          "function offline(){live=false;/*forceDownloadDataFile();*/}"\
          "function downloadFile(){forceDownloadDataFile();}"\
        "</script>"\
      "</p>"\
      "<p>"\
        "<button onclick='downloadFile()'>Download button</button>"\
      "</p>"\
      "<div>"\
        "<p>"\
          "<button onclick='document.getElementById('alarm_p').innerHTML='''>Clear Alarms</button>"\
        "</p>"\
        "<p id='alarm_p'>Here to alarm!</p>"\
      "</div>"\
    "</div>"\
    "<div>"\
      "<h3>Charts</h3>"\
      "<script type='text/javascript' src='https://www.gstatic.com/charts/loader.js'></script>"\
      "<div id='accel_chart_div'></div>"\
      "<input type='range' min='-1000' max='-100' id='chart_range'></input>"\
      "<div id='fft_chart_div'></div>"\
    "</div>"\
    "<script>"\
      "google.charts.load('current',{packages:['corechart','line']});"\
      "var accelQueue=[];"\
      "var meanSquareVal;"\
      "var n=0;"\
      "let fftValues=[];"\
      "let fftLimits=[];"\
      "var temperatureValue;"\
      "var virtualFile="";"\
      "var AccelChart,FFTChart;"\
      "const max_q_length=1000;"\
      "function forceDownloadDataFile() {"\
        "const a=document.createElement('a');"\
        "a.download='data.csv';"\
        "a.href=URL.createObjectURL(new Blob ([virtualFile], {type : 'text/plain'}));"\
        "a.click();"\
        "virtualFile="";"\
      "}"\
      "setInterval(async function() {"\
        "if(!live) {return;}"\
        "await getMeasurement();"\
        "await checkAlarmsAccel();"\
        "drawAccelChart();"\
      "}, 100);//100/measurement_freq*1000"\
      "function drawAccelChart() {"\
        "if(AccelChart){AccelChart.clearChart()}"\
        "const range_scale=accelQueue.length+Number(document.getElementById('chart_range').value);"\
        "const scaler=(range_scale>0) ? range_scale : 0;"\
        "var data, options;"\
        "if(accelQueue[0].length==4) {"\
          "data=new google.visualization.arrayToDataTable([['n','ax','ay','az'], ...accelQueue.slice(scaler)]);"\
          "options={"\
            "hAxis:{title:'Time'},"\
            "vAxis:{title:'Acceleration'},"\
            "colors:['#a52714','#14a527','#2714a5']"\
          "};"\
        "}"\
        "else if(accelQueue[0].length==2) {"\
          "data=new google.visualization.arrayToDataTable([['n','a'], ...accelQueue.slice(scaler)]);"\
          "options={"\
            "hAxis:{title:'Time'},"\
            "vAxis:{title:'Acceleration'},"\
            "colors:['#a52714']"\
          "};"\
        "}"\
        "AccelChart=new google.visualization.LineChart(document.getElementById('accel_chart_div'));"\
        "AccelChart.draw(data,options);"\
      "};"\
      "async function getMeasurement() {"\
        "fetch(hostIp+'/msrmnt')"\
          ".then(r => r.text())"\
            ".then(data =>{"\
              "virtualFile+=data;"\
              "const values=data.split(';');//.slice(1);"\
              "values.forEach(row => {"\
                "if(!row) return;"\
                "const vals=row.split(',').map(Number);"\
                "accelQueue.push([n++, ...vals]);"\
              "});"\
              "while (accelQueue.length>max_q_length) {"\
                "accelQueue.shift();"\
              "}"\
            "});"\
      "}"\
      "async function checkAlarmsAccel(){"\
        "var sum=0;"\
        "accelQueue.forEach(el=>{sum+=el*el});"\
        "sum/=accelQueue.length;"\
        "if(sum>meanSquareVal){"\
          "document.getElementById('alarm_p').innerHTML+='MeanSquereValue: '+Number(meanSquareVal)+'>setted;\n';"\
        "}"\
      "}"\
      "setInterval(async function() {"\
        "if(!live) {return}"\
        "await getFFTValues();"\
        "await checkAlarmsFFT();"\
        "drawFFTChart();"\
      "}, 1000); //fft_interval_count*2/measurement_freq*1000"\
      "function drawFFTChart() {"\
        "if(FFTChart){FFTChart.clearChart()}"\
        "var data=new google.visualization.arrayToDataTable([['freq','accel'], ...fftValues]);"\
        "var options={"\
          "hAxis:{title:'Frequency, HZ'},"\
          "vAxis:{title:'Accel, G`s'},"\
          "colors:['#a52714']"\
        "};"\
        "FFTChart=new google.visualization.LineChart(document.getElementById('fft_chart_div'));"\
        "FFTChart.draw(data,options);"\
      "};"\
      "async function getFFTValues() {"\
        "const response=await fetch(hostIp+'/fft');"\
        "const data=await response.text();"\
        "var i=0;"\
        "fftValues=[];"\
        "const values=data.split(';').map(Number);//.slice(1);"\
        "values.forEach(val => {"\
          "if(!val)return;"\
          "const curr_freq=((i++)*1.0*measurement_freq)/fft_interval_count;"\
          "fftValues.push([curr_freq,val]);"\
        "});"\
      "}"\
      "async function checkAlarmsFFT() {"\
        "for(var i=0; i<intervals; i++) {"\
          "if(fftValues[i]>fftLimits[i]) {"\
            "document.getElementById('alarm_p').innerHTML+='FFT Value in '+i+' out of limit ('+fftValues[i]+'>'+fftLimits[i]+');\n';"\
          "}"\
        "}"\
      "}"\
    "</script>"\
    "<div>"\
      "<p>"\
        "<button type='button' id='switcher' onclick='switchForm()'>Set new limits</button>"\
        "<script>"\
          "var form_is_hidden=true;"\
          "function switchForm() {"\
            "if(form_is_hidden) {"\
              "document.getElementById('new_values_form').style.display='block';"\
              "form_is_hidden=false;"\
              "document.getElementById('switcher').innerHTML='Hide limits panel';"\
            "}"\
            "else{"\
              "document.getElementById('new_values_form').style.display='none';"\
              "form_is_hidden=true;"\
              "document.getElementById('switcher').innerHTML='Set new limits';"\
            "}"\
          "}"\
          "var withTemperature=false, withAccelRange=false; withFreq=false; withFFT=false; withRMS=false;"\
        "</script>"\
      "</p>"\
      "<div id='new_values_form' style='display:none'>"\
        "<script>"\
          "function VisibilityChange(check,elId) {"\
            "if(check){"\
              "document.getElementById(elId).style.visibility='visible';"\
            "}"\
            "else{"\
              "document.getElementById(elId).style.visibility='hidden';"\
            "}"\
          "}"\
        "</script>"\
        "<p>"\
          "<input type='checkbox' onclick='{VisibilityChange(this.checked, 'accel_range');withAccelRange=!withAccelRange}'>"\
          "Acceleration measurement range:"\
          "<select id='accel_range' style='visibility:hidden'>"\
            "<option value='2g'>+-2g</option>"\
            "<option value='4g'>+-4g</option>"\
            "<option value='8g'>+-8g</option>"\
            "<option value='16g'>+-16g</option>"\
          "</select>"\
        "</p>"\
        "<p>"\
          "<input type='checkbox' onclick='{VisibilityChange(this.checked,'temp');withTemperature=!withTemperature}'>"\
          "Limit Temperature in Celsius:"\
          "<input type='number' id='temp' value='44' style='visibility:hidden'> </input>"\
        "</p>"\
        "<p>"\
          "<input type='checkbox' onclick='{VisibilityChange(this.checked, 'freq');withFreq=!withFreq}'>"\
          "Working Frequency in"\
          "<select id='freq_units'>"\
            "<option value='Hz'>Hz</option>"\
            "<option value='CPM'>CPM</option>"\
          "</select>"\
          ":"\
          "<input type='number' id='freq' value='10.66667' style='visibility:hidden'> </input>"\
        "</p>"\
        "<p>"\
          "<input type='checkbox' onclick='{VisibilityChange(this.checked, 'rms');withRMS=!withRMS}'>"\
          "Limit Root Mean Square for interval count values:"\
          "<input type='number' id='rms' value='12' style='visibility:hidden'> </input>"\
        "</p>"\
        "<div>"\
          "<p>"\
            "<input type='checkbox' onclick='{VisibilityChange(this.checked, 'fft'); withFFT=withFFT}'>"\
            "FFT"\
          "</p>"\
          "<div id='fft' style='visibility:hidden'>"\
            "<select id='interval_count'>Intervals:"\
              "<option></option>"\
              "<option value='32'>32</option>"\
              "<option value='64'>64</option>"\
              "<option value='128'>128</option>"\
              "<option value='256'>256</option>"\
            "</select>"\
            "<button type='button' id='add_fieds_values' onclick='add_fields()'>Get Fields</button>"\
            "<script>"\
              "var intervals_is_hidden = true;"\
              "function add_fields() {"\
                "const parent=document.getElementById('intervals');"\
                "while (parent.firstChild) {"\
                  "parent.removeChild(parent.firstChild);"\
                "}"\  
                "const intervals=document.getElementById('interval_count').value;"\
                "if(intervals>0){"\
                  "intervals_is_hidden = false;"\
                  "const freq=document.getElementById('freq').value * 2.56;"\
                  "const freq_units=document.getElementById('freq_units').value;"\
                  "if(freq_units=='CPM') {freq /= 60}"\
                  "for(var i=0; i<intervals; i++) {"\
                    "const interval_id='input_id_'+i;"\
                    "const newline=document.createElement('div');"\
                    "const nofield=document.createElement('input');"\
                    "with(nofield) {"\
                      "type='number';"\
                      "value=(fft_limits[i]!=null) ? fft_limits[i] : '1.0';"\
                      "id=interval_id;"\
                    "}"\
                    "const curr_freq=(i*1.0*freq)/intervals;"\
                    "const freqText=curr_freq.toFixed(2) + ': ';"\
                    "newline.append(freqText, nofield);"\
                    "parent.append(newline);"\
                  "}"\
                "}"\
                "else{intervals_is_hidden=true}"\
              "}"\
            "</script>"\
            "<p>Intervals max values for 2.56*intervals in G</p>"\
            "<p id='intervals'></p>"\
          "</div>"\
          "<button type='button' onclick='submit()'>Submit</button>"\
          "<script>"\
            "function submit(){"\
              "var data=[];"\
              "var freq,intervals,rms;"\
              "var freqs=[];"\
              "if(withTemperature) {data.push('NewTempLimit='+parseFloat(document.getElementById('temp').value));}"\
              "if(withAccelRange) {data.push('NewAccelRange='+document.getElementById('accel_range').value);}"\
              "if(withFreq){"\
                "freq=parseFloat(document.getElementById('freq').value)"\
                "data.push('NewMeasurementFreq='+freq);"\
              "}"\
              "if(withRMS){"\
                "rms=parseFloat(document.getElementById('rms').value);"\
                "data.push('NewAccelRMS='+rms);"\
              "}"\
              "if(withFFT){"\
                "intervals = document.getElementById('interval_count').value;"\
                "if(!intervals_is_hidden && intervals>0) {"\
                  "for (let i=0; i<intervals; i++) {"\
                    "const id='input_id_'+i;"\
                    "freqs.push(document.getElementById(id).value);"\
                  "}"\
                  "data.push('NewFreqLimits='+freqs.join(','));"\
                "}"\
              "}"\
              "fetch(hostIp+'/req/post',{"\
                "method:'POST',"\
                "headers:{'Content-Type':'application/x-www-form-urlencoded'},"\
                "body:data.join('&')"\
              "}).then(resp =>{"\
                "if(resp.ok){"\
                  "measurement_freq = freq*2.56;"\
                  "console.log(measurement_freq);"\
                  "fft_interval_count=intervals;"\
                  "fftLimits=freqs;"\
                  "meanSquareVal=rms;"\
                "}"\
              "})"\
            "}"\
          "</script>"\
        "</div>"\
      "</div>"\
    "</div>"\
  "</body>"\
"</html>";

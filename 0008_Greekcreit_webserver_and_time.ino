
/*********

  The dark theme can be found here https://github.com/jeffThompson/DarkArduinoTheme

  Most of the code was taken from a great website
  Rui Santos
  Complete project details at https://randomnerdtutorials.com/esp8266-dht11dht22-temperature-and-humidity-web-server-with-arduino-ide/

  I have added our coding into it

   Dew point calculations    http://bmcnoldy.rsmas.miami.edu/Humidity.html

   time code by  https://github.com/G6EJD/ESP_Simple_Clock_Functions/blob/master/ESP_RTC_with_NTP_Synchronisation_SSD1306_OLED.ino
                     Dave can be found  https://www.youtube.com/user/G6EJD
  I now use an extra File called "myconfig.h" which store the passwords and secret stuff that I want to avoid showing on the
  YouTube videos

  oled new library esp8266-oled-ssd1306 ThingPluse


  https://www.plus2net.com/javascript_tutorial/clock.php   adding a clock to the webpage


  Fontawesome   https://fontawesome.com/icons/database?style=solid

*********/

// Import required libraries

#include "time.h"
#include <Wire.h>
#include "SSD1306.h"                 //See https://github.com/squix78/esp8266-oled-ssd1306 or via Sketch/Include Library/Manage Libraries
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>

#define SDA 2                         // define where our SDA pin is connected
#define SCL 14                        // define where our SCL pin is connected
#define DHTPIN 5                      // Digital pin connected to the DHT sensor


void WiFiStart();                                    // i had to put these here otherwise the IDE would not use them
void Setup_Interrupts_and_Initialise_Clock();
void UpdateLocalTime();
void timer0_ISR (void);
void IRAM_ATTR onTimer();

SSD1306 display(0x3c, SDA, SCL);   // OLED display object definition (address, SDA, SCL) this is because we have changed libraries


//#include my ***** PRIVATE DETAILS  ***********
// this look at the code in the myconfig files in the extra tab

//#include "myconfig.h"


// Replace with your network credentials
const char* ssid     = "myssid";					// add your ssid here
const char* password = "mypassword";				// add your password here


const char* Timezone =  "CET-1CEST,M3.5.0,M10.5.0/3";   // europe italy/rome  this makes it easy to change where in the world you are.  I had this HARD coded
                                                        // in the void loop before which is bad practice


//Example time zones see: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
//const char* Timezone = "GMT0BST,M3.5.0/01,M10.5.0/02";     // UK
//const char* Timezone = "MET-2METDST,M3.5.0/01,M10.5.0/02"; // Most of Europe
//const char* Timezone = "CET-1CEST,M3.5.0,M10.5.0/3";       // Central Europe
//const char* Timezone = "EST-2METDST,M3.5.0/01,M10.5.0/02"; // Most of Europe
//const char* Timezone = "EST5EDT,M3.2.0,M11.1.0";           // EST USA
//const char* Timezone = "CST6CDT,M3.2.0,M11.1.0";           // CST USA
//const char* Timezone = "MST7MDT,M4.1.0,M10.5.0";           // MST USA
//const char* Timezone = "NZST-12NZDT,M9.5.0,M4.1.0/3";      // Auckland
//const char* Timezone = "EET-2EEST,M3.5.5/0,M10.5.5/0";     // Asia
//const char* Timezone = "ACST-9:30ACDT,M10.1.0,M4.1.0/3":   // Australia

String Date_str, Time_str;
volatile unsigned int local_Unix_time = 0, next_update_due = 0;
volatile unsigned int update_duration = 60 * 60;              // Time duration in seconds, so synchronise every hour


                                                              // Uncomment the type of sensor in use:
#define DHTTYPE    DHT11     // DHT 11
//#define DHTTYPE    DHT22     // DHT 22 (AM2302)
//#define DHTTYPE    DHT21     // DHT 21 (AM2301)

DHT dht(DHTPIN, DHTTYPE, 11);

                                                                // current temperature & humidity, updated in loop()
float t = 0.0;
float h = 0.0;
float dhi = 0.0;
float DP = 0.0;

String tempsensorworking;
String humidsensorworking;
int tempsensor1;
int humidsensor1;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Generally, you should use "unsigned long" for variables that hold time
// The value will quickly become too large for an int to store

unsigned long previousMillis = 0;    // will store last time in mills that the DHT was updated

// to control the loop
int interval = 3000;   // update the clock every second

// ************************************************  lets make a web page *********************************

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" >
  <title>DHT Server</title>
  
  <style>
 html {
   font-family: Arial;
   display: inline-block;
   margin: 0px auto;
   text-align: center;
  }
  
  h1 { font-size: 1.8rem;
        font-family : "Times New Roman", Arial;           // just playing with fonts
        text-align: centre;
        color: red;
        }
  h2 { font-size: 1.4rem; }
  h3 { font-size: 1.0rem; 
        color :blue;
        }
  p { font-size: 1.4rem; 
      font-family: Arial, "Time New Roman";            // just playing with fonts
      text-align: centre;
      text-transform: capitalize;
      }
  .units { font-size: 0.9rem; }
  .dht-labels{
    font-size: 1.2rem;
    vertical-align:middle;
    padding-bottom: 10px;
  }
  .Working{color:green;
  }
  .Failed{color:red; animation: blinker steps(1) 500ms infinite alternate;
  }
   
</style>


  <script type="text/javascript">                       // used to refresh the seconds on the day month year time clock every 1 second
  function display_c(){
  var refresh=1000; // Refresh rate in milli seconds
  mytime=setTimeout('display_ct()',refresh)
}

  function display_ct() {
  var x = new Date()
  document.getElementById('ct').innerHTML = x;
  display_c();
 }

</script>

</head>

<body>
  <h1>DHT-11 Web Server</h1>
  <h3> With Error Checking</h4>

  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i>
    <span class="dht-labels">Temperature:- </span>
    <span id="temperature">%TEMPERATURE_PH%</span>
    <sup class="units">&deg;C</sup>
    </p>

  <p>
    <i class="fas fa-tint" style="color:#00add6;"></i>
    <span class="dht-labels">Humidity:- </span>
    <span id="humidity">%HUMIDITY_PH%</span>
    <sup class="units">%%</sup>
  </p>

  <p>
    <i class="fas fa-tint" style="color:#00add6;"></i>
    <span class="dht-labels">Dew Point:- </span>
    <span id="dewpoint">%DEWPOINT_PH%</span>
    <sup class="units">%%</sup>
  </P>

  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i>
    <span class="dht-labels">Heat Index:- </span>
    <span id="heatindex">%HEATINDEX_PH%</span>
    <sup class="units">&deg;C</sup>
  </p>
   
  <p>
    <i class="fas fa-database" style="color:#059e8a;"></i>
    <span class="dht-labels">Temp Sensor:- </span>
    <span id="tempsensor">%TEMPSENSOR_PH%</span> 
  </p>

<p>
    <i class="fas fa-database" style="color:#059e8a;"></i>
    <span class="dht-labels">Humid Sensor:- </span>
    <span id="humidsensor" <p> class="<?php echo $value__class;?>>%HUMIDSENSOR_PH%</span>,</p>
</P>

<p>
    <span class="dht-labels">Checking if it's </span>  <i class="fas fa-bicycle" style="color:#00add6;"></i></span><span> weather</span>
</p>


<body onload=display_ct();> 
<span id='ct' ></span>

</body>



<script>
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("temperature").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/temperature", true);
  xhttp.send();
}, 3000 ) ;                                               // change this to like 10000 to update screen every 10 seconds

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("humidity").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/humidity", true);
  xhttp.send();
}, 3000 ) ;                                                  // change this to like 10000 to update screen every 10 seconds

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("dewpoint").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/dewpoint", true);
  xhttp.send();
}, 3000 ) ;                                                  // change this to like 10000 to update screen every 10 seconds

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("heatindex").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/heatindex", true);
  xhttp.send();
}, 3000 ) ;                                                 // change this to like 10000 to update screen every 10 seconds

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("tempsensor").innerHTML = this.responseText;
    }  
  };
  xhttp.open("GET", "/tempsensor", true);
  xhttp.send();
}, 3000 ) ;                                                 // change this to like 10000 to update screen every 10 seconds

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("humidsensor").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/humidsensor", true);
  xhttp.send();
}, 3000 ) ;  

</script>

</html>)rawliteral";

// *************************************  end of web page *****************************************



// Replaces placeholder with DHT values
String processor(const String& var) {

  if (var == "TEMPERATURE_PH") {
    return String(t);
  }
  else if (var == "HUMIDITY_PH") {
    return String(h);
  }
  else if (var == "DEWPOINT_PH") {
    return String(DP);
  }
  else if (var == "HEATINDEX_PH") {
    return String(dhi);
  }
  else if (var == "TEMPSENSOR_PH") {
    return String(tempsensorworking);
  }
  else if (var == "HUMIDSENSOR_PH") {
    return String(humidsensorworking);
  }
  return String();
}

// ********************* end of web page **************************************************************

void setup() {
  



  Serial.begin(115200);                                           // Serial port for debugging purposes

  pinMode(4, OUTPUT);                                             // this pin could be used to control a relay to turn say a fan on and off

  Wire.begin(SDA, SCL);                                           // (sda,scl) Start the Wire service for the OLED display using pin 14 for SCL and Pin 2 for SDA

  dht.begin();                                                    //start the temp and humid sensor

  display.init();                                                 // Initialise the display
  display.flipScreenVertically();                                 // In my case flip the screen around by 180°
  display.setContrast(20);                                        // If you want turn the display contrast down, 255 is maxium and 0 in minimum, in practice about 20 is OK
  display.setFont(ArialMT_Plain_10);                              // Set the Font size 10 is the smallest
  WiFiStart();
                                                                  // Now configure time services
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  setenv("TZ", Timezone, 1);
  delay(1000);                                                    // Wait for time services
                                                                  //Now setup a timer interrupt to occur every 1-second, to keep seconds accurate
  Setup_Interrupts_and_Initialise_Clock();


                                                                           // all the text is just static it does not get over written *********

                                                                           // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", index_html, processor);
  });
  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(t).c_str());
  });
  server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(h).c_str());
  });
  server.on("/dewpoint", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(DP).c_str());
  });
  server.on("/heatindex", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(dhi).c_str());
  });
  server.on("/tempsensor", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(tempsensorworking).c_str());
  });
  server.on("/humidsensor", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(humidsensorworking).c_str());
  });
                  
  server.begin();                                                                         // Start server

}

// ************************   MAIN LOOP   ************************************* 

void loop() {

beginagain:                                                         // i used a GOTO function to return to this point if the sensor fails.  
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >=  interval) {
    // save the last time you updated the DHT values
    previousMillis = currentMillis;
    // Read temperature as Celsius (the default)
    float newT = dht.readTemperature();
    // Read temperature as Fahrenheit (isFahrenheit = true)
    //float newT = dht.readTemperature(true);
    // if temperature read failed, do not update value

    if (isnan(newT)) {
      Serial.println("Failed to read from DHT sensor!");
      tempsensorworking = ("Failed");                               // send this "Failed" text to the webserver later
    }
    else {
    
      t = newT;
      tempsensorworking = "Working";                                // send this "Working" text to the webserver later
      Serial.print("Temp: ");
      Serial.print(t);
      Serial.print("   ");
}

    // Read Humidity
    // Reading temperature for humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (it's a very slow sensor)
    float newH = dht.readHumidity();
    // if humidity read failed,  do not update value
    if (isnan(newH)) {

      Serial.println("Failed to read from DHT sensor!");
      humidsensorworking = "Failed";                                  // send this "Failed" text to the webserver later
    }
    else {
      h = newH;
      humidsensorworking = "Working";                               // send this "Working" text to the webserver later
      Serial.print("Humid:  ");
      Serial.print(h);
      Serial.print("  ");
    }

    //float calcDewpoint(float temp, float rh);
    {
      //calculate the DEWPOINT
      //TD: =243.04*(LN(h/100)+((17.625*t)/(243.04+t)))/(17.625-LN(h/100)-((17.625*t)/(243.04+t)))
      DP = 243.04 * (log(h / 100) + ((17.625 * t) / (243.04 + t))) / (17.625 - log(h / 100) - ((17.625 * t) / (243.04 + t)));

      Serial.print("Dew Point:  ");
      Serial.print(DP);
      Serial.print("  ");

      dhi = dht.computeHeatIndex(t, h, false);
      Serial.print("Heat Index:  ");
      Serial.print(dhi);
      Serial.println(" ");

    }
    {
      //read temperature and humidity
      float t = dht.readTemperature();
      float h = dht.readHumidity();
      float dhi = dht.computeHeatIndex(t, h, false);
      if (isnan(h) || isnan(t)) {
      Serial.println("Failed to read from DHT sensor!");        //IF NO READING CAN BE FOUND PRINT ON THE SERIAL MONITOR YOU WILL ALSO GET "nan" ON THE OLED
                                                                // but instaed of the values showing "nan" i clear the OLED and print a message
      }
    }   
  }

  UpdateLocalTime();                                                          // and update the time and date display

  display.clear();                                                            // clear the display


  if (tempsensorworking == "Failed" || humidsensorworking=="Failed") {        // lets clear the OLED if one or both readings from the sensor fails and print a new message
    display.drawString (50, 0, "ERROR");
    display.drawString (11, 16, "Check the DHT Sensor");
    display.display();
    goto beginagain;                                                          // ******  people say not to use the GOTO command ********
  }


  display.clear();                                                            // clear the display and get ready to print something
  

  Serial.println(Time_str + "  " + Date_str);                                  // send time and date to the serial monitor
  delay(100);

  // display Temp
  display.drawString(0, 3, "Temp:");                                          //POSITION WHERE TO SHOW THE TEXT
  display.drawString(60, 3, String(t));                                       //PRINT THE VALUE STORED IN THE VARIABLE "t"
  display.drawString(90, 3, "°C");

  // display humidity
  display.drawString(0, 13, "Humidity:");
  display.drawString(60, 13, String(h));                                      //PRINT THE VALUE STORED IN THE VARIABLE "h"
  display.drawString(87, 13, " %");

  // display DewPoint
  display.drawString(0, 23, "Dew Point:");
  display.drawString(60, 23, String(DP));                                     //PRINT THE VALUE STORED IN THE VARIABLE "h"
  display.drawString(87, 23, " %");

  // display Heat Index
  display.drawString(0, 33, "Heat Index:");
  display.drawString(60, 33, String(dhi));                                   //PRINT THE VALUE STORED IN THE VARIABLE "dhi"
  display.drawString(90, 33, "°C");

  display.drawString(0, 54, Time_str);                                       // print the time day and date
  display.drawString(56, 54, Date_str);

  
  display.display();                                                         // send all of the above to the OLED
                                                                                                                                                    
  if (h > 65) digitalWrite(4, LOW);                                          //  turn on the onboard led if the humidity is over 65%  
  else digitalWrite(4, HIGH);                                                //  we are using pin 4 it control a realy to say turn a fan on/off

}

//////////////////////   end of loop    /////////////////////////////////////////////

void WiFiStart() {

  /* Set the ESP to be a WiFi-client, otherwise by default, it acts as ss both a client and an access-point
      and can cause network-issues with other WiFi-devices on your WiFi-network. */

  WiFi.mode(WIFI_STA);
  Serial.print(F("\r\nConnecting to: ")); Serial.println(String(ssid));
  WiFi.begin(myssid, mypassword);
  while (WiFi.status() != WL_CONNECTED ) {
    delay(500);
    Serial.print(F("."));
  }
  Serial.println("WiFi connected to address: ");
  Serial.println(WiFi.localIP());
  delay(3000);
}

// *******************************************************************************

void UpdateLocalTime() {
  time_t now;
  if (local_Unix_time > next_update_due) { // only get a time synchronisation from the NTP server every duration for update set
    time(&now);
    Serial.println("Synchronising local time, time error was: " + String(now - local_Unix_time));
    // If this displays a negative result the interrupt clock is running fast or positive running slow
    local_Unix_time = now;
    next_update_due = local_Unix_time + update_duration;
  } else now = local_Unix_time;
  //See http://www.cplusplus.com/reference/ctime/strftime/
  char hour_output[30], day_output[30];
  strftime(day_output, 30, "%a  %d-%m-%y", localtime(&now)); // Formats date as: Sat 24-Jun-17
  strftime(hour_output, 30, "%T", localtime(&now));    // Formats time as: 14:05:49
  Date_str = day_output;
  Time_str = hour_output;
}

//#########################################################################################

// Interrupt setup and local Unix Time initialisation
#ifdef ESP8266
void Setup_Interrupts_and_Initialise_Clock() {
  noInterrupts();
  timer0_isr_init();
  timer0_attachInterrupt(timer0_ISR);
  timer0_write(ESP.getCycleCount() + 80000000L); // 80MHz == 1sec, some devices might be slower or faster than 80MHz, adjust for absolute time accuracy if required.
  interrupts();
  //Now get current Unix time and assign the value to local Unix time counter and start the clock.
  time_t now;
  time(&now);
  local_Unix_time = now + 2; // The addition of 2 counters the NTP setup time delay
  next_update_due = local_Unix_time + update_duration;
}

//#########################################################################################

// Interrupt service routine
void timer0_ISR (void) { // Every second come here to increment the local Unix time counter
  local_Unix_time++;
  timer0_write(ESP.getCycleCount() + 80000000L); // 80MHz == 1sec
}
#else
volatile int interruptCounter;
int totalInterruptCounter;

hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  local_Unix_time++;
  portEXIT_CRITICAL_ISR(&timerMux);
}

void Setup_Interrupts_and_Initialise_Clock() {
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 1000000, true);
  timerAlarmEnable(timer);
  //Now get current Unix time and assign the value to local Unix time counter and start the clock.
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.println(F("Failed to obtain time"));
  }
  time_t now;
  time(&now);
  local_Unix_time = now + 2; // The addition of 2 counters the NTP setup time delay
  next_update_due = local_Unix_time + update_duration;
}
#endif
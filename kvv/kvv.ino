/*
   kvv.ino - Till Harbaum <till@harbaum.org>

   Sketch for ESP8266 driving a 2.9" tri color eink display.

   Displaying the departure information for KVV tram lines in Karlsruhe.
   Specify your preferred station id below and setup your WIFI credentials
   and the device will update the eink display whenever powered up and go
   to deep sleep / power save after that. Simply press reset whenever you
   want to have the display updated. 
*/

// stop IDs can be found in the JSON reply for e.g. Pionierstraße using this request in a regular browser:
// https://www.kvv.de/tunnelEfaDirect.php?action=XSLT_STOPFINDER_REQUEST&name_sf=pionierstraße&outputFormat=JSON&type_sf=any
#define STOP_ID  "7000238"  // Pionierstraße
// #define STOP_ID  "7001004"    // Europaplatz/Postgalerie

// number of departures to be requested
#define LIMIT "6"   // the epaper display can display six text lines

// make sure there's a wifi.h with the following contents:
// #define WIFI_SSID "<your ssid>"
// #define WIFI_PASSWORD "<your password>"
#include "wifi.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Adafruit_ThinkInk.h>
#include <time.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

#include <ESP8266HTTPClient.h>

#include <WiFiClientSecureBearSSL.h>

// Eink pins: DC = D0, CS=D3, BUSY=D2, SRAM_CS=-1, RESET=D1
ThinkInk_290_Tricolor_Z10 display(D0, D1, D3, -1, D2);

ESP8266WiFiMulti WiFiMulti;

#define TOP 15
#define SKIP 18
#define COL0_WIDTH 28
#include "FreeSansBold9pt8b.h"

// ****** UTF8-Decoder: convert UTF8-string to extended ASCII *******

// Convert a single Character from UTF8 to Extended ASCII
// Return "0" if a byte has to be ignored
byte utf8ascii(byte ascii) {
  static byte c1;  // Last character buffer
  
  if ( ascii<128 ) {  // Standard ASCII-set 0..0x7F handling  
    c1=0;
    return( ascii );
  }

  // get previous input
  byte last = c1;   // get last char
  c1=ascii;         // remember actual character

  switch (last) {  // conversion depending on first UTF8-character
    case 0xC2: return  (ascii);  break;
    case 0xC3: return  (ascii | 0xC0);  break;
    case 0x82: if(ascii==0xAC) return(0x80);       // special case Euro-symbol
  }

  return  (0);                                     // otherwise: return zero, if character has to be ignored
}

// convert String object from UTF8 String to Extended ASCII
String utf8ascii(String s) {      
  String r="";
  char c;
  for (int i=0; i<s.length(); i++) {
    c = utf8ascii(s.charAt(i));
    if (c!=0) r+=c;
  }
  return r;
}

uint16_t get_dsp_length(String str) {
  int16_t  x, y;
  uint16_t w, h;

  display.getTextBounds(utf8ascii(str), 0, 0, &x, &y, &w, &h);
  return w;
}

void parse_time(struct tm *timeinfo, const JsonObject &obj) {
  memset(timeinfo, 0, sizeof(struct tm));
  if(obj.containsKey("year")) timeinfo->tm_year = atoi(obj["year"]) - 1900;
  if(obj.containsKey("month")) timeinfo->tm_mon = atoi(obj["month"]) - 1;
  if(obj.containsKey("day")) timeinfo->tm_mday = atoi(obj["day"]);
  if(obj.containsKey("hour")) timeinfo->tm_hour = atoi(obj["hour"]);
  if(obj.containsKey("minute")) timeinfo->tm_min = atoi(obj["minute"]);
}

// void parse_reply(String payload) {
void parse_reply(Stream &payload) {
  int16_t  x, y;
  uint16_t w, h;

  DynamicJsonDocument doc(4096);
          
  // install filters to extract only the required information from the stream
  StaticJsonDocument<300> filter;
  filter["dateTime"] = true;
  filter["dm"]["points"]["point"]["name"] = true;      // station name
  filter["departureList"][0]["countdown"] = true;
  filter["departureList"][0]["realDateTime"]["hour"] = true;
  filter["departureList"][0]["realDateTime"]["minute"] = true;
  filter["departureList"][0]["dateTime"]["hour"] = true;
  filter["departureList"][0]["dateTime"]["minute"] = true;
  filter["departureList"][0]["servingLine"]["direction"] = true;
  filter["departureList"][0]["servingLine"]["symbol"] = true;

  // start parsing
  DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter)); 
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }

  JsonObject obj = doc.as<JsonObject>();

  // get stop name
  const char *stopName = obj["dm"]["points"]["point"]["name"];  
  // find last comma in string and cut at the first non-space afterwards
  // to get rid of city name
  const char *c = stopName;
  for(int i=0; i < strlen(c);i++)
    if(c[i] == ',') {
      for(i++;c[i]==' ';i++);
      stopName = c+i;
    }

  // parse time from reply into timeinfo
  struct tm timeinfo;
  parse_time(&timeinfo, obj["dateTime"]);

  // convert to time_t for time span calculation
  time_t now = mktime(&timeinfo);

  // one would usually use strftime, but that adds leading 0's to the
  // month and hour which we don't want to save space
  // max length of timestring is "DD.MM.YY HH:mm" -> 15 Bytes incl \0-term 
  char timeStamp[15];
  sprintf(timeStamp, "%d.%d.%02d %d:%02d", 
    timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year-100,
    timeinfo.tm_hour, timeinfo.tm_min);
  
  Serial.printf("Name: %s\n", stopName);
  Serial.printf("Timestamp: %s\n", timeStamp);

  display.begin(THINKINK_TRICOLOR);
  display.setFont(&FreeSansBold9pt8b);
  display.clearBuffer();
  display.setTextSize(1);

  // top row has a black background
  display.fillRect(0, 0, 296, SKIP, EPD_BLACK);

  // station name in white
  display.setTextColor(EPD_WHITE);
  display.setCursor(COL0_WIDTH+2, TOP);
  display.print(utf8ascii(stopName));

  // time stamp is right aligned
  display.getTextBounds(timeStamp, 0, 0, &x, &y, &w, &h);
  display.setTextColor(EPD_WHITE);
  display.setCursor(295-w-2, TOP);
  display.print(timeStamp);
  
  // TODO: Delayed trains are listed first even if their real departure time
  // is later then other trains. We'd like to display them in their real
  // departure order instead.
  for(int i=0;i<obj["departureList"].size();i++) {  
    JsonObject nobj = obj["departureList"][i];     // i'th object in departure list 

    // some directions include redirects of the form "> next line", like e.g.
    // "Rheinbergstraße > 75 Bruchweg"
    // This is too long for the small display, so we get rid of everything
    // after the ">"
    const char *direction = nobj["servingLine"]["direction"];
    char *c = (char*)direction;
    while(*c && *c != '>') c++;  // search for '>'
    if(*c) {                     // '>' was found
      c--;                       // skip before '>'
      while(*c == ' ') c--;      // skip back over whitespaces
      c[1] = '\0';               // terminate string after last non-space
    }   

    // further (previus) direction/destination handlng requires a String object
    String destination(direction);
    
    const char *route = nobj["servingLine"]["symbol"];
    
    // countdown are the minutes to go
    int countdown = atoi(nobj["countdown"]);

    // get readDateTime if present, otherwise dateTime
    struct tm deptime;
    if(nobj.containsKey("realDateTime")) parse_time(&deptime, nobj["realDateTime"]);
    else                                 parse_time(&deptime, nobj["dateTime"]);

    // create nice time string
    char time[8];
    if(countdown == 0)       strcpy(time, "sofort");
    else if(countdown < 10)  sprintf(time, "%d min", countdown);
    else                     sprintf(time, "%d:%02d", deptime.tm_hour, deptime.tm_min);      

    Serial.printf("[%s] %s %s\n", route, direction, time);
  
    // up to 6  entries fit onto the screen
    if(i < 6) {
      uint16_t w, dw;

      // route column has red background
      display.fillRect(0, TOP+SKIP*i+5, COL0_WIDTH, SKIP-2, EPD_RED);

      // center route in first column
      display.setTextColor(EPD_WHITE);
      w = get_dsp_length(route);
      display.setCursor((COL0_WIDTH-w)/2-1, TOP+SKIP+SKIP*i);
      display.print(route);

      // left align destination
      display.setTextColor(EPD_BLACK, EPD_WHITE);
      dw = get_dsp_length(destination);
      w = get_dsp_length(time);

      // check if destination fits left of time or if it
      // needs to be truncated
      if(COL0_WIDTH+2+dw >= 295-w-2) {
        // destination needs to be truncated.

        // append elipsis (...) and recalculate disdplay width
        destination.concat("...");
        dw = get_dsp_length(destination);

        // truncate until destination fits
        while(COL0_WIDTH+2+dw >= 295-w-2) {             
          destination = destination.substring(0, destination.length()-4);
          destination.concat("...");
          dw = get_dsp_length(destination);
        }        
      }
      
      display.setCursor(COL0_WIDTH+2, TOP+SKIP+SKIP*i);
      display.print(utf8ascii(destination));

      // display time right aligned
      display.setTextColor(EPD_RED, EPD_WHITE);
      display.setCursor(295-w-2, TOP+SKIP+SKIP*i);
      display.print(time);
    }
  }  
  display.display();  
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(115200);
  // Serial.setDebugOutput(true);

  Serial.println();
  Serial.println();
  Serial.println();

  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  // wait for WiFi link up
  while((WiFiMulti.run() != WL_CONNECTED)) {
    Serial.print('.');
    digitalWrite(LED_BUILTIN, HIGH);  // led off
    delay(50);
    digitalWrite(LED_BUILTIN, LOW);   // led on
    delay(50);
  }

  std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);

  client->setInsecure();

  HTTPClient https;

  Serial.print("[HTTPS] begin...\n");
  if (https.begin(*client, "https://projekte.kvv-efa.de/sl3-alone/XSLT_DM_REQUEST?"
        "outputFormat=JSON&coordOutputFormat=WGS84[dd.ddddd]&depType=stopEvents&"
        "locationServerActive=1&mode=direct&name_dm=" STOP_ID "&type_dm=stop&"
        "useOnlyStops=1&useRealtime=1&limit=" LIMIT)) {

    Serial.print("[HTTPS] GET...");
    // start connection and send HTTP header
    int httpCode = https.GET();

    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf(" code: %d\n", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
        parse_reply(https.getStream());
        
    } else
      Serial.printf(" failed, error: %s\n", https.errorToString(httpCode).c_str());

    https.end();
  } else
    Serial.printf("[HTTPS] Unable to connect\n");
    
  Serial.println("Going to sleep ...");
  ESP.deepSleep(0); 
}

void loop() {
  Serial.printf("loop() should never be reached!\n");
  delay(1000);
}

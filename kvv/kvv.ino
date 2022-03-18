/*
   kvv.ino - Till Harbaum <till@harbaum.org>

   Sketch for ESP8266 driving a 2.9" tri color eink display.

   Displaying the departure information for KVV tram lines in Karlsruhe.
   Specify your preferred station id below and setup your WIFI credentials
   and the device will update the eink display whenever powered up and go
   to deep sleep / power save after that. Simply press reset whenever you
   want to have the display updated. 
*/

// station ID as used at https://live.kvv.de/
#define STOP_ID  "PIO"  // Pionierstraße
// #define STOP_ID  "EPO"    // Europaplatz/Postgalerie

#define WIFI_SSID "********"
#define WIFI_PASSWORD "**********"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Adafruit_ThinkInk.h>

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

void parse_reply(String payload) {
  int16_t  x, y;
  uint16_t w, h;
  
  DynamicJsonDocument doc(2048);
          
  // start parsing
  DeserializationError error = deserializeJson(doc, payload); 
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }

  display.begin(THINKINK_TRICOLOR);
  display.setFont(&FreeSansBold9pt8b);
  display.clearBuffer();
  display.setTextSize(1);

  JsonObject obj = doc.as<JsonObject>();

  String stopName = obj[String("stopName")];
  String timeStamp = obj[String("timestamp")];

  // reformat time string from YYYY-MM-DD HH:MM:SS to a narrower
  // (D)D.(M)M.YY (H)H:MM
  timeStamp = timeStamp.substring((timeStamp.charAt(8)=='0')?9:8,10) + "." +
              timeStamp.substring((timeStamp.charAt(5)=='0')?6:5,7) + "." +
              timeStamp.substring(2,4) + " " +
              timeStamp.substring((timeStamp.charAt(11)=='0')?12:11,16);

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
  
  for(int i=0;i<obj[String("departures")].size();i++) {  
    JsonObject nobj = obj[String("departures")][i];
    String route = nobj[String("route")];
    String destination = nobj[String("destination")];
    String time = nobj[String("time")];
    if(time == "0") time = "sofort";

    // other (yet unused) fields:
    // traction: integer, number of carriages. 0=1 carriage, 1=1, 2=2, ...
    // lowfloor: true, use wheelchair icon
    // realtime: so far, only "true" has been observed
    // direction: always 1 or 2 for forth and back

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
  if (https.begin(*client, "https://live.kvv.de/webapp/departures/bystop/" STOP_ID
      "?maxInfos=10"
      "&key=377d840e54b59adbe53608ba1aad70e8")) {  // HTTPS

    Serial.print("[HTTPS] GET...");
    // start connection and send HTTP header
    int httpCode = https.GET();

    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf(" code: %d\n", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
        parse_reply(https.getString());
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

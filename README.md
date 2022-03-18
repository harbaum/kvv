# KVV

We recently (Nov 2020) got a new tram extension in Karlsruhe, Germany.
To celebrate the fact that there's now a station pretty close to our
home I decided to build a virtual timetable. And since I had some
spare ESP8266 powered Wemos D1 and a 2.9" tricolor e-paper display I
decided to use these.

![Final setup](doc/kvv_final.jpg)

## Breadboard setup

The whole setup is pretty simple and can be wired on a breadboard as
depicted.

![Wiring](doc/kvv.png)

Since the epaper display keeps the data even if not powered at all the
ESP8266 simply boots up, connects to the (hardcoded) WiFi network,
downloads the live data, updates the display and then powers down/goes
into deep sleep. The whole setup can thus be run from a battery with
the reset button exposed so it can be pushed to boot the ESP8266 and 
to update the display.

![Breadboard setup](doc/kvv_bb.jpg)

## Final setup

For the final setup depicted above I unsoldered the connector on the
displays rear and used double sided tape to stick the Wemos D1 to the
display. A total of eight short wires are needed to connect both
PCBs. The resulting setup is sufficiently small and robust.

![Final setup rear view](doc/kvv_final_rear.jpg)

## Where does the data come from?

The KVV offers a [live timetable](http://live.kvv.de). A quick look
at the network traffic (or at the embedded Javascript code) reveals that
some easy to handle JSON can be downloaded e.g. for the tram stop "Pionierstraße" using [this http get request](https://live.kvv.de/webapp/departures/bystop/PIO?maxInfos=10&key=377d840e54b59adbe53608ba1aad70e8).

The station ID of the station to be displayed is hardcoded into the
sketch. By default this is set to PIO which is the ID of the station
named Pionierstraße. The API allows to search for IDs by station names
and even parts of it. E.g. to search for any station including
"Pionier" in its name use the following search:
[https://live.kvv.de/webapp/stops/byname/Pionier?key=377d840e54b59adbe53608ba1aad70e8](https://live.kvv.de/webapp/stops/byname/Pionier?key=377d840e54b59adbe53608ba1aad70e8). For
other stations replace "Pionier" with the phrase you are searching
for. The station IDs can then be read from the JSON reply.

## The sketch

The sketch is based on the
[BasicHttpsClient.ino](https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266HTTPClient/examples/BasicHttpsClient/BasicHttpsClient.ino)
for the ESP8266. The display is driven using the [Adafruit EPD
library](https://github.com/adafruit/Adafruit_EPD). And finally the
JSON parsing is done using the [ArduinoJson
library](https://github.com/bblanchon/ArduinoJson). All these
dependencies can be installed from the library manager within the
Arduino IDE.

A custom generated font is being used since the fonts distributed by
Adafruit with the Adafruit GFX library only include 7 bit fonts and
we need an 8 bit font for the german umlauts used by the KVV.

## Related projects

- [Python bindings for the KVV live API](https://github.com/Nervengift/kvvliveapi)
- [MMM-KVV - Magic Mirror KVV display](https://github.com/yo-less/MMM-KVV)

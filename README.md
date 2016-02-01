ESP8266 Sonos RFID Controller
==============

**This project WIP.**

This ESP8266 firmware allows you to control your Sonos Wireless Speakers by assigning operations to RFID tokens. This way you can switch playlists by placing a new RFID token on the reader.
It will automatically detect your Sonos speaker using SSDP once the ESP8266 connects to your network. Currently it does not support zone topologies, but might be supported in the future.

Setup
-----------------
* Get the Arduino ESP8266 firmware. For install instructions please check [here](https://github.com/esp8266/Arduino).
* Install the Arduino [RFID library](https://github.com/miguelbalboa/rfid)
* Install the [yxml library](https://dev.yorhel.nl/yxml) by placing both the .h and .c files in a new [Arduino library folder](https://www.arduino.cc/en/Guide/Libraries).

Diagram
----------------
TODO

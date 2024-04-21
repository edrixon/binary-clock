An NTP synchronised binary clock for the MKR1000 or Adafruit ESP32-S3

Time is displayed in BCD on some LEDs with a multiplexed display.
An hourly chime or time can be sent in morse code.
Configured by booting board in access point mode and using web browser.
In normal use, board connects to WiFi and synchronises to external NTP server.
Can see status using built-in webserver.  ESP32 version web page continually updates
showing state of clock and time as displayed on the LEDs.

Both versions support a command interpretter on the USB serial port for configuration, status
and (for ESP32 version) file management.

ESP32-S3 version supports a FAT filesystem for its webserver and configuration data.
Software can be updated over WiFi using the Arduino IDE.
Uses mDNS to help finding it on the network.
Individual files can be updated using FTP - note only one connection allowed at a time, windows CLI FTP client doesn't work.

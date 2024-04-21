//
//  Binary clock for either MKR1000 or Adafruit ESP32-S3 Feather boards
//    uses multiplexed LED display directly connected to GPIO lines
//    Time (12h/24h mode) or date can be displayed using BCD
//    keeps time using Network Time Protocol (NTP) over WiFi
//    WiFi configuration by built-in wireless access point and web interface or serial port
//    Can send time in morse code and have an hourly morse chime
//    Some things can be set at compile time in config.h
//    User configuration done by putting clock into configuration mode which starts an access point and a webserver
//    Connect to that on http://192.168.1.1 to complete setup
//

#include "config.h"

#include <NTPClient.h>
#ifdef __MK1_HW
#include <WiFi101.h>
#include <FlashStorage.h>
#else
#include <WiFi.h>
#include <FS.h>
#include <FFat.h>
#include <ESPmDNS.h>
#include <SimpleFTPServer.h>
#endif
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <Timezone.h>

#ifdef __WITH_OTA
#include <ArduinoOTA.h>
#endif

#define __IN_MAIN

#include "types.h"
#include "telnet.h"
#include "webserver.h"
#include "cli.h"
#include "globals.h"
#include "morse.h"

#ifdef __MK1_HW

#include "Timer5.h"
FlashStorage(savedConfig, eepromData);

#else

hw_timer_t *timer0 = NULL;

#ifdef __WITH_FTP
FtpServer ftpSrv;
#endif

#endif

#ifdef __WITH_TELNET
WiFiServer telnetServer(TELNET_PORT);
#endif

// State machine
clockStateType clockState;

// NTP things
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Timezone things
TimeChangeRule ukBST = { "BST", Last, Sun, Mar, 2, 60 };
TimeChangeRule ukGMT = { "GMT", Last, Sun, Oct, 2, 0 };
Timezone ukTime(ukGMT, ukBST);

int ntpUpdates;

int tickTime;

boolean chimedAlready;

unsigned long int lastMillis;

int ledState;

void defaultClockConfig()
{
    strcpy(clockConfig.ssid, DEFAULT_SSID);
    strcpy(clockConfig.password, DEFAULT_PSWD);
    strcpy(clockConfig.ntpServer, DEFAULT_NTPS);
    strcpy(clockConfig.hostName, DEFAULT_HOSTNAME);
    strcpy(clockConfig.ftpUser, DEFAULT_FTPUSER);
    strcpy(clockConfig.ftpPassword, DEFAULT_FTPPSWD);
    clockConfig.initUpdate = INIT_UPDATE;
    clockConfig.syncUpdate = SYNC_UPDATE;
    clockConfig.syncValid = SYNC_VALID;  
}

boolean initClockConfig()
{
    Serial.print("No valid configuration");

#ifdef __USE_DEFAULTS

    Serial.println(" - loading defaults");
    defaultClockConfig();
    return true;
        
#else

    Serial.println("");
    memset(&clockConfig, 0, sizeof(clockConfig));

    return false;

#endif
}

// Interrupt handler for display timer
#ifdef __MK1_HW
void displayInterrupt()
#else
void IRAM_ATTR displayInterrupt()
#endif
{
    int c;
    int tmpDisplay;

    interruptCount++;

    digitalWrite(ledColPins[ledColSelect], LOW);

    ledColSelect++;
    if(ledColSelect == MAX_COLS)
    { 
        ledColSelect = 0;
    }

    tmpDisplay = ledColData[ledColSelect];

    for(c = 0; c < ledColSize[ledColSelect]; c++)
    {
        if((tmpDisplay & 0x01) != 0)
        {
            digitalWrite(ledRowPins[c], HIGH);
        }
        else
        {
            digitalWrite(ledRowPins[c], LOW);
        }

        tmpDisplay = tmpDisplay >> 1;
    }

    digitalWrite(ledColPins[ledColSelect], HIGH);
}

#ifdef __MK1_HW

void syncLed(int state)
{
    digitalWrite(PIN_SYNCLED, state);
    ntpSyncState = state;
}

void initDisplayTimer()
{
    interruptCount = 0;
    MyTimer5.begin(DISP_INTS_PER_SECOND);
    MyTimer5.attachInterrupt(displayInterrupt);
    MyTimer5.start();
}

boolean getClockConfig()
{
    clockConfig = savedConfig.read();
    if(clockConfig.eepromValid == EEPROM_VALID)
    {
        return true;
    }
    else
    {
        return initClockConfig(); 
    }
}

void saveClockConfig()
{
    clockConfig.eepromValid = EEPROM_VALID;
    savedConfig.write(clockConfig);
}

#else

void syncLed(int state)
{
    ntpSyncState = state;
}

void initDisplayTimer()
{
    interruptCount = 0;
    
    timer0 = timerBegin(0, TIMER0_PRESCALE, true);
    timerAttachInterrupt(timer0, &displayInterrupt, true);
    timerAlarmWrite(timer0, TIMER0_RELOAD, true);
    timerAlarmEnable(timer0);
}

boolean getClockConfig()
{
    File fp;
    
    clockConfig.eepromValid = ~EEPROM_VALID;

    // Read the config file
    fp = FFat.open(CONFIG_FILENAME, FILE_READ);
    if(!fp)
    {
        Serial.println("Error opening configuration file for reading");
    }
    else
    {
        fp.read((unsigned char *)&clockConfig, sizeof(clockConfig));
        fp.close();
    }
  
    if(clockConfig.eepromValid == EEPROM_VALID)
    {
        return true;
    }
    else
    {
        return initClockConfig(); 
    }
}

void saveClockConfig()
{
    File fp;

    fp = FFat.open(CONFIG_FILENAME, FILE_WRITE);
    if(!fp)
    {
        Serial.println("Error opening configuration file for writing");
        return;
    }

    clockConfig.eepromValid = EEPROM_VALID;

    // Write the config file
    fp.write((unsigned char *)&clockConfig, sizeof(clockConfig));
    fp.close();
}

// Do something obvious to prove board has reset properly
// Take long enough to swap com: ports for serial monitor
// in case board has just been programmed using boot0 button
void initDelay()
{
    // Blue
    neopixelWrite(PIN_NEOPIXEL, RGB_OFF, RGB_OFF, RGB_VAL);
    delay(1000);

    // Green
    neopixelWrite(PIN_NEOPIXEL, RGB_OFF, RGB_VAL, RGB_OFF);
    delay(1000);

    // Red
    neopixelWrite(PIN_NEOPIXEL, RGB_VAL, RGB_OFF, RGB_OFF);
    delay(1000);

    // Off
    neopixelWrite(PIN_NEOPIXEL, RGB_OFF, RGB_OFF, RGB_OFF);
    delay(1000);

    // Red
    neopixelWrite(PIN_NEOPIXEL, RGB_VAL, RGB_OFF, RGB_OFF);
}

void initMDNS()
{
    if(MDNS.begin(clockConfig.hostName))
    {
        Serial.printf(" - mDNS responder (%s)\r\n", clockConfig.hostName);
    }
    else
    {
        Serial.println(" - error setting up mDNS responder");      
    }
}

#ifdef __WITH_OTA

void initArduinoOTA()
{
    Serial.printf(" - ArduinoOTA (%s, port %d)\r\n", clockConfig.hostName, OTA_PORT);
  
    ArduinoOTA.setHostname(clockConfig.hostName);
    ArduinoOTA.setPort(OTA_PORT);

    ArduinoOTA
     .onStart([]()
     {
        String type;
        if(ArduinoOTA.getCommand() == U_FLASH)
        {
            type = "sketch";
        }
        else
        {
            type = "filesystem";
        }

        FFat.end();
        Serial.println("Start updating " + type);
    })
    
    .onEnd([]()
    {
        Serial.println("");
        Serial.println("Update finished");
    })
    
    .onProgress([](unsigned int progress, unsigned int total)
    {
        unsigned long int pcent;

        pcent = progress / (total / 100);
        if(pcent % 10 == 0)
        {
            Serial.printf("Progress: %u%%\r\n", pcent);
        }
    })
    
    .onError([](ota_error_t error)
    {
        Serial.printf("Error[%u]: ", error);
        switch(error)
        {
            case OTA_AUTH_ERROR:
                Serial.println("Auth failed");
                break;
                
            case OTA_BEGIN_ERROR:
                Serial.println("Begin failed");
                break;
                
            case OTA_CONNECT_ERROR:
                Serial.println("Connect failed");
                break;
                
            case OTA_RECEIVE_ERROR:
                Serial.println("Receive failed");
                break;
                
            case OTA_END_ERROR:
                Serial.println("End failed");
                break;

            default:
                Serial.println("Unknown error!");
        }
    });

    ArduinoOTA.begin();
}

#endif

#ifdef __WITH_FTP

void ftpSrvCallback(FtpOperation ftpOperation, unsigned int freeSpace, unsigned int totalSpace){
  switch (ftpOperation) {
    case FTP_CONNECT:
      Serial.println(F("FTP: Connected!"));
      break;
    case FTP_DISCONNECT:
      Serial.println(F("FTP: Disconnected!"));
      break;
    case FTP_FREE_SPACE_CHANGE:
      Serial.printf("FTP: Free space change, free %u of %u!\n", freeSpace, totalSpace);
      break;
    default:
      break;
  }
};

void ftpSrvTransferCallback(FtpTransferOperation ftpOperation, const char* name, unsigned int transferredSize){
  switch (ftpOperation) {
    case FTP_UPLOAD_START:
      Serial.println(F("FTP: Upload start!"));
      break;
    case FTP_UPLOAD:
      Serial.printf("FTP: Upload of file %s byte %u\n", name, transferredSize);
      break;
    case FTP_TRANSFER_STOP:
      Serial.println(F("FTP: Finish transfer!"));
      break;
    case FTP_TRANSFER_ERROR:
      Serial.println(F("FTP: Transfer error!"));
      break;
    default:
      break;
  }

  /* FTP_UPLOAD_START = 0,
   * FTP_UPLOAD = 1,
   *
   * FTP_DOWNLOAD_START = 2,
   * FTP_DOWNLOAD = 3,
   *
   * FTP_TRANSFER_STOP = 4,
   * FTP_DOWNLOAD_STOP = 4,
   * FTP_UPLOAD_STOP = 4,
   *
   * FTP_TRANSFER_ERROR = 5,
   * FTP_DOWNLOAD_ERROR = 5,
   * FTP_UPLOAD_ERROR = 5
   */
};

void initFtpServer()
{
    Serial.println(" - FTP server");
    
    ftpSrv.setCallback(ftpSrvCallback);
    ftpSrv.setTransferCallback(ftpSrvTransferCallback);

    ftpSrv.begin(clockConfig.ftpUser, clockConfig.ftpPassword);
}

#endif

#endif

// Initialise display
void initDisplay()
{
    int c;

    for(c = 0; c < MAX_ROWS; c++)
    {
        pinMode(ledRowPins[c], OUTPUT);
        digitalWrite(ledRowPins[c], LOW);
    }

    for(c = 0; c < MAX_COLS; c++)
    {
        ledColData[c] = 0;
        pinMode(ledColPins[c], OUTPUT);
        digitalWrite(ledColPins[c], LOW);
    }

    ledColSelect = 0;
}

int splitTime(time_t epoch, timeNow_t *timeStruct)
{
    int rtn;

    rtn = timeStruct -> tm_mday;
    timeStruct -> tm_mday = day(epoch);
    timeStruct -> tm_mon = month(epoch);
    timeStruct -> tm_year = year(epoch);
    timeStruct -> tm_wday = weekday(epoch) - 1;
    timeStruct -> tm_hour = hour(epoch);
    timeStruct -> tm_min = minute(epoch);
    timeStruct -> tm_sec = second(epoch);

    return rtn;
}

void ledShowTime(timeNow_t *timeStruct)
{
    int tmpDisp[2];
    int hourNow;

#ifdef __MK2_HW
    int c;

    if(digitalRead(PIN_BOOT) == LOW)
    {
        for(c = 0; c < MAX_COLS; c++)
        {
            ledColData[c] = 0x0f;
        }
    }
    else
    {

#endif

        hourNow = timeStruct -> tm_hour;
        
        // use tmpDisp to stop flickering NTP-sync and am/pm lights
        if(mode12() == true && hourNow >= 12)
        {
            hourNow = hourNow - 12;

            splitDigit(hourNow, &tmpDisp[0]);
        
            // most significant hour bit is used to drive AM/PM indicator
            tmpDisp[0] = tmpDisp[0] | BIT_AMPM;
        }
        else
        {
            splitDigit(hourNow, &tmpDisp[0]);
        }

#ifdef __MK2_HW
        if(ntpSyncState == HIGH)
        { 
            tmpDisp[0] = tmpDisp[0] | BIT_SYNCLED; 
        }
#endif

        ledColData[0] = tmpDisp[0];
        ledColData[1] = tmpDisp[1];
        splitDigit(timeStruct -> tm_min, &ledColData[2]);
        splitDigit(timeStruct -> tm_sec, &ledColData[4]);     

#ifdef __MK2_HW
    }
#endif
}

void ledShowDate(timeNow_t *timeStruct)
{
    splitDigit(timeStruct -> tm_mday, &ledColData[0]);
    ledColData[2] = 0;
    ledColData[3] = 0;
    splitDigit(timeStruct -> tm_mon, &ledColData[4]);
}

#ifdef __TEST_DISPLAY

void testDisplay()
{
      int c;
      int d;


      for(c = 0; c < MAX_COLS; c++)
      {
          Serial.print("Column: ");
          Serial.println(ledColPins[c]);
          digitalWrite(ledColPins[c], HIGH);

          for(d = 0; d < ledColSize[c]; d++)
          {
              Serial.print("Row: ");
              Serial.println(ledRowPins[d]);
              digitalWrite(ledRowPins[d], HIGH);

              delay(500);

              digitalWrite(ledRowPins[d], LOW);
          }

          digitalWrite(ledColPins[c], LOW);

          delay(500);
      }

#ifdef __MK1_HW
      digitalWrite(syncLedPin, HIGH);
      delay(500);
      digitalWrite(syncLedPin, LOW);
      delay(500);
#endif
}

#endif

void initDisplayPattern()
{
    int c;

    c = 0;
    while(c < MAX_COLS)
    {
        ledColData[c] = 0x0f;
        delay(100);
        ledColData[c] = 0;

        c++;
    }

    c = c - 2;
    while(c >= 0)
    {
        ledColData[c] = 0x0f;
        delay(100);
        ledColData[c] = 0;

        c--;      
    }

#ifdef __MK1_HW
    for(c = 0; c < 3; c++)
    {
        digitalWrite(PIN_SYNCLED, HIGH);
        delay(100);
        digitalWrite(PIN_SYNCLED, LOW);
        delay(100);
    }
#endif
}

boolean tickTimeExpired()
{
    if((millis() - lastMillis) >= tickTime)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void resetTickTime()
{
    lastMillis = millis();
}

boolean updateClock()
{
    boolean rtn;

    rtn = false;
    
    if(WiFi.status() == WL_CONNECTED)
    { 
        ntpUpdates++;
        reachability = reachability << 1;
        Serial.print("[UPDT] Sending NTP update time request - ");
        Serial.println(ntpUpdates);

        // forceUpdate() times out after 1second if there's no response
        if(timeClient.forceUpdate() == true)
        {
            Serial.println("[UPDT] NTP response received");
            reachability = reachability | 0x01;

            if(ntpUpdates == clockConfig.syncValid)
            {
                syncLed(HIGH);
                updateTime = clockConfig.syncUpdate;
            }

            rtn = true;
        }
        else
        {
            Serial.println("[UPDT] Timedout waiting for NTP response");
        }
    }
    else
    {
        Serial.println("[UPDT] WiFi has disconnected");
    }

    return rtn;
}

void correctTime()
{
    time_t epoch;
    TimeChangeRule *tcr;
    int dayOfMonth;

    // get time from NTP library
    // seconds since start of time
    epoch = timeClient.getEpochTime();
 
    // convert to uk time with bst or gmt
    epoch = ukTime.toLocal(epoch, &tcr);
    dayOfMonth = splitTime(epoch, &timeNow);
            
    // Reset reSyncCount when day changes
    if(dayOfMonth != timeNow.tm_mday)
    {
        reSyncCount = 0;
    }

    // send everything to serial port
    serialShowTime(&timeNow, tcr -> abbrev);
}

boolean handleButtons()
{
    boolean rtn;

    rtn = true;
    // If button pressed, send the time in morse code
    if(digitalRead(PIN_MORSETIME) == LOW)
    {
        Serial.print("[MRSE] ");
        if(digitalRead(PIN_DATETIME) == LOW)
        {
            Serial.println("Sending IP address");
            ipAddressInMorse();
        }
        else
        {
            Serial.println("Sending time in morse");
            timeInMorse();
        }
    }
    else
    {
        if(digitalRead(PIN_DATETIME) == LOW)
        {
            Serial.println("[DATE] Show date");
            ledShowDate(&timeNow);                
        }
        else
        {
            rtn = false;
        }
    }

    return rtn;
}

void hourlyChime()
{
    // If enabled, chime hour in morse code, once, on the hour
    if(timeNow.tm_min == 0 && chimesEnabled() == true)
    {
        if(chimedAlready == false)
        {
            Serial.println("[CHME] Hourly chime");
            chimeMorse();
            chimedAlready = true;
        }
    }
    else
    {
        chimedAlready = false;
    }
}

void startNtpClient()
{
    Serial.print(" - NTP client started with server ");
    Serial.println(clockConfig.ntpServer);
    timeClient.setPoolServerName(clockConfig.ntpServer);
    timeClient.begin(NTP_PORT);
    updateTime = clockConfig.initUpdate;
    ntpUpdates = 0;
}

#ifdef __WITH_TELNET
void startTelnetServer()
{
    Serial.print(" - Telnet server started, listening on port ");
    Serial.println(TELNET_PORT);
    telnetServer.begin();
}
#endif

#ifdef __WITH_HTTP
void startWebserver()
{
    Serial.print(" - Webserver started, listening on port ");
    Serial.println(HTTP_PORT);
    httpServer.begin();
}
#endif

void wifiConnected()
{
    char rssiStr[16];

    sprintf(rssiStr, "%d dBm", WiFi.RSSI());
    
    Serial.print(" - WiFi connected (ch ");
    Serial.print(WiFi.channel());
    Serial.print(", rssi ");
    Serial.print(rssiStr);
    Serial.print(", ip address ");
    Serial.print(WiFi.localIP());
    Serial.println(")");

#ifdef __MK1_HW
    digitalWrite(LED_BUILTIN, HIGH);
#else
    neopixelWrite(PIN_NEOPIXEL, RGB_OFF, RGB_VAL, RGB_OFF);
#endif
}
              
void setup()
{
    // Serial port
    Serial.begin(9600);

#ifdef __MK2_HW

    // Disable I2C pullup resistors on ESP32 board
    pinMode(PIN_I2C_POWER, INPUT);

    Serial.println("12345678901234567890");
    initDelay();

#else

    delay(5000);

#endif
  
    Serial.println();
    Serial.println(HELLO_STR);
    Serial.println("");
    Serial.print("Software built for ");
#ifdef __MK1_HW
    Serial.println("MK1 hardware (MKR1000)");
#else
    Serial.println("MK2 hardware (Adafruit ESP32-S3)");
    Serial.printf("ESP32 Chip model %s, revision %d, %d cores\r\n", ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores());
    
    if(!FFat.begin(true))
    {
        Serial.println(" - Error mounting FAT filesystem");
    }
    else
    {
        Serial.printf(" - Mounted FAT filesystem (%ld bytes free)\r\n", FFat.freeBytes());
    }
#endif

    Serial.println(" - GPIO");

    // GPIO
#ifdef __MK1_HW
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(PIN_SYNCLED, OUTPUT);
#else

    // The "boot" button on the ESP32 board
    pinMode(PIN_BOOT, INPUT_PULLUP);
    
#endif
    pinMode(PIN_BEEP, OUTPUT);
    pinMode(PIN_CHIME, INPUT_PULLUP);
    pinMode(PIN_MODESEL, INPUT_PULLUP);
    pinMode(PIN_DATETIME, INPUT_PULLUP);
    pinMode(PIN_MORSETIME, INPUT_PULLUP);

    syncLed(LOW);
    digitalWrite(PIN_BEEP, LOW);
#ifdef __MK1_HW
    digitalWrite(LED_BUILTIN, LOW);
#endif

    Serial.println(" - initDisplay()");
    // LED display
    initDisplay();

    Serial.println(" - getClockConfig()");
    // Read configuration from flash memory
    // Will use defaults if nothing found and compiled with __USE_DEFAULTS set
    // if __USE_DEFAULTS is not defined, will start the command interpretter instead
    while(getClockConfig() == false)
    {
        // If there's no configuration, display a "X" and start command line
        // Keep on until there's some configuration to use...

        ledColData[0] = 0;
        ledColData[1] = 0;
        ledColData[2] = 5;
        ledColData[3] = 2;
        ledColData[4] = 5;
              
        Serial.println("No configuration found in FLASH!!");

        commandInterpretter();

        ledColData[0] = 0;
        ledColData[1] = 0;
        ledColData[2] = 0;
        ledColData[3] = 0;
        ledColData[4] = 0;      
    }

    // State machine
    clockState = STATE_INIT;

    tickTime = TICKTIME;
    lastMillis = millis();
    
    reSyncCount = 0;
    reachability = 0;
    chimedAlready = false;

    Serial.println(" - initDisplayTimer()");
    // Display interrupt and handler
    initDisplayTimer();
    
    Serial.println(" - initDisplayPattern()");
    // Display a pattern
    initDisplayPattern();

    Serial.println(" - morseBeep()");
    morseBeep(MORSE_DELAY);
#ifdef __TEST_DISPLAY

    while(1)
    {
        testDisplay();
    }

#endif

    Serial.println("*******************");
    Serial.println("***  R E A D Y  ***");
    Serial.println("*******************");
    Serial.println("");

}

void loop()
{
#ifdef __WITH_TELNET
    WiFiClient telnetClient;
#endif

    switch(clockState)
    {
        case STATE_INIT:
            Serial.println("*************************");
            Serial.println("***  S T A R T I N G  ***");
            Serial.println("*************************");
#ifdef __WITH_HTTP
            if(digitalRead(PIN_MORSETIME) == LOW || digitalRead(PIN_DATETIME) == LOW)
            {
#ifdef __MK2_HW
                neopixelWrite(PIN_NEOPIXEL, RGB_VAL, RGB_OFF, RGB_VAL);
#endif
                Serial.println(" - Web configuration mode");
                httpStartAP();
                httpWebServer();
#ifdef __MK1_HW
                WiFi.end();
#endif
            }
            else
            {
                Serial.println(" - Started timing mode");
#endif

                Serial.print(" - Trying to connect WiFi to ssid '");
                Serial.print(clockConfig.ssid);
                Serial.print("' . ");
#ifdef __MK2_HW
                WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
                WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
#endif
                WiFi.begin(clockConfig.ssid, clockConfig.password);
#ifdef __MK1_HW
                ledState = LOW;
                digitalWrite(LED_BUILTIN, ledState);
#else
                WiFi.mode(WIFI_STA);
                ledState = RGB_VAL;
                neopixelWrite(PIN_NEOPIXEL, RGB_OFF, RGB_OFF, ledState);
#endif
                syncLed(LOW);
                clockState = STATE_CONNECTING;
                reSyncCount++;
                loggedIn = false;
#ifdef __WITH_HTTP
            }
#endif
            resetTickTime();
            break;

        case STATE_CONNECTING:
            if(WiFi.status() == WL_CONNECTED)
            {
                Serial.println("ok");
                
                clockState = STATE_TIMING;

                wifiConnected();
                
#ifdef __MK2_HW
                initMDNS();
#ifdef __WITH_OTA
                initArduinoOTA();
#endif
#ifdef __WITH_FTP
                initFtpServer();
#endif
#endif

                ticks = 0;
                startNtpClient();
                
#ifdef __WITH_TELNET
                startTelnetServer();
#endif

#ifdef __WITH_HTTP
                startWebserver();
#endif
            }
            else
            {
                if(tickTimeExpired() == true)
                {
#ifdef __MK1_HW
                    if(ledState == HIGH)
                    {
                        ledState = LOW;
                    }
                    else
                    
                    {
                        ledState = HIGH;
                    }
                    digitalWrite(LED_BUILTIN, ledState);
#else                    
                    if(ledState == RGB_OFF)
                    {
                        ledState = RGB_VAL;
                    }
                    else
                    {
                        ledState = RGB_OFF;
                    }
                    neopixelWrite(PIN_NEOPIXEL, RGB_OFF, RGB_OFF, ledState);               
#endif
                    
                    Serial.print(". ");
                    resetTickTime();
                }
            }
            break;

        case STATE_TIMING:
            // Periodically force an NTP update
            // use forceUpdate() to keep track of whether NTP is still working
            // allows sync LED to be lit and "reachability" to be updated
            if(tickTimeExpired() == true)
            {
                if(ticks == 0)
                {
                    // Send NTP time request
                    if(updateClock() == true)
                    {
                        ticks = updateTime - 1;
                    }
                    else
                    {
                        clockState = STATE_STOPPED;
                    }                    
                }
                else
                {
                    ticks--;
                }

                // Apply daylight saving
                correctTime();
                
                lastMillis = millis();
            }
                
            if(handleButtons() == false)
            {
                ledShowTime(&timeNow);              
            }

            hourlyChime();
            
#ifdef __WITH_TELNET
            // If someone's connected with telnet, deal with it
            telnetClient = telnetServer.available();
            if(telnetClient.connected())
            {
                telnetShowStatus(telnetClient);
                telnetClient.flush();
                telnetClient.stop();
            }
#endif

#ifdef __WITH_HTTP

            // If someone's connected to webserver, deal with it
            httpClient = httpServer.available();
            if(httpClient.connected())
            {
                httpRequestHandler();
                httpClient.flush();
                httpClient.stop();
            }
#endif
            break;
 
        case STATE_STOPPED:
            Serial.println("***********************");
            Serial.println("***  S T O P P E D  ***");
            Serial.println("***********************");
            timeClient.end();
#ifdef __WITH_TELNET
            telnetServer.end();
#endif
#ifdef __WITH_HTTP
            httpServer.end();
#endif
#ifdef __MK1_HW
            WiFi.end();
            digitalWrite(LED_BUILTIN, LOW);
#else
            WiFi.disconnect();
#endif
            clockState = STATE_INIT;
            break;

       default:
            clockState = STATE_STOPPED;
    }

    // If someone's plugged the serial cable in and pressed a key
    // Enter CLI
    // Restart state machine when exiting CLI
    if(Serial.available() > 0)
    {
        commandInterpretter();
        clockState = STATE_STOPPED;
    }

#ifdef __WITH_FTP
    ftpSrv.handleFTP();
#endif

#ifdef __WITH_OTA

    ArduinoOTA.handle();

#endif
}

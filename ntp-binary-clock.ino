//
//                 A0, A1, A2, A3, A4, A5    --  Display column select lines (hh mm ss)
//                 A6,  0,  1,  2            --  Display LED select lines (lsb first)
//                  3                        --  Sync LED (low = initialising, high = NTP is stable)
//                  4                        --  Mode select input (low = 12 hour mode, high = 24 hour mode)
//                  5                        --  Show date or time input (low = date, high = time)
//                  6                        --  WiFi connected (built-in led lit if connected)
//                  7                        --  Beeper for hourly chimes in morse code
//                  8                        --  Hourly chimes input (low = no chimes, high = hourly morse code chime)
//                  9                        --  Time-in-morse input (low = send time in morse code)
//

#include <NTPClient.h>
#include <WiFi101.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <Timezone.h>
#include <FlashStorage.h>


#define __IN_MAIN

#include "config.h"
#include "types.h"
#include "telnet.h"
#include "webserver.h"
#include "cli.h"
#include "globals.h"
#include "morse.h"

#include "Timer5.h"

#ifdef __WITH_TELNET
WiFiServer ws(23);
#endif

FlashStorage(savedConfig, eepromData);

// State machine
clockStateType clockState;

// NTP things
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Timezone things
TimeChangeRule ukBST = { "BST", Last, Sun, Mar, 2, 60 };
TimeChangeRule ukGMT = { "GMT", Last, Sun, Oct, 2, 0 };
Timezone ukTime(ukGMT, ukBST);

int ntpUpdates;        // need 20 re-sync's before time is "stable"

int tickTime;

int dayOfMonth;

boolean chime;

unsigned long int lastMillis;

// Initialise display
void initDisplay()
{
    int c;

    for(c = 0; c < MAX_ROWS; c++)
    {
        pinMode(ledDisplay.rowDataPins[c], OUTPUT);
        digitalWrite(ledDisplay.rowDataPins[c], LOW);
    }

    for(c = 0; c < MAX_COLS; c++)
    {
        ledDisplay.data[c] = 0;
        pinMode(ledDisplay.colSelectPins[c], OUTPUT);
        digitalWrite(ledDisplay.colSelectPins[c], LOW);
    }

    ledDisplay.colSelect = 0;
}

// Interrupt handler for display timer
void displayInterrupt()
{
    int c;
    int tmpDisplay;

    interruptCount++;

    digitalWrite(ledDisplay.colSelectPins[ledDisplay.colSelect], LOW);
    
    ledDisplay.colSelect++;
    if(ledDisplay.colSelect == MAX_COLS)
    {
        ledDisplay.colSelect = 0;
    }

    tmpDisplay = ledDisplay.data[ledDisplay.colSelect];

    for(c = 0; c < ledDisplay.colDataSize[ledDisplay.colSelect]; c++)
    {
        if((tmpDisplay & 0x01) != 0)
        {
            digitalWrite(ledDisplay.rowDataPins[c], HIGH);
        }
        else
        {
            digitalWrite(ledDisplay.rowDataPins[c], LOW);
        }

        tmpDisplay = tmpDisplay >> 1;
    }

    digitalWrite(ledDisplay.colSelectPins[ledDisplay.colSelect], HIGH);
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
    if(digitalRead(modeSelectPin) == LOW && timeStruct -> tm_hour >= 12)
    {
        timeStruct -> tm_hour = timeStruct -> tm_hour - 12;

        splitDigit(timeStruct -> tm_hour, &ledDisplay.data[0]);
        
        // most significant hour bit is used to drive AM/PM indicator
        ledDisplay.data[0] = ledDisplay.data[0] | 0x08;
    }
    else
    {
        splitDigit(timeStruct -> tm_hour, &ledDisplay.data[0]);
    }

    splitDigit(timeStruct -> tm_min, &ledDisplay.data[2]);
    splitDigit(timeStruct -> tm_sec, &ledDisplay.data[4]);     
}

void ledShowDate(timeNow_t *timeStruct)
{
    splitDigit(timeStruct -> tm_mday, &ledDisplay.data[0]);
    ledDisplay.data[2] = 0;
    ledDisplay.data[3] = 0;
    splitDigit(timeStruct -> tm_mon, &ledDisplay.data[4]);
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
#ifdef __USE_DEFAULTS
        Serial.println("No configuration found - loading defaults");
        strcpy(clockConfig.ssid, DEFAULT_SSID);
        strcpy(clockConfig.password, DEFAULT_PSWD);
        strcpy(clockConfig.ntpServer, DEFAULT_NTPS);

        return true;
#else
        memset(&clockConfig, 0, sizeof(clockConfig));

        return false;
#endif        
    }
}

void saveClockConfig()
{
    clockConfig.eepromValid = EEPROM_VALID;
    savedConfig.write(clockConfig);
}

#ifdef __TEST_DISPLAY

void testDisplay()
{
      int c;
      int d;

      for(c = 0; c < MAX_COLS; c++)
      {
          Serial.print("Column: ");
          Serial.println(ledDisplay.colSelectPins[c]);
          digitalWrite(ledDisplay.colSelectPins[c], HIGH);

          for(d = 0; d < ledDisplay.colDataSize[c]; d++)
          {
              Serial.print("Row: ");
              Serial.println(ledDisplay.rowDataPins[d]);
              digitalWrite(ledDisplay.rowDataPins[d], HIGH);

              delay(500);

              digitalWrite(ledDisplay.rowDataPins[d], LOW);
          }

          digitalWrite(ledDisplay.colSelectPins[c], LOW);

          delay(500);
      }

      digitalWrite(syncLedPin, HIGH);
      delay(500);
      digitalWrite(syncLedPin, LOW);
      delay(500);

}

#endif

void initDisplayPattern()
{
    int c;

    c = 0;
    while(c < MAX_COLS)
    {
        ledDisplay.data[c] = 0x0f;
        delay(100);
        ledDisplay.data[c] = 0;

        c++;
    }

    c = c - 2;
    while(c >= 0)
    {
        ledDisplay.data[c] = 0x0f;
        delay(100);
        ledDisplay.data[c] = 0;

        c--;      
    }

    for(c = 0; c < 3; c++)
    {
        digitalWrite(syncLedPin, HIGH);
        delay(100);
        digitalWrite(syncLedPin, LOW);
        delay(100);
    }
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

void setup()
{
    // Serial port
    Serial.begin(9600);
//    while(!Serial);
    delay(5000);
    Serial.println();
    Serial.println(HELLO_STR);
    Serial.println("");

    // GPIO
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(syncLedPin, OUTPUT);
    pinMode(chimePin, OUTPUT);
    pinMode(disChimePin, INPUT_PULLUP);
    pinMode(modeSelectPin, INPUT_PULLUP);
    pinMode(dateTimePin, INPUT_PULLUP);
    pinMode(morseTimePin, INPUT_PULLUP);

    digitalWrite(syncLedPin, LOW);
    digitalWrite(chimePin, LOW);
    digitalWrite(LED_BUILTIN, LOW);

    // LED display
    initDisplay();

    // Display interrupt and handler
    interruptCount = 0;
    MyTimer5.begin(DISP_INTS_PER_SECOND);
    MyTimer5.attachInterrupt(displayInterrupt);
    MyTimer5.start();

    // Display a pattern
    initDisplayPattern();

    // One beep
    digitalWrite(chimePin, HIGH);
    delay(MORSE_DELAY);
    digitalWrite(chimePin, LOW);

#ifdef __TEST_DISPLAY

    while(1)
    {
        testDisplay();
    }

#endif

    // Read configuration from flash memory
    // Will use defaults if nothing found and compiled with __USE_DEFAULTS set
    while(getClockConfig() == false)
    {
        // If there's no configuration, display a "X" and start command line
        // Keep on until there's some configuration to use...
      
        ledDisplay.data[2] = 5;
        ledDisplay.data[3] = 2;
        ledDisplay.data[4] = 5;
              
        Serial.println("No configuration found in FLASH!!");

        cli();

        ledDisplay.data[2] = 0;
        ledDisplay.data[3] = 0;
        ledDisplay.data[4] = 0;      
    }
    
    // State machine
    clockState = STATE_INIT;

    lastMillis = millis();
    
    reSyncCount = 0;
    reachability = 0;
    chime = true;
}

void loop()
{
    time_t epoch;
    TimeChangeRule *tcr;
#ifdef __WITH_TELNET
    WiFiClient wc;
#endif

    switch(clockState)
    {
        case STATE_INIT:
            Serial.println("*** Starting  ***");
#ifdef __WITH_HTTP
            if(digitalRead(morseTimePin) == LOW || digitalRead(dateTimePin) == LOW)
            {
                Serial.println(" - Web configuration mode");
                httpStartAP();
                httpWebServer();
                WiFi.end();
            }
            else
            {
                Serial.println(" - Normal operation");
#endif
                WiFi.begin(clockConfig.ssid, clockConfig.password);
                digitalWrite(LED_BUILTIN, LOW);
                digitalWrite(syncLedPin, LOW);
                clockState = STATE_CONNECTING;
                tickTime = INIT_TICKTIME;
                reSyncCount++;
#ifdef __WITH_HTTP
            }
#endif
            lastMillis = millis();
            break;

        case STATE_CONNECTING:
            if(WiFi.status() == WL_CONNECTED)
            {
                Serial.println(" - Wifi connected");
                printWifiStatus();
                digitalWrite(LED_BUILTIN, HIGH);
                Serial.print(" - Starting NTP - ");
                Serial.println(clockConfig.ntpServer);
                timeClient.setPoolServerName(clockConfig.ntpServer);
                timeClient.setUpdateInterval(SYNC_UPDATE * 1000);
                timeClient.begin(NTP_PORT);
                clockState = STATE_TIMING;
                updateTime = INIT_UPDATE;
                ntpUpdates = 0;
                ticks = 0;
#ifdef __WITH_TELNET
                ws.begin();
                Serial.println(" - Telnet started");
#endif
#ifdef __WITH_HTTP
                httpServer.begin();
                Serial.println(" - Webserver started");
                
                // Only whilst testing...
                // httpWebServer();
#endif
            }
            else
            {
                if(tickTimeExpired() == true)
                {
                    Serial.print(". ");
                    lastMillis = millis();
                }
            }
            break;

        case STATE_TIMING:
            // Periodically, force an NTP update
            // use forceUpdate() to keep track of whether NTP is still working
            // allows sync LED to be lit and "reachability" to be updated
            if(tickTimeExpired() == true)
            {
                if(ticks == 0)
                {              
                    if(WiFi.status() == WL_CONNECTED)
                    { 
                        reachability = reachability << 1;
                        if(timeClient.forceUpdate() == true)
                        {
                            Serial.println("  <NTP update>");
                            ticks = updateTime - 1;
                            reachability = reachability | 0x01;
                            if(ntpUpdates == 20)
                            {
                                digitalWrite(syncLedPin, HIGH);
                                updateTime = SYNC_UPDATE;
                                tickTime = SYNC_TICKTIME;
                            }
                      
                            ntpUpdates++;
                        }
                        else
                        {
                            Serial.println("  <NTP timeout>");
                            clockState = STATE_STOPPED;
                        }
                    }
                    else
                    {
                        Serial.println("  <Wifi disconnected>");
                        clockState = STATE_STOPPED;
                    }
                }
                else
                {
                    ticks--;
                }

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
 
                lastMillis = millis();
            }  

            // load display data
            // with date or time depending on if the date/time button is pressed or not
            if(digitalRead(dateTimePin) == HIGH)
            {
                ledShowTime(&timeNow);
            }
            else
            {
                Serial.println("Show date");
                ledShowDate(&timeNow);
            }

            // If enabled, chime hour in morse code, once, on the hour
            if(timeNow.tm_min == 0 && digitalRead(disChimePin) == HIGH)
            {
                if(chime == true)
                {
                    chimeMorse();
                    chime = false;
                }
            }
            else
            {
                chime = true;
            }
                
            // If button pressed, send the time in morse code
            if(digitalRead(morseTimePin) == LOW)
            {
                timeInMorse();
            }

#ifdef __WITH_TELNET
            wc = ws.available();
            if(wc.connected())
            {
                telnetShowStatus(wc);
                wc.flush();
                wc.stop();
            }
#endif

#ifdef __WITH_HTTP
            // web page for normal operation
            httpClient = httpServer.available();
            if(httpClient.connected())
            {
                httpStatusGetRequest();
                httpClient.flush();
                httpClient.stop();
            }
#endif
            break;

        case STATE_STOPPED:
            Serial.println("*** Stopped");
            timeClient.end();
            WiFi.end();
            digitalWrite(LED_BUILTIN, LOW);
            digitalWrite(syncLedPin, LOW);
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
        cli();
        clockState = STATE_STOPPED;
    }
}

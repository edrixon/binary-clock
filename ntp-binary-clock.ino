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
#include <FFat.h>
#endif
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <Timezone.h>

#define __IN_MAIN

#include "types.h"
#include "telnet.h"
#include "webserver.h"
#include "cli.h"
#include "globals.h"
#include "morse.h"

#ifdef __MK1_HW
#include "Timer5.h"
#else
hw_timer_t *timer0 = NULL;
#endif

#ifdef __WITH_TELNET
WiFiServer ws(23);
#endif

#ifdef __MK1_HW
FlashStorage(savedConfig, eepromData);
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

int dayOfMonth;

boolean chime;

unsigned long int lastMillis;

void defaultClockConfig()
{
    strcpy(clockConfig.ssid, DEFAULT_SSID);
    strcpy(clockConfig.password, DEFAULT_PSWD);
    strcpy(clockConfig.ntpServer, DEFAULT_NTPS);
    clockConfig.initUpdate = INIT_UPDATE;
    clockConfig.syncUpdate = SYNC_UPDATE;
    clockConfig.syncValid = SYNC_VALID;  
}

boolean initClockConfig()
{
#ifdef __USE_DEFAULTS

        Serial.println("No configuration found - loading defaults");
        defaultClockConfig();
        return true;
#else
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
    
    timer0 = timerBegin(0, 80, true);
    timerAttachInterrupt(timer0, &displayInterrupt, true);
    timerAlarmWrite(timer0, DISP_INT_100US * 100, true);
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

#endif

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
#ifdef __MK2_HW

    int c;

    if(digitalRead(PIN_BOOT) == LOW)
    {
        for(c = 0; c < MAX_COLS; c++)
        {
            ledDisplay.data[c] = 0x0f;
        }
    }
    else
    {

#endif

        if(digitalRead(PIN_MODESEL) == LOW && timeStruct -> tm_hour >= 12)
        {
            timeStruct -> tm_hour = timeStruct -> tm_hour - 12;

            splitDigit(timeStruct -> tm_hour, &ledDisplay.data[0]);
        
            // most significant hour bit is used to drive AM/PM indicator
            ledDisplay.data[0] = ledDisplay.data[0] | BIT_AMPM;
        }
        else
        {
            splitDigit(timeStruct -> tm_hour, &ledDisplay.data[0]);
        }

#ifdef __MK2_HW
        if(ntpSyncState == HIGH)
        { 
            ledDisplay.data[0] = ledDisplay.data[0] | BIT_SYNCLED; 
        }
#endif

        splitDigit(timeStruct -> tm_min, &ledDisplay.data[2]);
        splitDigit(timeStruct -> tm_sec, &ledDisplay.data[4]);     

#ifdef __MK2_HW
    }
#endif
}

void ledShowDate(timeNow_t *timeStruct)
{
    splitDigit(timeStruct -> tm_mday, &ledDisplay.data[0]);
    ledDisplay.data[2] = 0;
    ledDisplay.data[3] = 0;
    splitDigit(timeStruct -> tm_mon, &ledDisplay.data[4]);
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

    Serial.println(" - initDisplayTimer()");
    // Display interrupt and handler
    initDisplayTimer();

    Serial.println(" - initDisplayPattern()");
    // Display a pattern
    initDisplayPattern();

    Serial.println(" - One beep");
    // One beep
    digitalWrite(PIN_BEEP, HIGH);
    delay(MORSE_DELAY);
    digitalWrite(PIN_BEEP, LOW);

#ifdef __TEST_DISPLAY

    while(1)
    {
        testDisplay();
    }

#endif

    Serial.println(" - getClockConfig()");
    // Read configuration from flash memory
    // Will use defaults if nothing found and compiled with __USE_DEFAULTS set
    // if __USE_DEFAULTS is not defined, will start the command interpretter instead
    while(getClockConfig() == false)
    {
        // If there's no configuration, display a "X" and start command line
        // Keep on until there's some configuration to use...
      
        ledDisplay.data[2] = 5;
        ledDisplay.data[3] = 2;
        ledDisplay.data[4] = 5;
              
        Serial.println("No configuration found in FLASH!!");

        commandInterpretter();

        ledDisplay.data[2] = 0;
        ledDisplay.data[3] = 0;
        ledDisplay.data[4] = 0;      
    }
    
    // State machine
    clockState = STATE_INIT;

    tickTime = TICKTIME;
    lastMillis = millis();
    
    reSyncCount = 0;
    reachability = 0;
    chime = true;

    Serial.println("");
    Serial.println("******* Showtime !!!");
    Serial.println("");

}

void loop()
{
    int ledState;
    time_t epoch;
    TimeChangeRule *tcr;
#ifdef __WITH_TELNET
    WiFiClient wc;
#endif

    switch(clockState)
    {
        case STATE_INIT:
            Serial.println("***  Starting  ***");
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
                Serial.println(" - Timing mode");
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
#ifdef __WITH_HTTP
            }
#endif
            resetTickTime();
            break;

        case STATE_CONNECTING:
            if(WiFi.status() == WL_CONNECTED)
            {
                Serial.println(" - Wifi connected");
                printWifiStatus();
#ifdef __MK1_HW
                digitalWrite(LED_BUILTIN, HIGH);
#else
                neopixelWrite(PIN_NEOPIXEL, RGB_OFF, RGB_VAL, RGB_OFF);
#endif
                Serial.print(" - Starting NTP - ");
                Serial.println(clockConfig.ntpServer);
                timeClient.setPoolServerName(clockConfig.ntpServer);
                timeClient.setUpdateInterval(clockConfig.syncUpdate * 1000);
                timeClient.begin(NTP_PORT);
                clockState = STATE_TIMING;
                updateTime = clockConfig.initUpdate;
                ntpUpdates = 0;
                ticks = 0;
#ifdef __WITH_TELNET
                ws.begin();
                Serial.println(" - Telnet server started");
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
                    if(WiFi.status() == WL_CONNECTED)
                    { 
                        reachability = reachability << 1;
                        if(timeClient.forceUpdate() == true)
                        {
                            Serial.println("  <NTP update>");
                            ticks = updateTime - 1;
                            reachability = reachability | 0x01;
                            ntpUpdates++;

                            if(ntpUpdates == clockConfig.syncValid)
                            {
                                syncLed(HIGH);
                                updateTime = clockConfig.syncUpdate;
                            }
                      
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

            // If button pressed, send the time in morse code
            if(digitalRead(PIN_MORSETIME) == LOW)
            {
                if(digitalRead(PIN_DATETIME) == LOW)
                {
                    Serial.println("  <IP address in morse>");
                    ipAddressInMorse();
                }
                else
                {
                    Serial.println("  <Time in morse>");
                    timeInMorse();
                }
            }
            else
            {
                if(digitalRead(PIN_DATETIME) == LOW)
                {
                    Serial.println("  <Show date>");
                    ledShowDate(&timeNow);                
                }
                else
                {
                    ledShowTime(&timeNow);
                }
            }

            // If enabled, chime hour in morse code, once, on the hour
            if(timeNow.tm_min == 0 && chimesEnabled() == true)
            {
                if(chime == true)
                {
                    Serial.println("  <Hourly chime>");
                    chimeMorse();
                    chime = false;
                }
            }
            else
            {
                chime = true;
            }
                
#ifdef __WITH_TELNET
            // If someone's connected with telnet, deal with it
            wc = ws.available();
            if(wc.connected())
            {
                telnetShowStatus(wc);
                wc.flush();
                wc.stop();
            }
#endif

#ifdef __WITH_HTTP
            // If someone's connected to webserver, deal with it
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
#ifdef __MK1_HW
            WiFi.end();
            digitalWrite(LED_BUILTIN, LOW);
#else
            WiFi.disconnect();
#endif
            syncLed(LOW);
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
}

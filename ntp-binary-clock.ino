//
//                 A0, A1, A2, A3, A4, A5    --  Display column select lines (hh mm ss)
//                 A6,  0,  1,  2            --  Display LED select lines (lsb first)
//                  5                        --  Show date or time input (low = date, high = time)
//                  4                        --  Mode select input (low = 12 hour mode, high = 24 hour mode)
//                  3                        --  Sync LED (lit if NTP is stable)
//                  6                        --  WiFi connected (built-in led lit if connected)
//

#include <NTPClient.h>
#include <WiFi101.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <Timezone.h>
#include <FlashStorage.h>

#include "Timer5.h"

//#define __TEST_DISPLAY            // Just do display test and nothing else...
#define __USE_DEFAULTS             // Load default configuration if nothing else already stored.  Otherwise, boot to CLI for user configuration

#define HELLO_STR       "** Ed's NTP clock **"

#define DISP_INTS_PER_SECOND 400  // Display timer interrupts per second 

#define EEPROM_VALID    0xdeadbeef

#define NTP_PORT        123       // local port number
#define SYNC_UPDATE     600       // number of seconds between NTP polls once stable
#define INIT_UPDATE     1         // initial time between NTP polls 

#define SYNC_TICKTIME   1000      // ms tick time for normal operation of state machine
#define INIT_TICKTIME   500       // tick time whilst stabilising

#define SERBUFF_LEN     80        // Command line buffer size

#define MAX_COLS        6         // Number of columns in display      
#define MAX_ROWS        4         // Number of rows in display

#ifdef __USE_DEFAULTS

// Default values for configuration if nothing found at startup
#define DEFAULT_SSID    "ballacurmudgeon"
#define DEFAULT_PSWD    "scaramanga"
#define DEFAULT_NTPS    "192.168.1.251"

#endif

// CLI function prototypes
void cmdClearConfig(void);
void cmdSaveConfig(void);
void cmdGetConfig(void);
void cmdSsid(void);
void cmdPassword(void);
void cmdNtpServer(void);
void cmdListCommands(void);
void cmdShowState(void);
void cmdDisplay(void);

// CLI command
typedef struct
{
    char *cmdName;
    void (*fn)(void);
} cmdType;

// Somewhere to store the time and date
typedef struct
{
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
} timeNow_t;

// Configuration data
typedef struct
{
    char ssid[32];
    char password[32];
    char ntpServer[32];
    unsigned long int eepromValid;
} eepromData;

// State machine states
typedef enum clockState_e
{
    STATE_INIT,
    STATE_CONNECTING,
    STATE_CONNECTED,
    STATE_TIMING,
    STATE_STOPPED
} clockStateType;

// LED display
typedef struct
{
    int colSelectPins[MAX_COLS];
    int colDataSize[MAX_COLS];
    int rowDataPins[MAX_ROWS];
    int data[MAX_COLS];
    int colSelect;
} ledDisplay_t;

const char *dayStrings[] =
{
    "Sunday",
    "Monday",
    "Tuesday",
    "Wednesday",
    "Thursday",
    "Friday",
    "Saturday"
};

eepromData clockConfig;
FlashStorage(savedConfig, eepromData);

ledDisplay_t ledDisplay =
{
    { A0, A1, A2, A3, A4, A5 },                 // column select pins h-h-m-m-s-s
    { 4,  4,  3,  4,  3,  4 },                  // number of leds on this column hh:mm:ss
    { A6, 0, 1, 2 },                            // data pins          lsb first
    { 0, 0, 0, 0, 0, 0 },                       // display data
    0,                                          // current column selection
};

int dateTimePin = 5;                            // Ground to show date instead of time
int modeSelectPin = 4;                          // 12 or 24hr mode
int syncLedPin = 3;                             // LED lights when sync'd

// State machine
clockStateType clockState;

// NTP things
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
unsigned char reachability;

// Timezone things
TimeChangeRule ukBST = { "BST", Last, Sun, Mar, 2, 60 };
TimeChangeRule ukGMT = { "GMT", Last, Sun, Oct, 2, 0 };
Timezone ukTime(ukGMT, ukBST);

timeNow_t timeNow;
int updateTime;
int ntpUpdates;        // need 20 re-sync's before time is "stable"
int reSyncCount;       // re-sync's since midnight

long ticks;
int tickTime;

int dayOfMonth;

cmdType cmdList[] =
{
    { "clear",     cmdClearConfig },
    { "display",   cmdDisplay },
    { "help",      cmdListCommands },
    { "load",      cmdGetConfig },
    { "ntpserver", cmdNtpServer },
    { "password",  cmdPassword },
    { "save",      cmdSaveConfig },
    { "show",      cmdShowState },
    { "ssid",      cmdSsid },
    { "?",         cmdListCommands },
    { NULL,        NULL }
};

// Buffer when reading serial port
char serialBuff[SERBUFF_LEN];
char *paramPtr;

unsigned long int interruptCount;

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

void printWifiStatus()
{
  long rssi;
  IPAddress ip;
  IPAddress mask;
  IPAddress gway;
  char wifiStr[32];
  
  if(WiFi.status() != WL_CONNECTED)
  {
      Serial.println("Wifi disconnected");
  }
  else
  {
      ip = WiFi.localIP();
      mask = WiFi.subnetMask();
      gway = WiFi.gatewayIP();
      rssi = WiFi.RSSI();

      sprintf(wifiStr, "Wifi connected to %s\n", clockConfig.ssid);
      Serial.print(wifiStr);
      sprintf(wifiStr, "  RSSI      : %d dBm\n", rssi);
      Serial.print(wifiStr);
      Serial.print("  IP        : ");
      Serial.println(ip);
      Serial.print("  Netmask   : ");
      Serial.println(mask);
      Serial.print("  Gateway   : ");
      Serial.println(gway);
  }
}

void splitDigit(int x, int *hi)
{
    *hi = (x / 10);
    hi++;
    *hi = (x % 10);
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
    ledDisplay.data[0] = 0;
    splitDigit(timeStruct -> tm_mday, &ledDisplay.data[1]);
    splitDigit(timeStruct -> tm_mon, &ledDisplay.data[3]);
    ledDisplay.data[5] = 0;
}

void serialShowTime(timeNow_t *timeStruct, char *timeName)
{
    int screen;
    int digit;
    char timeString[30];

    if(updateTime == INIT_UPDATE)
    {
        Serial.print("[INIT] ");
    }
    else
    {
        Serial.print("[SYNC] ");
    }

    sprintf(timeString, "%d-%d   %s %02d/%02d/%02d", reSyncCount, ticks, dayStrings[timeStruct -> tm_wday], timeStruct -> tm_mday, timeStruct -> tm_mon, timeStruct -> tm_year);
    Serial.print(timeString);
                  
    Serial.print(" - ");

    sprintf(timeString, "%02d:%02d:%02d %s", timeStruct -> tm_hour, timeStruct -> tm_min, timeStruct -> tm_sec, timeName);
    Serial.println(timeString);    
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

// Read a line from serial port up to '\n' character
// seperate command and parameter into two strings by replacing space with '\0'
// set paramPtr to point to parameter if present
void serialReadline()
{
    int c;
    int inchar;

    paramPtr = NULL;

    c = 0;
    do
    {
        if(Serial.available() > 0)
        {
            inchar = Serial.read();
            if(inchar == '\n')
            {
                serialBuff[c] = '\0';
            }
            else
            {
                if(inchar == ' ')
                {
                    serialBuff[c] = '\0';
                    c++;
                    paramPtr = &serialBuff[c];
                }
                else
                {
                    if(isprint(inchar))
                    {
                        serialBuff[c] = inchar;
                        c++;
                    }
                }
            }
        }

        if(c == SERBUFF_LEN)
        {
            serialBuff[SERBUFF_LEN - 1] = '\0';
        }
    }
    while(c < SERBUFF_LEN && inchar != '\n');
}

void cmdSaveConfig()
{
    saveClockConfig();
    Serial.println("Running configuration saved");
}

void cmdClearConfig()
{
    memset(&clockConfig, 0, sizeof(clockConfig));

    Serial.println("Running configuration erased");
}

void cmdGetConfig()
{
    getClockConfig();
    Serial.println("Loaded saved configuration");
}

void cmdSsid()
{
    if(paramPtr != NULL)
    {
         strncpy(clockConfig.ssid, paramPtr, 32);
    }

    Serial.print("SSID: ");
    Serial.print(clockConfig.ssid);
    Serial.print("\n");
}

void cmdDisplay()
{
    int c;
    
    if(paramPtr == NULL)
    {
        Serial.println("display <digits>");
    }
    else
    {
        if(strlen(paramPtr) != 6)
        {
            Serial.println("Need 6 digits");
        }
        else
        {
            for(c = 0; c < 6; c++)
            {
                ledDisplay.data[c] = (*(paramPtr + c)) - 48;
            }
        }
    }
}

void cmdPassword()
{
    if(paramPtr != NULL)
    {
        strncpy(clockConfig.password, paramPtr, 32);
    }

    Serial.print("Password: ");
    Serial.print(clockConfig.password);
    Serial.print("\n");
}

void cmdNtpServer()
{
    if(paramPtr != NULL)
    {
         strncpy(clockConfig.ntpServer, paramPtr, 32);
    }

    Serial.print("NTP server: ");
    Serial.print(clockConfig.ntpServer);
    Serial.print("\n");
}

void cmdShowState()
{
    char tmpStr[32];
    int c;
    unsigned char r;
    
    printWifiStatus();

    Serial.print("Running configuration\n");
    Serial.print("  SSID      : ");
    Serial.println(clockConfig.ssid);
    Serial.print("  Password  : ");
    Serial.println(clockConfig.password);
    Serial.print("  NTP server: ");
    Serial.println(clockConfig.ntpServer);

    Serial.print("\n");
    Serial.print("Interrupt count: ");
    Serial.print(interruptCount);
    Serial.print("\n");

    r = reachability;
    sprintf(tmpStr, "Reachability: 0x%02x (0b", r, r);
    Serial.print(tmpStr);

    for(c = 0; c < 8; c++)
    {
        if((r & 0x80) == 0x80)
        {
            Serial.print("1");
        }
        else
        {
            Serial.print("0");
        }
        
        r = r << 1;
    }
    Serial.print(")\n");
}

// List all available commands
void cmdListCommands()
{
    int c;
    
    c = 0;
    while(cmdList[c].cmdName != NULL)
    {
        Serial.print(cmdList[c].cmdName);
        Serial.print("\n");

        c++;
    }

    Serial.print("\n");
    Serial.print("'exit' to finish\n");
}

void cli()
{
    int cmd;
    int done;

    Serial.print(HELLO_STR);
    Serial.print("\n");

    done = false;
    do
    {
        Serial.print("> ");
        serialReadline();
        Serial.print("\n");
 
        if(serialBuff[0] != '\0')
        {
            if(strcmp("exit", serialBuff) == 0)
            {
                done = true;
            }
            else
            {
                cmd = 0;
                while(cmdList[cmd].cmdName != NULL && strcmp(cmdList[cmd].cmdName, serialBuff) != 0)
                {
                    cmd++;
                }
  
                if(cmdList[cmd].cmdName == NULL)
                {
                    Serial.print("Bad command\n");
                }
                else
                {
                    cmdList[cmd].fn();
                }
            }
        }

        Serial.print("\n");
    }
    while(done == false);

    Serial.print("CLI exit\n");
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

void setup()
{
    // Serial port
    Serial.begin(9600);
    Serial.println(HELLO_STR);
    Serial.print("\n");

    // GPIO
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(syncLedPin, OUTPUT);
    pinMode(modeSelectPin, INPUT_PULLUP);
    pinMode(dateTimePin, INPUT_PULLUP);

    // LED display
    initDisplay();

    // Display interrupt and handler
    interruptCount = 0;
    MyTimer5.begin(DISP_INTS_PER_SECOND);
    MyTimer5.attachInterrupt(displayInterrupt);
    MyTimer5.start();

    // Display a pattern
    initDisplayPattern();

#ifndef __TEST_DISPLAY

    // Read configuration from flash memory
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
#endif

#ifdef __TEST_DISPLAY
    while(1)
    {
        testDisplay();
    }
#endif

    reSyncCount = 0;
    reachability = 0;
}

void loop()
{
  time_t epoch;
  TimeChangeRule *tcr;

  switch(clockState)
  {
      case STATE_INIT:
          Serial.print("*** Initialising");
          WiFi.begin(clockConfig.ssid, clockConfig.password);
          digitalWrite(LED_BUILTIN, LOW);
          digitalWrite(syncLedPin, LOW);
          clockState = STATE_CONNECTING;
          tickTime = INIT_TICKTIME;
          reSyncCount++;
          break;

      case STATE_CONNECTING:
          if(WiFi.status() == WL_CONNECTED)
          {
              Serial.println("");
              Serial.println("*** Wifi connected");
              printWifiStatus();
              digitalWrite(LED_BUILTIN, HIGH);
              Serial.print("*** Starting NTP - ");
              Serial.println(clockConfig.ntpServer);
              timeClient.setPoolServerName(clockConfig.ntpServer);
              timeClient.setUpdateInterval(SYNC_UPDATE * 1000);
              timeClient.begin(NTP_PORT);
              clockState = STATE_TIMING;
              updateTime = INIT_UPDATE;
              ntpUpdates = 0;
              ticks = 0;
          }
          else
          {
              Serial.print(".");
          }
          break;

      case STATE_TIMING:
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

          // seconds since start of time
          epoch = timeClient.getEpochTime();

          // convert to uk time with bst or gmt
          epoch = ukTime.toLocal(epoch, &tcr);
          dayOfMonth = splitTime(epoch, &timeNow);
          if(dayOfMonth != timeNow.tm_mday)
          {
              reSyncCount = 0;
          }

          // send everything to serial port too
          serialShowTime(&timeNow, tcr -> abbrev);

          // load display data
          if(digitalRead(dateTimePin) == HIGH)
          {
              ledShowTime(&timeNow);
          }
          else
          {
              ledShowDate(&timeNow);
          }

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

  if(Serial.available() > 0)
  {
      cli();
      clockState = STATE_STOPPED;
  }
  else
  {
      delay(tickTime);
  }  
}

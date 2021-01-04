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

#include <driver/source/nmasic.h>

#include "Timer5.h"

//#define __TEST_DISPLAY            // Just do display test and nothing else...
#define __USE_DEFAULTS            // Load default configuration if nothing else already stored.  Otherwise, boot to CLI for user configuration
#define __WITH_TELNET             // To allow telnet connection for a status page
#define __WITH_HTTP               // To enable configuration web page

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

#define MORSE_DELAY     60        // Dot period for morse code chime (T=1200/WPM)

#ifdef __USE_DEFAULTS

// Default values for configuration if nothing found at startup
#define DEFAULT_SSID    "ballacurmudgeon"
#define DEFAULT_PSWD    "scaramanga"
#define DEFAULT_NTPS    "192.168.1.251"

#endif

#ifdef __WITH_HTTP

// HTTP GET request handlers
typedef struct
{
    char *fileName;
    void (*fn)(void);
} getRequestType;


// HTTP parameter setting handlers
typedef struct
{
    char *paramName;
    void (*fn)(char *);
} httpParamType;

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
void cmdWebConfig(void);
void cmdWiFiVersion(void);

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
    char ssid[40];
    char password[70];
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

#ifdef __WITH_TELNET
WiFiServer ws(23);
#endif

#ifdef __WITH_HTTP
WiFiServer httpServer(80);
#endif

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

int morseTimePin = 9;                           // Ground to send time in morse
int disChimePin = 8;                            // Ground to disable chimes
int chimePin = 7;                               // Hourly chimes output
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
#ifdef __WITH_HTTP
    { "webconfig", cmdWebConfig },
#endif
    { "wifiver",   cmdWiFiVersion },
    { "?",         cmdListCommands },
    { NULL,        NULL }
};

// Buffer when reading serial port
char serialBuff[SERBUFF_LEN];
char *paramPtr;

unsigned long int interruptCount;

boolean chime;

#ifdef __WITH_HTTP

WiFiClient httpClient;
char *httpParams[16];
char httpRequest[255];
boolean httpDone;

void httpResetConfiguration();
void httpSaveConfiguration();
void httpConfigPage();
void httpSetSsid(char *);
void httpSetPassword(char *);
void httpSetNtpServer(char *);

getRequestType getRequests[] =
{
    { "/reset.html", httpResetConfiguration },
    { "/config.html", httpSaveConfiguration },
    { "/", httpConfigPage },
    { NULL, NULL }
};

httpParamType httpParamHandlers[] =
{
    { "ssid", httpSetSsid },
    { "password", httpSetPassword },
    { "ntpserver", httpSetNtpServer },
    { NULL, NULL }
};

#endif

// Morse characters 0-9
// To send, clock out 5 bits, LSB first - if LSB is '1', send a dot, otherwise, send a dash
int morse[10] = { 0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x1e, 0x1c, 0x18, 0x10 };

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

// Convert a number to binary in ASCII
void binToStr(int bin, char *str)
{
    int c;

    for(c = 0; c < 8; c++)
    {
        if((bin & 0x80) == 0x80)
        {
            *str = '1';
        }
        else
        {
            *str = '0';
        }

        str++;
        bin = bin << 1;
    }

    *str = '\0';
}

void splitDigit(int x, int *digPtr)
{
    *digPtr = (x / 10);
    digPtr++;
    *digPtr = (x % 10);
}

void sendMorseChar(int ch)
{
    int c;
    int dashDelay;
    int morseChar;

    if(ch > 9)
    {
        Serial.print("Illegal value for sendMorseChar() - ");
        Serial.println(ch);
        return;
    }

    morseChar = morse[ch];
    
    dashDelay = 3 * MORSE_DELAY;

    for(c = 0; c < 5; c++)
    {
        digitalWrite(chimePin, HIGH);
        if((morseChar & 0x01) == 0x01)
        {
            delay(MORSE_DELAY);
        }
        else
        {
            delay(dashDelay);
        }
        digitalWrite(chimePin, LOW);

        delay(MORSE_DELAY);  // inter-symbol delay

        morseChar = morseChar >> 1;
    }
    
    delay(2 * MORSE_DELAY);
}

void chimeMorse()
{
    sendMorseChar(ledDisplay.data[0] & 0x03);
    sendMorseChar(ledDisplay.data[1]);
}

void timeInMorse()
{
    int c;

    Serial.print("Morse time");
    for(c = 0; c < 4; c++)
    {
        // Top bit of hours might be set to show PM
        if(c == 0)
        {
            sendMorseChar(ledDisplay.data[c] & 0x03);
        }
        else
        {
            sendMorseChar(ledDisplay.data[c]);
        }

        // Wait "word space" between hours and minutes
        if(c == 1)
        {
            delay(3 * MORSE_DELAY);
        }
    }
    Serial.println(" - done");
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

        Serial.print("Wifi connected to ");
        Serial.println(clockConfig.ssid);
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
         strncpy(clockConfig.ssid, paramPtr, 40);
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
        strncpy(clockConfig.password, paramPtr, 64);
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

    sprintf(tmpStr, "Reachability: 0x%02x (0b", reachability);
    Serial.print(tmpStr);
    binToStr(reachability, tmpStr);
    Serial.print(tmpStr);
    Serial.print(")\n");

    Serial.print("Resync count: ");
    Serial.print(reSyncCount);
    Serial.println(" today");
        
    Serial.print("Mode: ");
    if(digitalRead(modeSelectPin) == LOW)
    {
        Serial.println("12 hour");
    }
    else
    {
        Serial.println("24 hour");
    }
    
    Serial.print("Display: ");
    if(digitalRead(dateTimePin) == LOW)
    {
        Serial.println("Date");
    }
    else
    {
        Serial.println("Time");
    }

    Serial.print("Hourly chimes: ");
    if(digitalRead(disChimePin) == HIGH)
    {
        Serial.println("Enabled");
    }
    else
    {
        Serial.println("Disabled");
    }

}

void cmdWiFiVersion()
{
    String fv;
    String latestFw;

    fv = WiFi.firmwareVersion();

    if(REV(GET_CHIPID()) >= REV_3A0)
    {
        latestFw = WIFI_FIRMWARE_LATEST_MODEL_B;
    }
    else
    {
        latestFw = WIFI_FIRMWARE_LATEST_MODEL_A;
    }

    Serial.print("Latest firmware: ");
    Serial.println(latestFw);

    Serial.print("Current verstion: ");
    Serial.print(fv);
    if(fv >= latestFw)
    {
        Serial.println(" (OK)");
    }
    else
    {
        Serial.println(" (REQUIRES UPDATE)");
    }
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

#ifdef __WITH_TELNET

void telnetShowStatus(WiFiClient client)
{
    int c;
    char buff[32];

    client.println(HELLO_STR);

    client.print("WiFi SSID: ");
    client.println(clockConfig.ssid);

    sprintf(buff, "Wifi RSSI: %d dBm", WiFi.RSSI());
    client.println(buff);

    client.println("Display data:");
    for(c = 0; c < 6; c++)
    {
        sprintf(buff, "  Digit %d - 0x%02x - ", c, ledDisplay.data[c]);
        client.print(buff);
        binToStr(ledDisplay.data[c], buff);
        client.println(buff);
    }

    client.print("NTP server: ");
    client.println(clockConfig.ntpServer);

    sprintf(buff, "Reachability: 0x%02x (0b", reachability);
    client.print(buff);
    binToStr(reachability, buff);
    strcat(buff, ")");
    client.println(buff);

    sprintf(buff, "Hourly chimes: ");
    if(digitalRead(disChimePin) == HIGH)
    {
        strcat(buff, "Enabled");
    }
    else
    {
        strcat(buff, "Disabled");
    }
    client.println(buff);

    sprintf(buff, "Display mode: ");
    if(digitalRead(modeSelectPin) == LOW)
    {
        strcat(buff, "12 hour");
    }
    else
    {
        strcat(buff, "24 hour");
    }
    client.println(buff);

    sprintf(buff, "Interrupt count: %ld", interruptCount);
    client.println(buff);

    sprintf(buff, "Resync's today: %d", reSyncCount);
    client.println(buff);
}

#endif

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

#ifdef __WITH_HTTP

void httpStartAP()
{
    IPAddress ip;

    Serial.println(" - Starting WiFi access point");
  
    // Start Wifi in AP mode
    if(WiFi.beginAP("NTPClock") != WL_AP_LISTENING)
    {
        Serial.println(" - Failed");
    }
    delay(5000);

    ip = WiFi.localIP();
    Serial.print(" - IP address: ");
    Serial.println(ip);
}

void httpHeader()
{
    httpClient.println("HTTP/1.1 200 OK");
    httpClient.println("Content-type:text/html");
    httpClient.println();
  
    httpClient.println("<html>");
    httpClient.println("<body>");
    
    httpClient.println("<h2>NTP Clock configuration</h2>");  
}

void httpFooter()
{
    httpClient.println("<p><a href=\"/\">Back</a> to main page</p>");
    httpClient.println("</body>");
    httpClient.println("</html>");  
}

void httpConfigPage()
{
    httpHeader();
    
    httpClient.println("<form action=\"/config.html\">");
    httpClient.println("<label for=\"ssid\">WiFi SSID:</label><br>");
    httpClient.print("<input type=\"text\" id=\"ssid\" name=\"ssid\" value=\"");
    httpClient.print(clockConfig.ssid);
    httpClient.println("\"><br><br>");
    httpClient.println("<label for=\"password\">WiFi Password:</label><br>");
    httpClient.print("<input type=\"text\" id=\"password\" name=\"password\" value=\"");
    httpClient.print(clockConfig.password);
    httpClient.println("\"><br><br>");
    httpClient.println("<label for=\"ntpserver\">NTP server:</label><br>");
    httpClient.print("<input type=\"text\" id=\"ntpserver\" name=\"ntpserver\" value=\"");
    httpClient.print(clockConfig.ntpServer);
    httpClient.println("\"><br><br>");
    httpClient.println("<input type=\"submit\" value=\"Save settings\">");
    httpClient.println("</form>");

    httpClient.println("<br>");

    httpClient.println("<form action=\"/reset.html\">");
    httpClient.println("<input type=\"submit\" value=\"Default settings\">");
    httpClient.println("</form>");

    httpClient.println("</body>");
    httpClient.println("</html>");
}

void httpResetConfiguration()
{
    httpHeader();
    httpClient.println("<p>Load default configuration</p>");
    httpFooter();

#ifdef __USE_DEFAULTS
    strcpy(clockConfig.ssid, DEFAULT_SSID);
    strcpy(clockConfig.password, DEFAULT_PSWD);
    strcpy(clockConfig.ntpServer, DEFAULT_NTPS);
#else
    memset(&clockConfig, 0, sizeof(clockConfig));
#endif
}

void httpSetSsid(char *ssid)
{
    strcpy(clockConfig.ssid, ssid); 
}

void httpSetPassword(char *password)
{
    strcpy(clockConfig.password, password);
}

void httpSetNtpServer(char *ntpServer)
{
    strcpy(clockConfig.ntpServer, ntpServer);
}

void httpParseParam(char *paramName, char *paramValue)
{
    int c;

    Serial.print("Trying to set ");
    Serial.print(paramName);
    Serial.print(" to ");
    Serial.println(paramValue);

    c = 0;
    while(httpParamHandlers[c].paramName != NULL && strcmp(httpParamHandlers[c].paramName, paramName) != 0)
    {
        c++;
    }

    if(httpParamHandlers[c].paramName != NULL)
    {
        httpParamHandlers[c].fn(paramValue);
    }
    else
    {
        Serial.print("Bad parameter");
    }
}

void httpSaveConfiguration()
{
    int c;
    char *paramName;
    char *paramValue;
    
    httpHeader();
    httpClient.println("<p>Load configuration:</p><br>");

    for(c = 1; c < 4; c++)
    {
        paramName = strtok(httpParams[c], "=");
        paramValue = strtok(NULL, "=");

        httpClient.print("<p>Set ");
        httpClient.print(paramName);
        httpClient.print(" to ");
        httpClient.print(paramValue);
        httpClient.print("</p><br>");
        
        httpParseParam(paramName, paramValue);
    }
    
    httpFooter();

    saveClockConfig();
    httpDone = true;
}

void httpNotFound()
{
    httpClient.println("HTTP/1.1 404 Not Found");
}

void httpHandleGetRequest(char *url)
{
    char *tokPtr;
    char *filename;
    int c;
  
    c = 0;

    tokPtr = strtok(url, "?& ");
    while(tokPtr != NULL)
    {
        httpParams[c] = tokPtr;
        Serial.print("Param ");
        Serial.print(c);
        Serial.print(": ");
        Serial.println(tokPtr);
        c++;

        tokPtr = strtok(NULL, "?& ");
    }

    c = 0;
    while(getRequests[c].fileName != NULL && strcmp(getRequests[c].fileName, httpParams[0]) != 0)
    {
        c++;
    }

    if(getRequests[c].fileName == NULL)
    {
        httpNotFound();
    }
    else
    {
        getRequests[c].fn();
    }
}

void httpWebServer()
{
    char c;
    char *urlPtr;
    unsigned long int ms;
  
    // Start webserver
    httpServer.begin();

    httpDone = false;
    ms = millis();
    while(httpDone == false)
    {
        httpClient = httpServer.available();
        if(httpClient)
        {
            urlPtr = httpRequest;
            while(httpClient.connected())
            {
                if(httpClient.available())
                {
                    ms = millis();
                    c = httpClient.read();
                    if(c == '\n')
                    {
                        *urlPtr = '\0';

                        Serial.println(httpRequest);
                        
                        if(strncmp(httpRequest, "GET", 3) == 0)
                        {
                            httpHandleGetRequest(&httpRequest[4]);
                        }

                        httpClient.flush();
                        httpClient.stop();
                    }
                    else
                    {
                        if(c != '\r')
                        {
                            *urlPtr = c;
                            urlPtr++;
                        }
                    }
                }
                else
                {
                    if(ms - millis() > 2000)
                    {
                        Serial.println("HTTP idle timeout");
                        httpClient.stop();
                    }
                }
            }

            Serial.println("Disconnected");
        }
    }
}

void httpStatusPage(char *url)
{
    char c;
    char *filename;
    char txtBuff[80];

    filename = strtok(url, "?& ");
    if(strcmp(filename, "/") != 0)
    {
        httpNotFound();
    }
    else
    {
        httpClient.println("HTTP/1.1 200 OK");
        httpClient.println("Content-type:text/html\r\n");
  
        httpClient.println("<html>");

        httpClient.println("<head>");
        httpClient.println("<style>");
        httpClient.println("table, th, td {");
        httpClient.println("  border: 1px solid black;");
        httpClient.println("  border-collapse: collapse;");
        httpClient.println("  padding: 5px;");
        httpClient.println("  text-align: left;");
        httpClient.println("}");
        httpClient.println("</style>");
        httpClient.println("</head>");

        httpClient.println("<body>");
    
        httpClient.println("<h2>NTP Clock Status</h2>");
          
        httpClient.println("<table>");
        httpClient.println("  <tr>");
        httpClient.println("    <th>Time</th>");
        sprintf(txtBuff, "    <td>%02d:%02d:%02d</td>", timeNow.tm_hour, timeNow.tm_min, timeNow.tm_sec);
        httpClient.println(txtBuff);
        httpClient.println("  </tr>");
        httpClient.println("  <tr>");
        httpClient.println("    <th>Date</th>");
        sprintf(txtBuff, "    <td>%02d/%02d/%04d</td>", timeNow.tm_mday, timeNow.tm_mon, timeNow.tm_year);
        httpClient.println(txtBuff);
        httpClient.println("  </tr>");
        httpClient.println("</table>");

        httpClient.println("<br>");
        
        httpClient.println("<table>");
        httpClient.println("  <tr>");
        httpClient.println("    <th>WiFi SSID</th>");
        sprintf(txtBuff, "    <td>%s</td>", clockConfig.ssid);
        httpClient.println(txtBuff);
        httpClient.println("  </tr>");
        httpClient.println("  <tr>");
        httpClient.println("    <th>WiFi RSSI</th>");
        sprintf(txtBuff, "    <td>%d dBm</td>", WiFi.RSSI());
        httpClient.println(txtBuff);
        httpClient.println("  </tr>");
        httpClient.println("</table>");

        httpClient.println("<br>");
        
        httpClient.println("<table>");
        httpClient.println("  <tr>");
        httpClient.println("    <th>NTP server</th>");
        sprintf(txtBuff, "    <td>%s</td>", clockConfig.ntpServer);
        httpClient.println(txtBuff);
        httpClient.println("  </tr>");
        httpClient.println("  <tr>");
        httpClient.println("    <th>Reachability</th>");
        sprintf(txtBuff, "    <td>0x%02x</td>", reachability);
        httpClient.println(txtBuff);
        httpClient.println("  </tr>");
        httpClient.println("  <tr>");
        httpClient.println("    <th>Re-syncs</th>");
        sprintf(txtBuff, "    <td>%d</td>", reSyncCount);
        httpClient.println(txtBuff);
        httpClient.println("  </tr>");
        httpClient.println("</table>");

        httpClient.println("<br>");
        
        httpClient.println("<table>");
        httpClient.println("  <tr>");
        httpClient.println("    <th>Mode</th>");
        if(digitalRead(dateTimePin) == LOW)
        {
            httpClient.println("    <td>12 hour</td>");
        }
        else
        {
            httpClient.println("    <td>24 hour</td>");          
        }
        httpClient.println("  </tr>");
        httpClient.println("  <tr>");
        httpClient.println("    <th>Hourly chimes</th>");
        if(digitalRead(disChimePin) == HIGH)
        {
            httpClient.println("    <td>Enabled</td>");
        }
        else
        {
            httpClient.println("    <td>Disabled</td>");          
        }
        httpClient.println("  </tr>");
        httpClient.println("</table>");

        httpClient.println("<p>");
        httpClient.println("Power up holding 'mode' or 'morse' button to enter configuration mode");
        httpClient.println("</p>");
        httpClient.println("<p>");
        httpClient.println("Clock will start WiFi access point with SSID 'NTPclock'<br>");
        httpClient.println("Connect to that to access configuration page at http://192.168.1.1");
        httpClient.println("</p>");
        httpClient.println("<p>");
        httpClient.println("Configuration can also be done using CLI on serial port at 9600 baud");
        httpClient.println("</p>");

        httpClient.println("</body>");
        httpClient.println("</html>");  
    }
}

void httpStatusGetRequest()
{
    char c;
    char *urlPtr;
    unsigned long int ms;

    Serial.println("Client connected to webserver");
  
    urlPtr = httpRequest;
    ms = millis();
    while(httpClient.connected())
    {
        if(httpClient.available())
        {
            ms = millis();
            c = httpClient.read();
            if(c == '\n')
            {
                *urlPtr = '\0';

                Serial.println(httpRequest);
                        
                if(strncmp(httpRequest, "GET", 3) == 0)
                {
                    httpStatusPage(&httpRequest[4]);
                }

                httpClient.flush();
                httpClient.stop();
            }
            else
            {
                if(c != '\r')
                {
                    *urlPtr = c;
                    urlPtr++;
                }
            }
        }
        else
        {
            if(millis() - ms > 2000)
            {
                Serial.println("HTTP idle timeout");
                httpClient.stop();
            }
        }
    }
    
    Serial.println("Disconnected");
}

void cmdWebConfig()
{
    Serial.println(" - Stopping WiFi client");
    WiFi.end();
    httpStartAP();
    httpWebServer();
    Serial.println(" - Stopping Wifi access point");
    WiFi.end();
}

#endif

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

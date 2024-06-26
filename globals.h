#ifdef __MK1_HW
#include <WiFi101.h>
#else
#include <WiFi.h>
#ifdef __WITH_TELNET_CLI
#include <ESPTelnet.h>
#endif
#endif

#ifdef __IN_MAIN

eepromData clockConfig;

// Buffer when reading serial port
char serialBuff[SERBUFF_LEN];
char *paramPtr[MAX_PARAMS];
int paramCount;
int ntpSyncState;

volatile int ledColSelect;
int ledColPins[MAX_COLS] = { PIN_COLH1, PIN_COLH2, PIN_COLM1, PIN_COLM2, PIN_COLS1, PIN_COLS2 };  // column select pins h-h-m-m-s-s
int ledColSize[MAX_COLS] = { 4, 4, 3, 4, 3, 4 };                                                  // number of leds on this column hh:mm:ss
int ledColData[MAX_COLS] = { 0, 0, 0, 0, 0, 0 };                                                  // display data
int ledRowPins[MAX_ROWS] = { PIN_DATA1, PIN_DATA2, PIN_DATA3, PIN_DATA4 };                        // data pins          lsb first

char *dayStrings[] =
{
    "Sunday",
    "Monday",
    "Tuesday",
    "Wednesday",
    "Thursday",
    "Friday",
    "Saturday"
};

#ifdef __WITH_HTTP
WiFiServer httpServer(HTTP_PORT);
WiFiClient httpClient;
#endif

#ifdef __WITH_TELNET
WiFiClient telnetClient;
#endif

#ifdef __WITH_TELNET_CLI
ESPTelnet telnet;
#endif

unsigned int reachability;
int reSyncCount;       // re-sync's since midnight

int updateTime;
timeNow_t timeNow;
long ticks;

volatile unsigned long int interruptCount;

boolean loggedIn;
boolean newTelnetConnection;

#else

extern eepromData clockConfig;
extern char serialBuff[];
extern char *paramPtr[];
extern int paramCount;
extern volatile int ledColSelect;
extern int ledColPins[];
extern int ledColSize[];
extern int ledColData[];
extern int ledRowPins[];
extern WiFiServer httpServer;
extern WiFiClient httpClient;
extern WiFiClient telnetClient;
#ifdef __WITH_TELNET_CLI
extern ESPTelnet telnet;
#endif
extern int updateTime;
extern int reSyncCount;
extern unsigned int reachability;
extern timeNow_t timeNow;
extern volatile unsigned long int interruptCount;
extern long ticks;
extern char *dayStrings[];
extern int ntpSyncState;
extern boolean loggedIn;
extern boolean newTelnetConnection;

#endif

void defaultClockConfig();
boolean getClockConfig();
void saveClockConfig();

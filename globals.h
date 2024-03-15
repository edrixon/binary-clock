#ifdef __MK1_HW
#include <WiFi101.h>
#else
#include <WiFi.h>
#endif

#ifdef __IN_MAIN

eepromData clockConfig;

// Buffer when reading serial port
char serialBuff[SERBUFF_LEN];
char *paramPtr[MAX_PARAMS];
int paramCount;
int ntpSyncState;

volatile ledDisplay_t ledDisplay =
{
    { PIN_COLH1, PIN_COLH2, PIN_COLM1, PIN_COLM2, PIN_COLS1, PIN_COLS2 },  // column select pins h-h-m-m-s-s
    { 4,  4,  3,  4,  3,  4 },                                             // number of leds on this column hh:mm:ss
    { PIN_DATA1, PIN_DATA2, PIN_DATA3, PIN_DATA4 },                        // data pins          lsb first
    { 0, 0, 0, 0, 0, 0 },                                                  // display data
    0,                                                                     // current column selection
};

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
WiFiServer httpServer(80);
WiFiClient httpClient;
#endif

unsigned char reachability;
int reSyncCount;       // re-sync's since midnight

int updateTime;
timeNow_t timeNow;
long ticks;

volatile unsigned long int interruptCount;


#else

extern eepromData clockConfig;
extern char serialBuff[];
extern char *paramPtr[];
extern int paramCount;
extern volatile ledDisplay_t ledDisplay;
extern WiFiServer httpServer;
extern WiFiClient httpClient;
extern int updateTime;
extern int reSyncCount;
extern unsigned char reachability;
extern timeNow_t timeNow;
extern volatile unsigned long int interruptCount;
extern long ticks;
extern char *dayStrings[];
extern int ntpSyncState;

#endif

void defaultClockConfig();
boolean getClockConfig();
void saveClockConfig();

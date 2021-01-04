#include <WiFi101.h>

#ifdef __IN_MAIN

eepromData clockConfig;

// Buffer when reading serial port
char serialBuff[SERBUFF_LEN];
char *paramPtr;

ledDisplay_t ledDisplay =
{
    { A0, A1, A2, A3, A4, A5 },                 // column select pins h-h-m-m-s-s
    { 4,  4,  3,  4,  3,  4 },                  // number of leds on this column hh:mm:ss
    { A6, 0, 1, 2 },                            // data pins          lsb first
    { 0, 0, 0, 0, 0, 0 },                       // display data
    0,                                          // current column selection
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

int morseTimePin = 9;                           // Ground to send time in morse
int disChimePin = 8;                            // Ground to disable chimes
int chimePin = 7;                               // Hourly chimes output
int dateTimePin = 5;                            // Ground to show date instead of time
int modeSelectPin = 4;                          // 12 or 24hr mode
int syncLedPin = 3;                             // LED lights when sync'd
long ticks;

unsigned long int interruptCount;


#else

extern eepromData clockConfig;
extern char serialBuff[];
extern char *paramPtr;
extern ledDisplay_t ledDisplay;
extern WiFiServer httpServer;
extern WiFiClient httpClient;
extern int updateTime;
extern int reSyncCount;
extern unsigned char reachability;
extern timeNow_t timeNow;
extern int morseTimePin;
extern int disChimePin; 
extern int chimePin;
extern int dateTimePin;
extern int modeSelectPin;
extern int syncLedPin;   
extern unsigned long int interruptCount;
extern long ticks;
extern char *dayStrings[];

#endif

boolean getClockConfig();
void saveClockConfig();

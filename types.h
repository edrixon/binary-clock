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
    char hostName[32];
    char ftpUser[32];
    char ftpPassword[32];
    int syncUpdate;
    int initUpdate;
    int syncValid;
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

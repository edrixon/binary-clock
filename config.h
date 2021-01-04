
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

// Default values for configuration if nothing found at startup
#define DEFAULT_SSID    "ballacurmudgeon"
#define DEFAULT_PSWD    "scaramanga"
#define DEFAULT_NTPS    "192.168.1.251"

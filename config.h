//#define __MK1_HW                // Original 2019 Veroboard version with MKR1000 processor
#define __MK2_HW                 // 2024 PCB version with ESP32 S3 processor

//#define __TEST_DISPLAY          // Just do display test and nothing else...
#define __USE_DEFAULTS           // Load default configuration if nothing else already stored.  Otherwise, boot to CLI for user configuration
#define __WITH_TELNET            // To allow telnet connection for a status page
#define __WITH_HTTP              // To enable configuration web page

// Printed on serial port when board starts
#define HELLO_STR      "** NTP clock by Ed Rixon, GD6XHG **"

// A value to indicate the saved configuration is valid
#define EEPROM_VALID   0xdeadbeef

// NTP configuration
#define NTP_PORT       123       // local port number
#define SYNC_UPDATE    900       // number of seconds between NTP polls once stable
#define INIT_UPDATE    20        // initial time between NTP polls 
#define SYNC_VALID     5         // How many initial NTP responses needed before "stable"

#define TICKTIME       1000      // ms tick time for normal operation of state machine

// Command line interpretter
#define SERBUFF_LEN    80        // Command line buffer size
#define CLI_PROMPT     "clock> " // Prompt
#define MAX_PARAMS     4         // Maximum number of command lineparameters

// Clock display dimensions
#define MAX_COLS       6         // Number of columns in display      
#define MAX_ROWS       4         // Number of rows in display

// Morse code timing
#define MORSE_DELAY    60        // Dot period in ms (T=1200/WPM)

// Default values for configuration if nothing found at startup
#define DEFAULT_SSID   "ballacurmudgeon"
#define DEFAULT_PSWD   "scaramanga"
#define DEFAULT_NTPS   "ubuntu.pool.ntp.org"

// Access point setup for configuration mode
#define AP_SSID        "NTPClock"

// Top bits of most significant hours are used for AM/PM
#define BIT_AMPM       0x08      // AM/PM

#ifdef __MK1_HW
// For MK1 hardware (Veroboard)

#define DISP_INTS_PER_SECOND 400 // Display timer interrupts per second

// GPIO Pins...

// LED display
#define PIN_COLH1      A0        // Hours columns
#define PIN_COLH2      A1
#define PIN_COLM1      A2        // Minutes columns
#define PIN_COLM2      A3
#define PIN_COLS1      A4        // Seconds columns
#define PIN_COLS2      A5
#define PIN_DATA1      A6        // Data LSB row
#define PIN_DATA2      0         //
#define PIN_DATA3      1         //
#define PIN_DATA4      2         // Data MSB row

// Other outputs
#define PIN_BEEP       7         // Output pin for morse code beeper - HIGH = beep, LOW = quiet
#define PIN_SYNCLED    3         // Output pin to drive "sync" LED - HIGH = NTP synchronised, LOW = not synch'ed

// Switches
#define PIN_MORSETIME  9         // LOW = send hours and minutes of time in morse code
#define PIN_CHIME      8         // LOW = enable hourly chimes in morse code
#define PIN_DATETIME   5         // LOW = show date instead of time
#define PIN_MODESEL    4         // LOW = 12 hour mode, HIGH = 24 hour mode

#else

// For MK2 hardware (PCB)

#define DISP_INT_100US  25       // Interrupt period in 100's of usecs - this is multiplied by 100 in the code to give ms period
#define BIT_SYNCLED     0x04     // Bit for NTP synchronisation LED in "hours" LEDs
#define CONFIG_FILENAME "/config.dat"  // The filename for configuration information
#define RGB_OFF         0        // RGB value to use to turn colour off 
#define RGB_VAL         10       // RGB value to use to turn colour on at sensible brightness

// GPIO Pins...
// LED display
#define PIN_COLH1      6         // Hours columns
#define PIN_COLH2      9
#define PIN_COLM1      10        // Minutes columns
#define PIN_COLM2      11
#define PIN_COLS1      12        // Seconds columns
#define PIN_COLS2      13
#define PIN_DATA1      A0        // Data LSB row
#define PIN_DATA2      A1        //
#define PIN_DATA3      A2        //
#define PIN_DATA4      A3        // Data MSB row

// Switches
#define PIN_MODESEL    A4        // SW0 - LOW = 12 hour mode, HIGH = 24 hour mode
#define PIN_CHIME      A5        // SW1 - LOW = enable hourly chimes in morse code
#define PIN_SWSPARE    36        // SW2 - Spare switch input
#define PIN_DATETIME   35        // SW3 - LOW = show date instead of time
#define PIN_MORSETIME  37        // SW4 - LOW = send time in morse code

// Other outputs
#define PIN_BEEP       4         // Output pin for morse code beeper - HIGH = beep, LOW = quiet

#define PIN_BOOT       0         // Boot button

#endif

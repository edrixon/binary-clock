#include "config.h"

#ifdef __MK1_HW
#include <WiFi101.h>

// for wifi version stuff
#include <driver/source/nmasic.h>
#else
#include <WiFi.h>
#endif

#ifdef __MK2_HW
#include <FFat.h>
#endif

#include "types.h"
#include "cli.h"
#include "globals.h"
#include "webserver.h"

#define BS  0x08
#define CR  0x0d
#define LF  0x0a
#define DEL 0x7f

cmdType cmdList[] =
{
#ifdef __MK2_HW
    { "cat",       cmdTypeFile },
#endif
    { "clear",     cmdClearConfig },
#ifdef __MK2_HW
    { "cp",        cmdCopy },
#endif
    { "display",   cmdDisplay },
#ifdef __MK2_HW
    { "format", cmdFormat },
    { "ftpuser", cmdFtpUsername },
    { "ftppassword", cmdFtpPassword },
    { "hd", cmdDump },
#endif
    { "help",      cmdListCommands },
#ifdef __MK2_HW
    { "hostname",  cmdHostname },
#endif
    { "initupdate", cmdInitUpdate },
    { "load",      cmdGetConfig },
#ifdef __MK2_HW
    { "ls",       cmdDirectory },
    { "mv",       cmdRename },
#endif
    { "ntpserver", cmdNtpServer },
    { "password",  cmdPassword },
#ifdef __MK2_HW
    { "rm", cmdDelete },
#endif
    { "save",      cmdSaveConfig },
    { "show",      cmdShowState },
    { "ssid",      cmdSsid },
    { "syncupdate", cmdSyncUpdate },
    { "syncvalid", cmdSyncValid },
#ifdef __WITH_HTTP
    { "webconfig", cmdWebConfig },
#endif
#ifdef __MK1_HW
    { "wifiver",   cmdWiFiVersion },
#endif
    { "?",         cmdListCommands },
    { NULL,        NULL }
};

// true if in 12 hour mode
boolean mode12()
{
    if(digitalRead(PIN_MODESEL) == LOW)
    {
        return true;
    }
    else
    {
        return false;
    }
}

// true if hourly chimes required
boolean chimesEnabled()
{
    if(digitalRead(PIN_CHIME) == LOW)
    {
        return true;
    }
    else
    {
        return false;
    }
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

void splitDigit(int x, volatile int *digPtr)
{
    *digPtr = (x / 10);
    digPtr++;
    *digPtr = (x % 10);
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
        sprintf(wifiStr, "  RSSI      : %d dBm", rssi);
        Serial.println(wifiStr);
        Serial.print("  IP        : ");
        Serial.println(ip);
        Serial.print("  Netmask   : ");
        Serial.println(mask);
        Serial.print("  Gateway   : ");
        Serial.println(gway);
    }
}

void serialShowTime(timeNow_t *timeStruct, char *timeName)
{
    int screen;
    int digit;
    char timeString[30];

    if(ntpSyncState == HIGH)
    {
        Serial.print("[SYNC] ");
    }
    else
    {
        Serial.print("[INIT] ");
    }

    sprintf(timeString, "%d-%d   %s %02d/%02d/%02d", reSyncCount, ticks, dayStrings[timeStruct -> tm_wday], timeStruct -> tm_mday, timeStruct -> tm_mon, timeStruct -> tm_year);
    Serial.print(timeString);
                  
    Serial.print(" - ");

    sprintf(timeString, "%02d:%02d:%02d %s", timeStruct -> tm_hour, timeStruct -> tm_min, timeStruct -> tm_sec, timeName);
    Serial.println(timeString);    
}

// Read a line from serial port up to '\n' character
// seperate command and parameter into two strings by replacing space with '\0'
// set paramPtr to point to parameter if present
void serialReadline()
{
    int c;
    int inchar;
    boolean eoln;

    eoln = false;
    paramCount = 0;

    for(c = 0; c < MAX_PARAMS; c++)
    {
        paramPtr[c] = NULL;
    }

    c = 0;
    do
    {
        if(Serial.available() > 0)
        {
            inchar = Serial.read();
            switch(inchar)
            {
                case DEL:;
                case BS:
                    if(c)
                    {
                        Serial.write(BS);
                        Serial.write(' ');
                        Serial.write(BS);
                            
                        c--;
                    }
                    break;
                        
                case ' ':
                    Serial.write(' ');
                    serialBuff[c] = '\0';
                    c++;
                    paramPtr[paramCount] = &serialBuff[c];
                    paramCount++;
                    break;

                case CR:;
                case LF:
                    Serial.println("");
                    serialBuff[c] = '\0';
                    eoln = true;
                    break;
                       
                default:
                    Serial.write(inchar);
                    serialBuff[c] = inchar;
                    c++;
            }
        }

        if(c == SERBUFF_LEN)
        {
            serialBuff[SERBUFF_LEN - 1] = '\0';
        }
    }
    while(c < SERBUFF_LEN && eoln == false && paramCount < MAX_PARAMS);
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
    if(paramPtr[0] != NULL)
    {
         strncpy(clockConfig.ssid, paramPtr[0], 40);
    }

    Serial.print("SSID: ");
    Serial.print(clockConfig.ssid);
    Serial.println("");
}

void cmdDisplay()
{
    int c;
    char *dPtr;
    
    if(paramPtr[0] == NULL)
    {
        Serial.println("display <six lowercase hex digits without spaces>");
    }
    else
    {
        if(strlen(paramPtr[0]) != 6)
        {
            Serial.println("Need 6 digits");
        }
        else
        {
            dPtr = paramPtr[0];
            for(c = 0; c < 6; c++)
            {
                if(isdigit(*dPtr))
                {
                    ledColData[c] = *dPtr - 48;
                }
                else
                {
                    ledColData[c] = *dPtr - 87;
                }
                dPtr++;
            }
        }
    }
}

void cmdPassword()
{
    if(paramPtr[0] != NULL)
    {
        strncpy(clockConfig.password, paramPtr[0], 64);
    }

    Serial.print("Password: ");
    Serial.print(clockConfig.password);
    Serial.println("");
}

void cmdNtpServer()
{
    if(paramPtr[0] != NULL)
    {
         strncpy(clockConfig.ntpServer, paramPtr[0], 32);
    }

    Serial.print("NTP server: ");
    Serial.print(clockConfig.ntpServer);
    Serial.println("");
}

void cmdShowState()
{
    char tmpStr[32];
    
    printWifiStatus();

    Serial.println("Running configuration");
    Serial.print("  SSID             : ");
    Serial.println(clockConfig.ssid);
    Serial.print("  Password         : ");
    Serial.println(clockConfig.password);
    Serial.print("  NTP server       : ");
    Serial.println(clockConfig.ntpServer);
    Serial.print("  Clock hostname   : ");
    Serial.println(clockConfig.hostName);
    Serial.print("  Updates for sync : ");
    Serial.println(clockConfig.syncValid);
    Serial.print("  Initial update   : ");
    Serial.print(clockConfig.initUpdate);
    Serial.println(" seconds");
    Serial.print("  Update period    : ");
    Serial.print(clockConfig.syncUpdate);
    Serial.println(" seconds");
        
    Serial.println("");
    
    Serial.print("Interrupt count: ");
    Serial.print(interruptCount);
    Serial.println("");

    sprintf(tmpStr, "Reachability: 0x%02x (0b", reachability);
    Serial.print(tmpStr);
    binToStr(reachability, tmpStr);
    Serial.print(tmpStr);
    Serial.println(")");

    Serial.print("Resync count: ");
    Serial.print(reSyncCount);
    Serial.println(" today");

    Serial.print("Mode: ");
    if(mode12() == true)
    {
        Serial.println("12 hour");
    }
    else
    {
        Serial.println("24 hour");
    }
    
    Serial.print("Display: ");
    if(digitalRead(PIN_DATETIME) == LOW)
    {
        Serial.println("Date");
    }
    else
    {
        Serial.println("Time");
    }

    Serial.print("Hourly chimes: ");
    if(chimesEnabled() == true)
    {
        Serial.println("Enabled");
    }
    else
    {
        Serial.println("Disabled");
    }

}

void cmdInitUpdate()
{
    if(paramPtr[0] != NULL)
    {
        clockConfig.initUpdate = atoi(paramPtr[0]);
    }

    Serial.print("Initial update period: ");
    Serial.print(clockConfig.initUpdate);
    Serial.println(" seconds");
}

void cmdSyncUpdate()
{
    if(paramPtr[0] != NULL)
    {
        clockConfig.syncUpdate = atoi(paramPtr[0]);
    }

    Serial.print("Update period once sync'ed: ");
    Serial.print(clockConfig.syncUpdate);
    Serial.println(" seconds");
}

void cmdSyncValid()
{
    if(paramPtr[0] != NULL)
    {
        clockConfig.syncValid = atoi(paramPtr[0]);
    }

    Serial.print("Sync's required before time valid: ");
    Serial.println(clockConfig.syncValid);
}

#ifdef __MK1_HW

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

#else

void cmdCopy()
{
    File fp1;
    File fp2;
    unsigned char cpBuff[64];
    int readed;

    if(paramCount != 2)
    {
        Serial.printf("cp <filename> <new filename>\r\n");
    }
    else
    {
        fp1 = FFat.open(paramPtr[0]);
        if(fp1)
        {
            fp2 = FFat.open(paramPtr[1], FILE_WRITE);
            if(fp2)
            {
                do
                {
                    readed = fp1.read(cpBuff, 64);
                    fp2.write(cpBuff, readed);
                }
                while(readed == 64);
                fp2.close();
            }
            else
            {
                Serial.printf("Can't open %s for writing\r\n", paramPtr[1]);
            }
            fp1.close();
        }
        else
        {
            Serial.printf("Can't open %s for reading\r\n", paramPtr[0]);
        }
    }
    
    Serial.printf("Copy %s to %s\r\n", paramPtr[0], paramPtr[1]);
}

void cmdDelete()
{
    if(paramPtr[0] == NULL)
    {
        Serial.println("rm <filename>");  
    }
    else
    {
        if(FFat.remove(paramPtr[0]))
        {
            Serial.printf("Deleted %s\r\n", paramPtr[0]);
        }
        else
        {
            Serial.printf("Failed to delete %s\r\n", paramPtr[0]);
        }
    }
}

void cmdDump()
{
    File fp;
    unsigned char dBuff[16];
    int offset;
    int readed;
    int c;

    fp = FFat.open(paramPtr[0]);
    if(fp)
    {
        offset = 0;
        do
        {
            readed = fp.read(dBuff, 16);

            Serial.printf("%04x   ", offset);
            for(c = 0; c < readed; c++)
            {
                Serial.printf("%02x ", dBuff[c]);
            }
            Serial.print("   :");
            for(c = 0; c < readed; c++)
            {
                if(isprint(dBuff[c]))
                {
                    Serial.printf("%c", dBuff[c]);
                }
                else
                {
                    Serial.print(".");
                }
            }
            Serial.println(":");
            offset = offset + 16;
        }
        while(readed == 16);
        fp.close();
    }
    else
    {
        Serial.printf("Can't open %s for reading\r\n", paramPtr[0]);
    }
}

void cmdRename()
{
    if(paramCount != 2)
    {
        Serial.println("mv <filename> <new filename>");
    }
    else
    {
        if(FFat.rename(paramPtr[0], paramPtr[1]))
        {
            Serial.printf("Renamed %s to %s\r\n", paramPtr[0], paramPtr[1]);
        }
    }
}

void cmdFormat()
{
    FFat.end();
    if(FFat.format() == true)
    {
        Serial.println("Filesystem formatted");
    }
    else
    {
        Serial.println("Failed to format");
    }
    
    if(FFat.begin() == true)
    {
        Serial.println("Mounted filesystem");
    }
}

void cmdDirectory()
{
    File entry;
    File dir;
    boolean done;

    done = false;
    dir = FFat.open("/");
    Serial.println("Directory listing of /");
    do
    {
        entry = dir.openNextFile();
        if(entry)
        {
            Serial.printf(" %s", entry.name());
            if(entry.isDirectory())
            {
                Serial.println("/");
            }
            else
            {
                Serial.printf("\t\t%d\r\n", entry.size());
            }

            entry.close();
        }
        else
        {
            done = true;
        }
    }
    while(done == false);

    dir.close();

    Serial.println("");
    Serial.printf("Used space %ld bytes\r\n", FFat.usedBytes());
    Serial.printf("Total space %ld bytes\r\n", FFat.totalBytes());
    Serial.printf("Free space %ld bytes\r\n", FFat.freeBytes());

}

void cmdHostname()
{
    if(paramPtr[0] != NULL)
    {
         strncpy(clockConfig.hostName, paramPtr[0], 32);
    }

    Serial.print("Hostname: ");
    Serial.print(clockConfig.hostName);
    Serial.println("");
}

void cmdFtpUsername()
{
    if(paramPtr[0] != NULL)
    {
        strncpy(clockConfig.ftpUser, paramPtr[0], 32);
    }

    Serial.print("FTP username: ");
    Serial.println(clockConfig.ftpUser);
}


void cmdFtpPassword()
{
    if(paramPtr[0] != NULL)
    {
        strncpy(clockConfig.ftpPassword, paramPtr[0], 32);
    }

    Serial.print("FTP password: ");
    Serial.println(clockConfig.ftpPassword);
}


void cmdTypeFile()
{
    File fp;
    unsigned char dBuff[16];
    int readed;
    int c;

    fp = FFat.open(paramPtr[0]);
    if(fp)
    {
        do
        {
            readed = fp.read(dBuff, 16);

            for(c = 0; c < readed; c++)
            {
                Serial.printf("%c", dBuff[c]);
            }
        }
        while(readed == 16);
        fp.close();
        Serial.println("");
    }
    else
    {
        Serial.printf("Can't open %s for reading\r\n", paramPtr[0]);
    }
}

#endif

#ifdef __WITH_HTTP

void cmdWebConfig()
{
    Serial.println(" - Stopping WiFi client");
#ifdef __MK1_HW
    WiFi.end();
#else
    WiFi.disconnect();
#endif
    httpStartAP();
    httpWebServer();
    Serial.println(" - Stopping Wifi access point");
#ifdef __MK1_HW
    WiFi.end();
#else
    WiFi.disconnect();
#endif
}

#endif

// List all available commands
void cmdListCommands()
{
    int c;
    
    c = 0;
    while(cmdList[c].cmdName != NULL)
    {
        Serial.print(cmdList[c].cmdName);
        Serial.println("");

        c++;
    }

    Serial.println("");
    Serial.println("'exit' to finish");
}

void commandInterpretter()
{
    int cmd;
    int done;

    Serial.print(HELLO_STR);
    Serial.println("");

    // Something other than the time...
    ledColData[0] = 0;
    ledColData[1] = 2;
    ledColData[2] = 2;
    ledColData[3] = 7;
    ledColData[4] = 2;
    ledColData[5] = 2;

    done = false;
    do
    {
        Serial.print(CLI_PROMPT);
        serialReadline();
 
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
                    Serial.println("Bad command");
                }
                else
                {
                    cmdList[cmd].fn();
                }
            }
        }
    }
    while(done == false);

    Serial.println("CLI exit");
}

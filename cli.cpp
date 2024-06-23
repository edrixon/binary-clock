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
#ifdef __WITH_TELNET_CLI
#include <ESPTelnet.h>
#endif
#endif

#include "types.h"
#include "cli.h"
#include "globals.h"
#include "webserver.h"
#include "util.h"

#ifdef __WITH_TELNET_CLI
#define CLI_DEV telnet
#else
#define CLI_DEV Serial
#endif

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
	{ "hw", cmdShowHardware },
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
    { "reboot", cmdReboot },
    { "rm", cmdDelete },
#endif
    { "save",      cmdSaveConfig },
    { "show",      cmdShowState },
    { "ssid",      cmdSsid },
    { "syncupdate", cmdSyncUpdate },
    { "syncvalid", cmdSyncValid },
    { "time", cmdShowTime },
    { "ver", cmdShowVersion },
#ifdef __WITH_HTTP
    { "webconfig", cmdWebConfig },
#endif
#ifdef __MK1_HW
    { "wifiver",   cmdWiFiVersion },
#endif
    { "?",         cmdListCommands },
    { NULL,        NULL }
};

void printWifiStatus()
{
    long rssi;
    IPAddress ip;
    IPAddress mask;
    IPAddress gway;
    char wifiStr[32];
  
    if(WiFi.status() != WL_CONNECTED)
    {
        CLI_DEV.println("Wifi disconnected");
    }
    else
    {
        ip = WiFi.localIP();
        mask = WiFi.subnetMask();
        gway = WiFi.gatewayIP();
        rssi = WiFi.RSSI();

        CLI_DEV.print("Wifi connected to ");
        CLI_DEV.println(clockConfig.ssid);
        sprintf(wifiStr, "  RSSI      : %d dBm", rssi);
        CLI_DEV.println(wifiStr);
        CLI_DEV.print("  IP        : ");
        CLI_DEV.println(ip);
        CLI_DEV.print("  Netmask   : ");
        CLI_DEV.println(mask);
        CLI_DEV.print("  Gateway   : ");
        CLI_DEV.println(gway);
    }
}

#ifndef __WITH_TELNET_CLI

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

#endif

void cmdShowTime()
{
    char timeString[80];
    
    sprintf(timeString, "%s %02d/%02d/%02d", dayStrings[timeNow.tm_wday], timeNow.tm_mday, timeNow.tm_mon, timeNow.tm_year);
    CLI_DEV.print(timeString);
                  
    CLI_DEV.print(" - ");

    sprintf(timeString, "%02d:%02d:%02d", timeNow.tm_hour, timeNow.tm_min, timeNow.tm_sec);
    CLI_DEV.println(timeString);    
}

void cmdSaveConfig()
{
    saveClockConfig();
    CLI_DEV.println("Running configuration saved");
}

void cmdClearConfig()
{
    memset(&clockConfig, 0, sizeof(clockConfig));

    CLI_DEV.println("Running configuration erased");
}

void cmdGetConfig()
{
    getClockConfig();
    CLI_DEV.println("Loaded saved configuration");
}

void cmdSsid()
{
    if(paramPtr[0] != NULL)
    {
         strncpy(clockConfig.ssid, paramPtr[0], 40);
    }

    CLI_DEV.print("SSID: ");
    CLI_DEV.print(clockConfig.ssid);
    CLI_DEV.println("");
}

void cmdDisplay()
{
    int c;
    char *dPtr;
    
    if(paramPtr[0] == NULL)
    {
        CLI_DEV.println("display <six lowercase hex digits without spaces>");
    }
    else
    {
        if(strlen(paramPtr[0]) != 6)
        {
            CLI_DEV.println("Need 6 digits");
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

    CLI_DEV.print("Password: ");
    CLI_DEV.print(clockConfig.password);
    CLI_DEV.println("");
}

void cmdNtpServer()
{
    if(paramPtr[0] != NULL)
    {
         strncpy(clockConfig.ntpServer, paramPtr[0], 32);
    }

    CLI_DEV.print("NTP server: ");
    CLI_DEV.print(clockConfig.ntpServer);
    CLI_DEV.println("");
}

void cmdShowState()
{
    char tmpStr[32];
    
    printWifiStatus();

    CLI_DEV.println("Running configuration");
    CLI_DEV.print("  SSID             : ");
    CLI_DEV.println(clockConfig.ssid);
    CLI_DEV.print("  Password         : ");
    CLI_DEV.println(clockConfig.password);
    CLI_DEV.print("  NTP server       : ");
    CLI_DEV.println(clockConfig.ntpServer);
    CLI_DEV.print("  Clock hostname   : ");
    CLI_DEV.println(clockConfig.hostName);
    CLI_DEV.print("  Updates for sync : ");
    CLI_DEV.println(clockConfig.syncValid);
    CLI_DEV.print("  Initial update   : ");
    CLI_DEV.print(clockConfig.initUpdate);
    CLI_DEV.println(" seconds");
    CLI_DEV.print("  Update period    : ");
    CLI_DEV.print(clockConfig.syncUpdate);
    CLI_DEV.println(" seconds");
        
    CLI_DEV.println("");
    
    CLI_DEV.print("Interrupt count: ");
    CLI_DEV.print(interruptCount);
    CLI_DEV.println("");

    sprintf(tmpStr, "Reachability: 0x%02x (0b", reachability);
    CLI_DEV.print(tmpStr);
    binToStr(reachability, tmpStr);
    CLI_DEV.print(tmpStr);
    CLI_DEV.println(")");

    CLI_DEV.print("Resync count: ");
    CLI_DEV.print(reSyncCount);
    CLI_DEV.println(" today");

    CLI_DEV.print("Mode: ");
    if(mode12() == true)
    {
        CLI_DEV.println("12 hour");
    }
    else
    {
        CLI_DEV.println("24 hour");
    }
    
    CLI_DEV.print("Display: ");
    if(digitalRead(PIN_DATETIME) == LOW)
    {
        CLI_DEV.println("Date");
    }
    else
    {
        CLI_DEV.println("Time");
    }

    CLI_DEV.print("Hourly chimes: ");
    if(chimesEnabled() == true)
    {
        CLI_DEV.println("Enabled");
    }
    else
    {
        CLI_DEV.println("Disabled");
    }

}

void cmdInitUpdate()
{
    if(paramPtr[0] != NULL)
    {
        clockConfig.initUpdate = atoi(paramPtr[0]);
    }

    CLI_DEV.print("Initial update period: ");
    CLI_DEV.print(clockConfig.initUpdate);
    CLI_DEV.println(" seconds");
}

void cmdSyncUpdate()
{
    if(paramPtr[0] != NULL)
    {
        clockConfig.syncUpdate = atoi(paramPtr[0]);
    }

    CLI_DEV.print("Update period once sync'ed: ");
    CLI_DEV.print(clockConfig.syncUpdate);
    CLI_DEV.println(" seconds");
}

void cmdSyncValid()
{
    if(paramPtr[0] != NULL)
    {
        clockConfig.syncValid = atoi(paramPtr[0]);
    }

    CLI_DEV.print("Sync's required before time valid: ");
    CLI_DEV.println(clockConfig.syncValid);
}

void cmdShowVersion()
{
    CLI_DEV.printf("Software version %s, %s\r\n", SW_VER, SW_DATE);
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

    CLI_DEV.print("Latest firmware: ");
    CLI_DEV.println(latestFw);

    CLI_DEV.print("Current verstion: ");
    CLI_DEV.print(fv);
    if(fv >= latestFw)
    {
        CLI_DEV.println(" (OK)");
    }
    else
    {
        CLI_DEV.println(" (REQUIRES UPDATE)");
    }
}

#else

void cmdShowHardware()
{
    CLI_DEV.printf("MK2 hardware (Adafruit ESP32-S3)");
    CLI_DEV.printf("ESP32 chip model %s, revision %d, %d cores\r\n", ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores());
    CLI_DEV.printf("With %ld bytes of FLASH\r\n", ESP.getFlashChipSize());
}

void cmdReboot()
{
    CLI_DEV.printf("Rebooting...\r\n");
    ESP.restart();
}

void cmdCopy()
{
    File fp1;
    File fp2;
    unsigned char cpBuff[64];
    int readed;

    if(paramCount != 2)
    {
        CLI_DEV.printf("cp <filename> <new filename>\r\n");
        return;
    }

    fp1 = FFat.open(paramPtr[0], FILE_READ);
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
            CLI_DEV.printf("Can't open %s for writing\r\n", paramPtr[1]);
        }
        fp1.close();
    }
    else
    {
        CLI_DEV.printf("Can't open %s for reading\r\n", paramPtr[0]);
    }
    
    CLI_DEV.printf("Copy %s to %s\r\n", paramPtr[0], paramPtr[1]);
}

void cmdDelete()
{
    if(paramPtr[0] == NULL)
    {
        CLI_DEV.println("rm <filename>");  
    }
    else
    {
        if(FFat.remove(paramPtr[0]))
        {
            CLI_DEV.printf("Deleted %s\r\n", paramPtr[0]);
        }
        else
        {
            CLI_DEV.printf("Failed to delete %s\r\n", paramPtr[0]);
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

    if(paramPtr[0] == NULL)
    {
        CLI_DEV.println("hd <filename>");
        return;
    }

    fp = FFat.open(paramPtr[0], FILE_READ);
    if(fp)
    {
        offset = 0;
        do
        {
            readed = fp.read(dBuff, 16);

            CLI_DEV.printf("%04x   ", offset);
            for(c = 0; c < readed; c++)
            {
                CLI_DEV.printf("%02x ", dBuff[c]);
            }
            CLI_DEV.print("   :");
            for(c = 0; c < readed; c++)
            {
                if(isprint(dBuff[c]))
                {
                    CLI_DEV.printf("%c", dBuff[c]);
                }
                else
                {
                    CLI_DEV.print(".");
                }
            }
            CLI_DEV.println(":");
            offset = offset + 16;
        }
        while(readed == 16);
        fp.close();
    }
    else
    {
        CLI_DEV.printf("Can't open %s for reading\r\n", paramPtr[0]);
    }
}

void cmdRename()
{
    if(paramCount != 2)
    {
        CLI_DEV.println("mv <filename> <new filename>");
    }
    else
    {
        if(FFat.rename(paramPtr[0], paramPtr[1]))
        {
            CLI_DEV.printf("Renamed %s to %s\r\n", paramPtr[0], paramPtr[1]);
        }
    }
}

void cmdFormat()
{
    FFat.end();
    if(FFat.format() == true)
    {
        CLI_DEV.println("Filesystem formatted");
    }
    else
    {
        CLI_DEV.println("Failed to format");
    }
    
    if(FFat.begin() == true)
    {
        CLI_DEV.println("Mounted filesystem");
    }
}

void cmdDirectory()
{
    File entry;
    File dir;
    boolean done;

    done = false;
    dir = FFat.open("/");
    CLI_DEV.println("Directory listing of /");
    do
    {
        entry = dir.openNextFile();
        if(entry)
        {
            CLI_DEV.printf(" %s", entry.name());
            if(entry.isDirectory())
            {
                CLI_DEV.println("/");
            }
            else
            {
                CLI_DEV.printf("\t\t%d\r\n", entry.size());
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

    CLI_DEV.println("");
    CLI_DEV.printf("Used space %ld bytes\r\n", FFat.usedBytes());
    CLI_DEV.printf("Total space %ld bytes\r\n", FFat.totalBytes());
    CLI_DEV.printf("Free space %ld bytes\r\n", FFat.freeBytes());

}

void cmdHostname()
{
    if(paramPtr[0] != NULL)
    {
         strncpy(clockConfig.hostName, paramPtr[0], 32);
    }

    CLI_DEV.print("Hostname: ");
    CLI_DEV.print(clockConfig.hostName);
    CLI_DEV.println("");
}

void cmdFtpUsername()
{
    if(paramPtr[0] != NULL)
    {
        strncpy(clockConfig.ftpUser, paramPtr[0], 32);
    }

    CLI_DEV.print("FTP username: ");
    CLI_DEV.println(clockConfig.ftpUser);
}


void cmdFtpPassword()
{
    if(paramPtr[0] != NULL)
    {
        strncpy(clockConfig.ftpPassword, paramPtr[0], 32);
    }

    CLI_DEV.print("FTP password: ");
    CLI_DEV.println(clockConfig.ftpPassword);
}


void cmdTypeFile()
{
    File fp;
    unsigned char dBuff[16];
    int readed;
    int c;

    fp = FFat.open(paramPtr[0], FILE_READ);
    if(fp)
    {
        do
        {
            readed = fp.read(dBuff, 16);

            for(c = 0; c < readed; c++)
            {
                CLI_DEV.printf("%c", dBuff[c]);
            }
        }
        while(readed == 16);
        fp.close();
        CLI_DEV.println("");
    }
    else
    {
        CLI_DEV.printf("Can't open %s for reading\r\n", paramPtr[0]);
    }
}

#endif

#ifdef __WITH_HTTP

void cmdWebConfig()
{
    CLI_DEV.println("Entering web configuration mode...");
    
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
        CLI_DEV.print(cmdList[c].cmdName);
        CLI_DEV.println("");

        c++;
    }

    CLI_DEV.println("");
    CLI_DEV.println("'exit' to finish");
}

void commandInterpretter()
{
    int cmd;
    int done;

    CLI_DEV.print(HELLO_STR);
    CLI_DEV.println("");

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
        CLI_DEV.print(CLI_PROMPT);
		
#ifdef __WITH_TELNET_CLI
        telnetReadline();
#else
        serialReadline();
#endif
 
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
                    CLI_DEV.println("Bad command");
                }
                else
                {
                    cmdList[cmd].fn();
                }
            }
        }
    }
    while(done == false);

    CLI_DEV.println("CLI exit");
#ifdef __WITH_TELNET_CLI
    telnet.disconnectClient();
#endif
}

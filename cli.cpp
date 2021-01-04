#include <WiFi101.h>

#include <driver/source/nmasic.h>

#include "config.h"
#include "types.h"
#include "cli.h"
#include "globals.h"
#include "webserver.h"

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

#ifdef __WITH_HTTP

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

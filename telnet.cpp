#include "config.h"

#ifdef __MK1_HW
#include <WiFi101.h>
#else
#include <WiFi.h>
#ifdef __WITH_TELNET_CLI
#include <ESPTelnet.h>
#endif
#endif

#include "types.h"
#include "globals.h"
#include "telnet.h"
#include "cli.h"
#include "util.h"

#ifdef __WITH_TELNET_CLI

#define TELNET_RX_BUFFER_SIZE 128

boolean gotLine;
char telnetRxBuffer[TELNET_RX_BUFFER_SIZE];

void telnetLoadSerialBuffer()
{
    char *sBuffPtr;
    char *tBuffPtr;
    boolean eoln;

    sBuffPtr = serialBuff;
    tBuffPtr = telnetRxBuffer;

    eoln = false;
    do
    {
        switch(*tBuffPtr)
        {
            case NUL:;
            case CR:;
            case LF:
                *sBuffPtr = '\0';
                eoln = true;
                break;
                
            case BS:;
            case DEL:
                if(sBuffPtr != serialBuff)
                {
                    sBuffPtr--;
                }
                break;
                
            default:
                *sBuffPtr = *tBuffPtr;
                sBuffPtr++;
        }

        tBuffPtr++;
    }
    while(eoln == false);
}

void telnetOnConnect(String ip)
{
    Serial.print("[TNET] ");
    Serial.print(ip);
    Serial.println(" has connected");
    
    telnet.printf("\r\nConnected to %s\r\n", clockConfig.hostName);

    newTelnetConnection = true;
}

void telnetOnDisconnect(String ip)
{
    Serial.println("[TNET] Disconnected");
    gotLine = false;
}

void telnetOnInputReceived(String str)
{
    int c;
    char *cPtr;

    gotLine = true;

    paramCount = 0;
    for(c = 0; c < MAX_PARAMS; c++)
    {
        paramPtr[c] = NULL;
    }
  
    // Copy str into serialBuffer[] and parameters into paramPtr[]
    strcpy(telnetRxBuffer, str.c_str());
    telnetLoadSerialBuffer();
    
    cPtr = strtok(serialBuff, " ");
    if(cPtr != NULL)
    {
        c = 0;
        do
        {
            cPtr = strtok(NULL, " ");
            if(cPtr != NULL)
            {
                paramPtr[c] = cPtr;
                paramCount++;
            }
            c++;
        }
        while(c < MAX_PARAMS && cPtr != NULL);
    }
}

void telnetReadline()
{
	while(gotLine == false)
	{
        telnet.loop();
	}
	
	gotLine = false;

}

void initTelnetServer()
{
    telnet.onConnect(telnetOnConnect);
    telnet.onDisconnect(telnetOnDisconnect);
    telnet.onInputReceived(telnetOnInputReceived);

    newTelnetConnection = false;
    gotLine = false;

    Serial.print(" - Telnet server ");
    if(telnet.begin(TELNET_PORT))
    {
        Serial.printf("(port %d)\r\n", TELNET_PORT);
    }
    else
    {
        Serial.println("failed to start");
    }
}

#endif

#ifdef __WITH_TELNET

void telnetShowStatus(WiFiClient client)
{
    int c;
    char buff[80];

    client.println(HELLO_STR);

    client.print("Software built for ");
#ifdef __MK1_HW
    client.println("MK1 hardware (MKR1000)");
#else
    client.println("MK2 hardware (Adafruit ESP32-S3)");

    sprintf(buff, "ESP32 chip model %s, revision %d, %d cores", ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores());
    client.println(buff);
#endif

    client.print("WiFi SSID: ");
    client.println(clockConfig.ssid);

    sprintf(buff, "Wifi RSSI: %d dBm", WiFi.RSSI());
    client.println(buff);

    client.println("Display data:");
    for(c = 0; c < 6; c++)
    {
        sprintf(buff, "  Digit %d - 0x%02x - ", c, ledColData[c]);
        client.print(buff);
        binToStr(ledColData[c], buff, 8);
        client.println(buff);
    }

    client.print("NTP server: ");
    client.println(clockConfig.ntpServer);

    sprintf(buff, "Reachability: 0x%04x (0b", reachability);
    client.print(buff);
    binToStr(reachability, buff, 16);
    strcat(buff, ")");
    client.println(buff);

    sprintf(buff, "Hourly chimes: ");
    if(chimesEnabled() == true)
    {
        strcat(buff, "Enabled");
    }
    else
    {
        strcat(buff, "Disabled");
    }
    client.println(buff);

    sprintf(buff, "Display mode: ");
    if(mode12() == true)
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

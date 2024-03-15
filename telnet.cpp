#include "config.h"

#ifdef __MK1_HW
#include <WiFi101.h>
#else
#include <WiFi.h>
#endif

#include "types.h"
#include "globals.h"
#include "telnet.h"
#include "cli.h"

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

    sprintf(buff, "ESP32 Chip model %s, revision %d, %d cores", ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores());
    client.println(buff);
#endif

    client.print("WiFi SSID: ");
    client.println(clockConfig.ssid);

    sprintf(buff, "Wifi RSSI: %d dBm", WiFi.RSSI());
    client.println(buff);

    client.println("Display data:");
    for(c = 0; c < 6; c++)
    {
        sprintf(buff, "  Digit %d - 0x%02x - ", c, ledDisplay.data[c]);
        client.print(buff);
        binToStr(ledDisplay.data[c], buff);
        client.println(buff);
    }

    client.print("NTP server: ");
    client.println(clockConfig.ntpServer);

    sprintf(buff, "Reachability: 0x%02x (0b", reachability);
    client.print(buff);
    binToStr(reachability, buff);
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

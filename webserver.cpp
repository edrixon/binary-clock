#include "config.h"

#ifdef __MK1_HW
#include <WiFi101.h>
#else
#include <WiFi.h>
#include <FFat.h>
#endif

#include "types.h"

#include "globals.h"
#include "util.h"

#ifdef __WITH_HTTP

#include "webserver.h"

// These can be anything...
IPAddress ap_ipaddr(192, 168, 1, 1);              // AP IP address and default route to give out
IPAddress ap_gw(192, 168, 1, 1);                  // Default gateway to give out to clients
IPAddress ap_netmask(255, 255, 255, 0);           // Netmask to give out to clients

char *httpParams[16];
char httpRequest[255];
boolean httpDone;

getRequestType configurationGetRequestList[] =
{
    { "/reset.html", httpResetConfiguration },
    { "/config.html", httpSaveConfiguration },
    { "/", httpConfigPage },
    { "/index.html", httpConfigPage },
    { NULL, NULL }
};

getRequestType normalGetRequestList[] =
{
    { "/", httpStatusPage },
    { "/index.html", httpStatusPage },
    { "/getClockState", httpClockState },
    { "/getLedData", httpLedData },
    { NULL, NULL }
};

httpParamType httpParamHandlers[] =
{
    { "ssid", httpSetSsid },
    { "password", httpSetPassword },
    { "ntpserver", httpSetNtpServer },
    { "initupdate", httpSetInitUpdate },
    { "syncupdate", httpSetSyncUpdate },
    { "syncvalid", httpSetSyncValid },
    { "hostname", httpSetHostName },
    { "ftpuser", httpSetFtpUsername },
    { "ftppassword", httpSetFtpPassword },
    { NULL, NULL }
};

void httpStartAP()
{
    IPAddress ip;

    Serial.println(" - Starting WiFi access point");

#ifdef __MK1_HW  
    // Start Wifi in AP mode
    if(WiFi.beginAP(AP_SSID) != WL_AP_LISTENING)
    {
        Serial.println(" - Failed");
    }
#else

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(ap_ipaddr, ap_gw, ap_netmask);
    WiFi.softAP(AP_SSID);	

#endif

    delay(5000);

#ifdef __MK1_HW
    ip = WiFi.localIP();
#else
    ip = WiFi.softAPIP();
#endif

    Serial.print(" - SSID: ");
    Serial.println(AP_SSID);

    Serial.print(" - IP address: ");
    Serial.println(ip);
}

void httpHeaderTop()
{
    httpClient.println("HTTP/1.1 200 OK");
    httpClient.println("Content-type:text/html");
    httpClient.println();  
}

void httpHeader()
{
    httpHeaderTop();
    httpClient.println("<html>");
    httpClient.println("<body>");
    
    httpClient.println("<h2>Configuration for NTP clock by Ed Rixon, GD6XHG</h2>");  
}

void httpFooter()
{
    httpClient.println("<p><a href=\"/\">Back</a> to main page</p>");
    httpClient.println("</body>");
    httpClient.println("</html>");  
}

void httpConfigPage()
{
    httpHeader();
    
    httpClient.println("<form action=\"/config.html\">");
    httpClient.println("<label for=\"ssid\">WiFi SSID:</label><br>");
    httpClient.print("<input type=\"text\" id=\"ssid\" name=\"ssid\" value=\"");
    httpClient.print(clockConfig.ssid);
    httpClient.println("\"><br><br>");
    httpClient.println("<label for=\"password\">WiFi Password:</label><br>");
    httpClient.print("<input type=\"text\" id=\"password\" name=\"password\" value=\"");
    httpClient.print(clockConfig.password);
    httpClient.println("\"><br><br>");
    httpClient.println("<label for=\"hostname\">Clock hostname:</label><br>");
    httpClient.print("<input type=\"text\" id=\"hostname\" name=\"hostname\" value=\"");
    httpClient.print(clockConfig.hostName);
    httpClient.println("\"><br><br>");
    httpClient.println("<label for=\"ftpuser\">FTP server username:</label><br>");
    httpClient.print("<input type=\"text\" id=\"ftpuser\" name=\"ftpuser\" value=\"");
    httpClient.print(clockConfig.ftpUser);
    httpClient.println("\"><br><br>");
    httpClient.println("<label for=\"ftppassword\">FTP server password:</label><br>");
    httpClient.print("<input type=\"text\" id=\"ftppassword\" name=\"ftppassword\" value=\"");
    httpClient.print(clockConfig.ftpPassword);
    httpClient.println("\"><br><br>");
    httpClient.println("<label for=\"ntpserver\">NTP server:</label><br>");
    httpClient.print("<input type=\"text\" id=\"ntpserver\" name=\"ntpserver\" value=\"");
    httpClient.print(clockConfig.ntpServer);
    httpClient.println("\"><br><br>");
    httpClient.println("<label for=\"syncupdate\">Normal update period:</label><br>");
    httpClient.print("<input type=\"text\" id=\"syncupdate\" name=\"syncupdate\" value=\"");
    httpClient.print(clockConfig.syncUpdate);
    httpClient.println("\"><br><br>");
    httpClient.println("<label for=\"initupdate\">Initial update period:</label><br>");
    httpClient.print("<input type=\"text\" id=\"initupdate\" name=\"initupdate\" value=\"");
    httpClient.print(clockConfig.initUpdate);
    httpClient.println("\"><br><br>");
    httpClient.println("<label for=\"syncvalid\">Initial sync's required:</label><br>");
    httpClient.print("<input type=\"text\" id=\"syncvalid\" name=\"syncvalid\" value=\"");
    httpClient.print(clockConfig.syncValid);
    httpClient.println("\"><br><br>");
    httpClient.println("<input type=\"submit\" value=\"Save settings\">");
    httpClient.println("</form>");

    httpClient.println("<br>");

    httpClient.println("<form action=\"/reset.html\">");
    httpClient.println("<input type=\"submit\" value=\"Default settings\">");
    httpClient.println("</form>");

    httpClient.println("</body>");
    httpClient.println("</html>");
}

void httpResetConfiguration()
{
    httpHeader();
    httpClient.println("<p>Load default configuration</p>");
    httpFooter();

#ifdef __USE_DEFAULTS
    defaultClockConfig();
#else
    memset(&clockConfig, 0, sizeof(clockConfig));
#endif
}

void httpSetSsid(char *ssid)
{
    strcpy(clockConfig.ssid, ssid); 
}

void httpSetHostName(char *hostName)
{
    strcpy(clockConfig.hostName, hostName); 
}

void httpSetFtpUsername(char *username)
{
    strcpy(clockConfig.ftpUser, username); 
}

void httpSetFtpPassword(char *password)
{
    strcpy(clockConfig.ftpPassword, password); 
}

void httpSetPassword(char *password)
{
    strcpy(clockConfig.password, password);
}

void httpSetNtpServer(char *ntpServer)
{
    strcpy(clockConfig.ntpServer, ntpServer);
}

void httpSetSyncUpdate(char *syncUpdate)
{
    clockConfig.syncUpdate = atoi(syncUpdate);  
}

void httpSetInitUpdate(char *initUpdate)
{
    clockConfig.initUpdate = atoi(initUpdate);  
}

void httpSetSyncValid(char *syncValid)
{
    clockConfig.syncValid = atoi(syncValid);  
}

void httpParseParam(char *paramName, char *paramValue)
{
    int c;

    Serial.print("Trying to set ");
    Serial.print(paramName);
    Serial.print(" to ");
    Serial.println(paramValue);

    c = 0;
    while(httpParamHandlers[c].paramName != NULL && strcmp(httpParamHandlers[c].paramName, paramName) != 0)
    {
        c++;
    }

    if(httpParamHandlers[c].paramName != NULL)
    {
        httpParamHandlers[c].fn(paramValue);
    }
    else
    {
        Serial.print("Bad parameter");
    }
}

void httpSaveConfiguration()
{
    int c;
    char *paramName;
    char *paramValue;
    
    httpHeader();
    httpClient.println("<p>Load configuration:</p><br>");

    c = 1;
    do
    {
        paramName = strtok(httpParams[c], "=");
        if(paramName != NULL)
        {
            paramValue = strtok(NULL, "=");
            if(paramValue != NULL)
            {
                httpClient.print("<p>Set ");
                httpClient.print(paramName);
                httpClient.print(" to ");
                httpClient.print(paramValue);
                httpClient.print("</p><br>");
        
                httpParseParam(paramName, paramValue);
                c++;
            }
        }
    }
    while(paramName != NULL && paramValue != NULL);
    
    httpFooter();

    saveClockConfig();
    httpDone = true;
}

#ifdef __MK1_HW

void httpStatusPage()
{
    httpBuiltinStatusPage();
}

#else

boolean httpGetRealFile(char *fName)
{
    File fp;
    unsigned char dBuff[128];
    int readed;

    fp = FFat.open(fName, FILE_READ);
    if(fp)
    {
        Serial.printf("Sending real file %s\r\n", fName);
        do
        {
            readed = fp.read(dBuff, 128);
            httpClient.write(dBuff, readed);
        }
        while(readed == 128);
        fp.close();
        httpClient.flush();

        return true;
    }
    else
    {
        Serial.printf("Can't open real file %s for reading\r\n", fName);
        return false;
    }
}

void httpStatusPage()
{
    if(httpGetRealFile("/index.html") == false)
    {
        httpBuiltinStatusPage();
    }
}

#endif

void httpNotFound()
{
    httpClient.println("HTTP/1.1 404 Not Found");
}

void httpHandleGetRequest(char *url, getRequestType *getRequests)
{
    char *tokPtr;
    char *filename;
    int c;
  
    c = 0;

    tokPtr = strtok(url, "?& ");
    while(tokPtr != NULL)
    {
        httpParams[c] = tokPtr;
        c++;

        tokPtr = strtok(NULL, "?& ");
    }

    Serial.print("GET ");
    Serial.println(httpParams[0]);

    c = 0;
    while(getRequests[c].fileName != NULL && strcmp(getRequests[c].fileName, httpParams[0]) != 0)
    {
        c++;
    }

    if(getRequests[c].fileName == NULL)
    {
#ifdef __MK1_HW
        httpNotFound();
#else
        if(httpGetRealFile(httpParams[0]) == false)
        {
            httpNotFound();
        }
#endif
    }
    else
    {
        getRequests[c].fn();
    }
}

void httpWebServer()
{
    char c;
    char *urlPtr;
    unsigned long int ms;
  
    // Start webserver
    httpServer.begin();

    httpDone = false;
    ms = millis();
    while(httpDone == false)
    {
        httpClient = httpServer.available();
        if(httpClient)
        {
            urlPtr = httpRequest;
            while(httpClient.connected())
            {
                if(httpClient.available())
                {
                    ms = millis();
                    c = httpClient.read();
                    if(c == '\n')
                    {
                        *urlPtr = '\0';

                        Serial.println(httpRequest);
                        
                        if(strncmp(httpRequest, "GET", 3) == 0)
                        {
                            httpHandleGetRequest(&httpRequest[4], configurationGetRequestList);
                        }

                        httpClient.flush();
                        httpClient.stop();
                    }
                    else
                    {
                        if(c != '\r')
                        {
                            *urlPtr = c;
                            urlPtr++;
                        }
                    }
                }
                else
                {
                    if(ms - millis() > 2000)
                    {
                        Serial.println("HTTP idle timeout");
                        httpClient.stop();
                    }
                }
            }

            Serial.println("Disconnected");
        }
    }
}

void httpBuiltinStatusPage()
{
    char c;
    char *filename;
    char txtBuff[80];

    httpClient.println("HTTP/1.1 200 OK");
    httpClient.println("Content-type:text/html\r\n");
  
    httpClient.println("<html>");

    httpClient.println("<head>");
    httpClient.println("<style>");
    httpClient.println("table, th, td {");
    httpClient.println("  border: 1px solid black;");
    httpClient.println("  border-collapse: collapse;");
    httpClient.println("  padding: 5px;");
    httpClient.println("  text-align: left;");
    httpClient.println("}");
    httpClient.println("</style>");
    httpClient.println("</head>");

    httpClient.println("<body>");
   
    httpClient.println("<h2>NTP Clock by Ed Rixon, GD6XHG</h2>");
          
    httpClient.println("<table>");
    httpClient.println("  <tr>");
    httpClient.println("    <th>Time</th>");
    sprintf(txtBuff, "    <td>%02d:%02d:%02d</td>", timeNow.tm_hour, timeNow.tm_min, timeNow.tm_sec);
    httpClient.println(txtBuff);
    httpClient.println("  </tr>");
    httpClient.println("  <tr>");
    httpClient.println("    <th>Date</th>");
    sprintf(txtBuff, "    <td>%02d/%02d/%04d</td>", timeNow.tm_mday, timeNow.tm_mon, timeNow.tm_year);
    httpClient.println(txtBuff);
    httpClient.println("  </tr>");
    httpClient.println("</table>");

    httpClient.println("<br>");
       
    httpClient.println("<table>");
    httpClient.println("  <tr>");
    httpClient.println("    <th>WiFi SSID</th>");
    sprintf(txtBuff, "    <td>%s</td>", clockConfig.ssid);
    httpClient.println(txtBuff);
    httpClient.println("  </tr>");
    httpClient.println("  <tr>");
    httpClient.println("    <th>WiFi RSSI</th>");
    sprintf(txtBuff, "    <td>%d dBm</td>", WiFi.RSSI());
    httpClient.println(txtBuff);
    httpClient.println("  </tr>");
#ifdef __MK2_HW
    httpClient.println("  <tr>");
    httpClient.println("    <th>WiFi channel</th>");
    sprintf(txtBuff, "    <td>%d</td>", WiFi.channel());
    httpClient.println(txtBuff);
    httpClient.println("  </tr>");
#endif
    httpClient.println("</table>");

    httpClient.println("<br>");
        
    httpClient.println("<table>");
    httpClient.println("  <tr>");
    httpClient.println("    <th>NTP server</th>");
    sprintf(txtBuff, "    <td>%s</td>", clockConfig.ntpServer);
    httpClient.println(txtBuff);
    httpClient.println("  </tr>");
    httpClient.println("  <tr>");
    httpClient.println("    <th>Reachability</th>");
    sprintf(txtBuff, "    <td>0x%08x</td>", reachability);
    httpClient.println(txtBuff);
    httpClient.println("  </tr>");
    httpClient.println("  <tr>");
    httpClient.println("    <th>Re-syncs</th>");
    sprintf(txtBuff, "    <td>%d</td>", reSyncCount);
    httpClient.println(txtBuff);
    httpClient.println("  </tr>");
    httpClient.println("</table>");

    httpClient.println("<br>");
        
    httpClient.println("<table>");
    httpClient.println("  <tr>");
    httpClient.println("    <th>Mode</th>");
    if(mode12() == true)
    {
        httpClient.println("    <td>12 hour</td>");
    }
    else
    {
        httpClient.println("    <td>24 hour</td>");          
    }
    httpClient.println("  </tr>");
    httpClient.println("  <tr>");
    httpClient.println("    <th>Hourly chimes</th>");
    if(chimesEnabled())
    {
        httpClient.println("    <td>Enabled</td>");
    }
    else
    {
        httpClient.println("    <td>Disabled</td>");          
    }
    httpClient.println("  </tr>");
    httpClient.println("</table>");

    httpClient.println("<p>");
    httpClient.println("Power up holding 'mode' or 'morse' button to enter configuration mode");
    httpClient.println("</p>");
    httpClient.println("<p>");
    httpClient.print("Clock will start WiFi access point with SSID '");
    httpClient.print(AP_SSID);
    httpClient.println("'<br>");
    httpClient.println("Connect to that to access configuration page at http://192.168.1.1");
    httpClient.println("</p>");
    httpClient.println("<p>");
    httpClient.println("Configuration can also be done using CLI on serial port at 9600 baud");
    httpClient.println("</p>");

    httpClient.println("</body>");
    httpClient.println("</html>");  
}

void httpHandlePostRequest(char *request)
{
    Serial.print("HTTP POST request - ");
    Serial.println(request);
}

void httpRequestHandler()
{
    char c;
    char *urlPtr;
    unsigned long int ms;

    Serial.println("Client connected to webserver");
  
    urlPtr = httpRequest;
    ms = millis();
    while(httpClient.connected())
    {
        if(httpClient.available())
        {
            ms = millis();
            c = httpClient.read();
            if(c == '\n')
            {
                *urlPtr = '\0';

                if(strncmp(httpRequest, "GET", 3) == 0)
                {
                    httpHandleGetRequest(&httpRequest[4], normalGetRequestList);
                }
                else
                {
                    if(strncmp(httpRequest, "POST", 4) == 0)
                    {
                        httpHandlePostRequest(&httpRequest[5]);
                    }
                }

                httpClient.flush();
                httpClient.stop();
            }
            else
            {
                if(c != '\r')
                {
                    *urlPtr = c;
                    urlPtr++;
                }
            }
        }
        else
        {
            if(millis() - ms > 2000)
            {
                Serial.println("HTTP idle timeout");
                httpClient.stop();
            }
        }
    }

    Serial.println("Disconnected");
}

void httpClockState()
{
    httpHeaderTop();
    httpClient.printf("%s|%d|%s|0x%08x|%d|", clockConfig.ssid, WiFi.RSSI(), clockConfig.ntpServer, reachability, reSyncCount);
    if(ntpSyncState == HIGH)
    {
        httpClient.print("YES");
    }
    else
    {
        httpClient.print("NO");
    }
    httpClient.print("|");

    if(chimesEnabled() == true)
    {
        httpClient.print("ENABLED");
    }
    else
    {
        httpClient.print("DISABLED");
    }
    httpClient.print("|");

    httpClient.printf("%ld|%s|%s", FFat.freeBytes(), SW_VER, SW_DATE);
}

void httpSplitLedData(int col)
{
    int x;
    int c;

    x = ledColData[col];

    for(c = 0; c < 4; c++)
    {
        if(x & 0x01 == 0x01)
        {
            httpClient.print("red");
        }
        else
        {
            httpClient.print("black");
        }

        if(c != 3)
        {
            httpClient.print("|");
        }

        x = x >> 1;
    }
}

void httpLedData()
{
    int col;
    int led;
    
    httpHeaderTop();

    for(col = 0; col < MAX_COLS; col++)
    {
        httpSplitLedData(col);
        httpClient.print("|");
    }

    if(mode12() == true && timeNow.tm_hour >= 12)
    {
        httpClient.print("green");
    }
    else
    {
        httpClient.print("black");
    }
    httpClient.print("|");

    if(ntpSyncState == HIGH)
    {
        httpClient.print("orange");
    }
    else
    {
        httpClient.print("black");
    }
}

#endif

#include <WiFi101.h>

#include "config.h"
#include "types.h"

#include "globals.h"

#ifdef __WITH_HTTP

#include "webserver.h"

char *httpParams[16];
char httpRequest[255];
boolean httpDone;

getRequestType getRequests[] =
{
    { "/reset.html", httpResetConfiguration },
    { "/config.html", httpSaveConfiguration },
    { "/", httpConfigPage },
    { NULL, NULL }
};

httpParamType httpParamHandlers[] =
{
    { "ssid", httpSetSsid },
    { "password", httpSetPassword },
    { "ntpserver", httpSetNtpServer },
    { NULL, NULL }
};




void httpStartAP()
{
    IPAddress ip;

    Serial.println(" - Starting WiFi access point");
  
    // Start Wifi in AP mode
    if(WiFi.beginAP("NTPClock") != WL_AP_LISTENING)
    {
        Serial.println(" - Failed");
    }
    delay(5000);

    ip = WiFi.localIP();
    Serial.print(" - IP address: ");
    Serial.println(ip);
}

void httpHeader()
{
    httpClient.println("HTTP/1.1 200 OK");
    httpClient.println("Content-type:text/html");
    httpClient.println();
  
    httpClient.println("<html>");
    httpClient.println("<body>");
    
    httpClient.println("<h2>NTP Clock configuration</h2>");  
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
    httpClient.println("<label for=\"ntpserver\">NTP server:</label><br>");
    httpClient.print("<input type=\"text\" id=\"ntpserver\" name=\"ntpserver\" value=\"");
    httpClient.print(clockConfig.ntpServer);
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
    strcpy(clockConfig.ssid, DEFAULT_SSID);
    strcpy(clockConfig.password, DEFAULT_PSWD);
    strcpy(clockConfig.ntpServer, DEFAULT_NTPS);
#else
    memset(&clockConfig, 0, sizeof(clockConfig));
#endif
}

void httpSetSsid(char *ssid)
{
    strcpy(clockConfig.ssid, ssid); 
}

void httpSetPassword(char *password)
{
    strcpy(clockConfig.password, password);
}

void httpSetNtpServer(char *ntpServer)
{
    strcpy(clockConfig.ntpServer, ntpServer);
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

    for(c = 1; c < 4; c++)
    {
        paramName = strtok(httpParams[c], "=");
        paramValue = strtok(NULL, "=");

        httpClient.print("<p>Set ");
        httpClient.print(paramName);
        httpClient.print(" to ");
        httpClient.print(paramValue);
        httpClient.print("</p><br>");
        
        httpParseParam(paramName, paramValue);
    }
    
    httpFooter();

    saveClockConfig();
    httpDone = true;
}

void httpNotFound()
{
    httpClient.println("HTTP/1.1 404 Not Found");
}

void httpHandleGetRequest(char *url)
{
    char *tokPtr;
    char *filename;
    int c;
  
    c = 0;

    tokPtr = strtok(url, "?& ");
    while(tokPtr != NULL)
    {
        httpParams[c] = tokPtr;
        Serial.print("Param ");
        Serial.print(c);
        Serial.print(": ");
        Serial.println(tokPtr);
        c++;

        tokPtr = strtok(NULL, "?& ");
    }

    c = 0;
    while(getRequests[c].fileName != NULL && strcmp(getRequests[c].fileName, httpParams[0]) != 0)
    {
        c++;
    }

    if(getRequests[c].fileName == NULL)
    {
        httpNotFound();
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
                            httpHandleGetRequest(&httpRequest[4]);
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

void httpStatusPage(char *url)
{
    char c;
    char *filename;
    char txtBuff[80];

    filename = strtok(url, "?& ");
    if(strcmp(filename, "/") != 0)
    {
        httpNotFound();
    }
    else
    {
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
    
        httpClient.println("<h2>NTP Clock Status</h2>");
          
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
        sprintf(txtBuff, "    <td>0x%02x</td>", reachability);
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
        if(digitalRead(dateTimePin) == LOW)
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
        if(digitalRead(disChimePin) == HIGH)
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
        httpClient.println("Clock will start WiFi access point with SSID 'NTPclock'<br>");
        httpClient.println("Connect to that to access configuration page at http://192.168.1.1");
        httpClient.println("</p>");
        httpClient.println("<p>");
        httpClient.println("Configuration can also be done using CLI on serial port at 9600 baud");
        httpClient.println("</p>");

        httpClient.println("</body>");
        httpClient.println("</html>");  
    }
}

void httpStatusGetRequest()
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

                Serial.println(httpRequest);
                        
                if(strncmp(httpRequest, "GET", 3) == 0)
                {
                    httpStatusPage(&httpRequest[4]);
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

#endif

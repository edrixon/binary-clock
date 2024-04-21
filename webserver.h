void httpStartAP();
void httpHeader();
void httpFooter();
void httpConfigPage();
void httpResetConfiguration();
void httpSetSsid(char *ssid);
void httpSetPassword(char *password);
void httpSetNtpServer(char *ntpServer);
void httpSetSyncUpdate(char *syncUpdate);
void httpSetInitUpdate(char *initUpdate);
void httpSetSyncValid(char *syncValid);
void httpSetHostName(char *hostName);
void httpSetFtpUsername(char *username);
void httpSetFtpPassword(char *password);
void httpParseParam(char *paramName, char *paramValue);
void httpSaveConfiguration();
void httpNotFound();
void httpHandleGetRequest(char *url, getRequestType *getRequests);
void httpWebServer();
void httpBuiltinStatusPage();
void httpStatusPage();
void httpRequestHandler();
void httpClockState();
void httpLedData();

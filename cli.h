boolean chimesEnabled();
boolean mode12();
void binToStr(int bin, char *str);
void splitDigit(int x, volatile int *digPtr);
void printWifiStatus();
void serialShowTime(timeNow_t *timeStruct, char *timeName);
void serialReadline();
void cmdSaveConfig();
void cmdClearConfig();
void cmdGetConfig();
void cmdSsid();
void cmdDisplay();
void cmdPassword();
void cmdNtpServer();
void cmdShowState();
void cmdInitUpdate();
void cmdSyncUpdate();
void cmdSyncValid();
#ifdef __MK1_HW
void cmdWiFiVersion();
#else
void cmdCopy();
void cmdDelete();
void cmdDirectory();
void cmdDump();
void cmdFormat();
void cmdRename();
void cmdHostname();
void cmdTypeFile();
#endif
void cmdWebConfig();
void cmdListCommands();
void commandInterpretter();

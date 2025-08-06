#ifndef STORAGE_H
#define STORAGE_H

#include "config.h"
#include <ArduinoJson.h>

struct WiFiCredentials {
    char ssid[32];
    char password[64];
    bool isConfigured;
};

void setupStorage();
void saveConfig(const AppConfig& config);
AppConfig loadConfig();
void saveWiFiCredentials(const WiFiCredentials& creds);
WiFiCredentials loadWiFiCredentials();
void clearWiFiCredentials();
void saveSessionLog(const JsonObject& log);
String getSessionLogs();
String getSessionLogsJson();
void deleteSessionLog(int index);
void deleteAllSessionLogs();

// IP vibration flag
void setVibrateIPFlag(bool shouldVibrate);
bool getVibrateIPFlag();

#endif // STORAGE_H 
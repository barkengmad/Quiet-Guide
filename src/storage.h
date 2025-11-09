#ifndef STORAGE_H
#define STORAGE_H

#include "config.h"
#include <ArduinoJson.h>

struct WiFiCredentials {
    char ssid[32];
    char password[64];
    bool isConfigured;
};

const int MAX_WIFI_NETWORKS = 5;

struct WiFiNetwork {
    char ssid[32];
    char password[64];
    int priority; // 0 = highest priority, 4 = lowest
};

struct WiFiNetworkList {
    WiFiNetwork networks[MAX_WIFI_NETWORKS];
    int count; // Number of configured networks (0-5)
};

void setupStorage();
void saveConfig(const AppConfig& config);
AppConfig loadConfig();
// Legacy single-network functions (for backward compatibility)
void saveWiFiCredentials(const WiFiCredentials& creds);
WiFiCredentials loadWiFiCredentials();
void clearWiFiCredentials();
// New multi-network functions
void saveWiFiNetworks(const WiFiNetworkList& networks);
WiFiNetworkList loadWiFiNetworks();
void clearWiFiNetworks();
void saveSessionLog(const JsonObject& log);
String getSessionLogs();
String getSessionLogsJson();
void deleteSessionLog(int index);
void deleteAllSessionLogs();

// IP vibration flag
void setVibrateIPFlag(bool shouldVibrate);
bool getVibrateIPFlag();

#endif // STORAGE_H 
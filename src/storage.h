#ifndef STORAGE_H
#define STORAGE_H

#include "config.h"
#include <ArduinoJson.h>

void setupStorage();
void saveConfig(const AppConfig& config);
AppConfig loadConfig();
void saveSessionLog(const JsonObject& log);
String getSessionLogs();
String getSessionLogsJson();

#endif // STORAGE_H 
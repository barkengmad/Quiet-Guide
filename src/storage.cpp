#include "storage.h"
#include <EEPROM.h>
#include <SPIFFS.h>

const int CONFIG_ADDRESS = 0;
const int CONFIG_MAGIC_NUMBER = 0x1A2B3C4D;

struct StoredConfig {
    int magicNumber;
    AppConfig appConfig;
};

void setupStorage() {
    EEPROM.begin(sizeof(StoredConfig));
    if (!SPIFFS.begin(true)) {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }
}

void saveConfig(const AppConfig& config) {
    StoredConfig storedConfig;
    storedConfig.magicNumber = CONFIG_MAGIC_NUMBER;
    storedConfig.appConfig = config;
    EEPROM.put(CONFIG_ADDRESS, storedConfig);
    EEPROM.commit();
}

AppConfig loadConfig() {
    StoredConfig storedConfig;
    EEPROM.get(CONFIG_ADDRESS, storedConfig);

    if (storedConfig.magicNumber == CONFIG_MAGIC_NUMBER) {
        return storedConfig.appConfig;
    } else {
        AppConfig defaultConfig = {
            DEFAULT_MAX_ROUNDS,
            DEFAULT_DEEP_BREATHING_S,
            DEFAULT_RECOVERY_S,
            DEFAULT_IDLE_TIMEOUT_MIN,
            DEFAULT_SILENT_PHASE_MAX_MIN,
            DEFAULT_SILENT_REMINDER_ENABLED,
            DEFAULT_SILENT_REMINDER_INTERVAL_MIN,
            1 // currentRound
        };
        saveConfig(defaultConfig);
        return defaultConfig;
    }
}

void saveSessionLog(const JsonObject& log) {
    File file = SPIFFS.open("/session_logs.json", FILE_APPEND);
    if (!file) {
        Serial.println("Failed to open file for appending");
        return;
    }
    serializeJson(log, file);
    file.println(); // Add a newline to separate logs
    file.close();
}

String getSessionLogs() {
    File file = SPIFFS.open("/session_logs.json", FILE_READ);
    if (!file) {
        return "No logs found.";
    }
    String logs = "";
    while(file.available()){
        logs += file.readStringUntil('\n');
        logs += "<br>";
    }
    file.close();
    return logs;
}

String getSessionLogsJson() {
    File file = SPIFFS.open("/session_logs.json", FILE_READ);
    if (!file) {
        return "[]";
    }
    String logs = "[";
    bool first = true;
    while(file.available()){
        if (!first) {
            logs += ",";
        }
        logs += file.readStringUntil('\n');
        first = false;
    }
    logs += "]";
    file.close();
    return logs;
} 
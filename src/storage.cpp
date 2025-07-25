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

void deleteSessionLog(int index) {
    File file = SPIFFS.open("/session_logs.json", FILE_READ);
    if (!file) {
        Serial.println("No session logs file found");
        return;
    }
    
    // Read all lines into memory
    String lines[100]; // Assuming max 100 sessions - could be made dynamic
    int lineCount = 0;
    while (file.available() && lineCount < 100) {
        lines[lineCount] = file.readStringUntil('\n');
        lineCount++;
    }
    file.close();
    
    // Check if index is valid
    if (index < 0 || index >= lineCount) {
        Serial.println("Invalid session index for deletion");
        return;
    }
    
    // Rewrite file without the session at the specified index
    file = SPIFFS.open("/session_logs.json", FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    }
    
    for (int i = 0; i < lineCount; i++) {
        if (i != index) {
            file.println(lines[i]);
        }
    }
    file.close();
    
    Serial.print("Deleted session at index: ");
    Serial.println(index);
}

void deleteAllSessionLogs() {
    if (SPIFFS.remove("/session_logs.json")) {
        Serial.println("All session logs deleted");
    } else {
        Serial.println("Failed to delete session logs or file doesn't exist");
    }
} 
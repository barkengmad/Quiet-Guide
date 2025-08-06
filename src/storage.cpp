#include "storage.h"
#include <EEPROM.h>
#include <SPIFFS.h>

struct StoredConfig {
    int magicNumber;
    AppConfig appConfig;
};

struct StoredWiFi {
    int magicNumber;
    WiFiCredentials wifiCreds;
};

struct StoredVibrateFlag {
    int magicNumber;
    bool shouldVibrate;
};

const int CONFIG_ADDRESS = 0;
const int CONFIG_MAGIC_NUMBER = 0x1A2B3C4D;
const int WIFI_ADDRESS = sizeof(StoredConfig);
const int WIFI_MAGIC_NUMBER = 0x5E6F7A8B;
const int VIBRATE_FLAG_ADDRESS = sizeof(StoredConfig) + sizeof(StoredWiFi);
const int VIBRATE_FLAG_MAGIC = 0x9F8E7D6C;

void setupStorage() {
    EEPROM.begin(sizeof(StoredConfig) + sizeof(StoredWiFi) + sizeof(StoredVibrateFlag));
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

void saveWiFiCredentials(const WiFiCredentials& creds) {
    StoredWiFi storedWiFi;
    storedWiFi.magicNumber = WIFI_MAGIC_NUMBER;
    storedWiFi.wifiCreds = creds;
    
    Serial.println("=== SAVING WIFI CREDENTIALS ===");
    Serial.print("WIFI_ADDRESS: ");
    Serial.println(WIFI_ADDRESS);
    Serial.print("StoredWiFi size: ");
    Serial.println(sizeof(StoredWiFi));
    Serial.print("Magic number: 0x");
    Serial.println(WIFI_MAGIC_NUMBER, HEX);
    Serial.print("SSID: '");
    Serial.print(creds.ssid);
    Serial.print("', Password: '");
    Serial.print(creds.password);
    Serial.print("', Configured: ");
    Serial.println(creds.isConfigured ? "true" : "false");
    
    EEPROM.put(WIFI_ADDRESS, storedWiFi);
    EEPROM.commit();
    Serial.println("WiFi credentials saved to EEPROM");
    
    // Verify the save by reading it back
    StoredWiFi verifyWiFi;
    EEPROM.get(WIFI_ADDRESS, verifyWiFi);
    Serial.println("=== VERIFICATION READ ===");
    Serial.print("Magic number read: 0x");
    Serial.println(verifyWiFi.magicNumber, HEX);
    Serial.print("SSID read: '");
    Serial.print(verifyWiFi.wifiCreds.ssid);
    Serial.print("', Password read: '");
    Serial.print(verifyWiFi.wifiCreds.password);
    Serial.print("', Configured read: ");
    Serial.println(verifyWiFi.wifiCreds.isConfigured ? "true" : "false");
    Serial.println("=== END SAVE ===");
}

WiFiCredentials loadWiFiCredentials() {
    StoredWiFi storedWiFi;
    
    Serial.println("=== LOADING WIFI CREDENTIALS ===");
    Serial.print("WIFI_ADDRESS: ");
    Serial.println(WIFI_ADDRESS);
    Serial.print("StoredWiFi size: ");
    Serial.println(sizeof(StoredWiFi));
    
    EEPROM.get(WIFI_ADDRESS, storedWiFi);
    
    Serial.print("Magic number expected: 0x");
    Serial.println(WIFI_MAGIC_NUMBER, HEX);
    Serial.print("Magic number found: 0x");
    Serial.println(storedWiFi.magicNumber, HEX);
    
    if (storedWiFi.magicNumber == WIFI_MAGIC_NUMBER) {
        Serial.println("WiFi credentials loaded from EEPROM");
        Serial.print("SSID: '");
        Serial.print(storedWiFi.wifiCreds.ssid);
        Serial.print("', Password: '");
        Serial.print(storedWiFi.wifiCreds.password);
        Serial.print("', Configured: ");
        Serial.println(storedWiFi.wifiCreds.isConfigured ? "true" : "false");
        Serial.println("=== END LOAD ===");
        return storedWiFi.wifiCreds;
    } else {
        Serial.println("No valid WiFi credentials found, returning empty");
        Serial.println("=== END LOAD ===");
        WiFiCredentials emptyCreds = {"", "", false};
        return emptyCreds;
    }
}

void clearWiFiCredentials() {
    WiFiCredentials emptyCreds = {"", "", false};
    saveWiFiCredentials(emptyCreds);
    Serial.println("WiFi credentials cleared");
}

void setVibrateIPFlag(bool shouldVibrate) {
    StoredVibrateFlag flag;
    flag.magicNumber = VIBRATE_FLAG_MAGIC;
    flag.shouldVibrate = shouldVibrate;
    
    EEPROM.put(VIBRATE_FLAG_ADDRESS, flag);
    EEPROM.commit();
    
    Serial.print("Vibrate IP flag set to: ");
    Serial.println(shouldVibrate ? "true" : "false");
}

bool getVibrateIPFlag() {
    StoredVibrateFlag flag;
    EEPROM.get(VIBRATE_FLAG_ADDRESS, flag);
    
    if (flag.magicNumber == VIBRATE_FLAG_MAGIC) {
        Serial.print("Vibrate IP flag loaded: ");
        Serial.println(flag.shouldVibrate ? "true" : "false");
        
        // Clear the flag after reading (one-time use)
        if (flag.shouldVibrate) {
            setVibrateIPFlag(false);
        }
        
        return flag.shouldVibrate;
    } else {
        Serial.println("No vibrate IP flag found");
        return false;
    }
} 
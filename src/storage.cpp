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

struct StoredWiFiNetworks {
    int magicNumber;
    WiFiNetworkList networkList;
};

const int CONFIG_ADDRESS = 0;
const int CONFIG_MAGIC_NUMBER = 0x1A2B3C4D;
const int WIFI_ADDRESS = sizeof(StoredConfig);
const int WIFI_MAGIC_NUMBER = 0x5E6F7A8B;
const int VIBRATE_FLAG_ADDRESS = sizeof(StoredConfig) + sizeof(StoredWiFi);
const int VIBRATE_FLAG_MAGIC = 0x9F8E7D6C;
const int WIFI_NETWORKS_ADDRESS = sizeof(StoredConfig) + sizeof(StoredWiFi) + sizeof(StoredVibrateFlag);
const int WIFI_NETWORKS_MAGIC = 0xA1B2C3D4;

void setupStorage() {
    EEPROM.begin(sizeof(StoredConfig) + sizeof(StoredWiFi) + sizeof(StoredVibrateFlag) + sizeof(StoredWiFiNetworks));
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
        AppConfig cfg = storedConfig.appConfig;
        // Backward compatibility defaults if new fields are unset/garbage
        if (cfg.currentPatternId <= 0 || cfg.currentPatternId > 10) cfg.currentPatternId = DEFAULT_PATTERN_ID;
        if (cfg.boxSeconds < 2 || cfg.boxSeconds > 60) cfg.boxSeconds = DEFAULT_BOX_SECONDS;
        // For bools, assume 0/1; if eeprom random, normalize to defaults
        if (cfg.startConfirmationHaptics != 0 && cfg.startConfirmationHaptics != 1) cfg.startConfirmationHaptics = DEFAULT_START_CONFIRMATION_HAPTICS;
        if (cfg.abortSaveThresholdSeconds < 5 || cfg.abortSaveThresholdSeconds > 3600) cfg.abortSaveThresholdSeconds = DEFAULT_ABORT_SAVE_THRESHOLD_S;
        // Custom fields clamp 0..16
        if (cfg.customInhaleSeconds < 0 || cfg.customInhaleSeconds > 16) cfg.customInhaleSeconds = DEFAULT_CUSTOM_INHALE;
        if (cfg.customHoldInSeconds < 0 || cfg.customHoldInSeconds > 16) cfg.customHoldInSeconds = DEFAULT_CUSTOM_HOLD_IN;
        if (cfg.customExhaleSeconds < 0 || cfg.customExhaleSeconds > 16) cfg.customExhaleSeconds = DEFAULT_CUSTOM_EXHALE;
        if (cfg.customHoldOutSeconds < 0 || cfg.customHoldOutSeconds > 16) cfg.customHoldOutSeconds = DEFAULT_CUSTOM_HOLD_OUT;
        // Inclusion defaults normalize
        if (cfg.includeWimHof != 0 && cfg.includeWimHof != 1) cfg.includeWimHof = DEFAULT_INCLUDE_WIMHOF;
        if (cfg.includeBox != 0 && cfg.includeBox != 1) cfg.includeBox = DEFAULT_INCLUDE_BOX;
        if (cfg.include478 != 0 && cfg.include478 != 1) cfg.include478 = DEFAULT_INCLUDE_478;
        if (cfg.includeResonant != 0 && cfg.includeResonant != 1) cfg.includeResonant = DEFAULT_INCLUDE_RESONANT;
        if (cfg.includeCustom != 0 && cfg.includeCustom != 1) cfg.includeCustom = DEFAULT_INCLUDE_CUSTOM;
        if (cfg.includeDynamic != 0 && cfg.includeDynamic != 1) cfg.includeDynamic = DEFAULT_INCLUDE_DYNAMIC;
        // Ensure a valid default order if uninitialized
        bool badOrder = false;
        for (int i = 0; i < 6; ++i) {
            int v = cfg.patternOrder[i];
            if (v < 1 || v > 6) { badOrder = true; break; }
        }
        if (badOrder) {
            cfg.patternOrder[0]=1; cfg.patternOrder[1]=2; cfg.patternOrder[2]=3; cfg.patternOrder[3]=4; cfg.patternOrder[4]=5; cfg.patternOrder[5]=6;
        }
        // Ensure order is a permutation (dedupe/fill) in case of legacy data
        bool seen[7]; for (int i=0;i<7;i++) seen[i]=false;
        int out[6]; int outIdx=0;
        for (int i=0;i<6;i++){ int id=cfg.patternOrder[i]; if (id>=1 && id<=6 && !seen[id]) { out[outIdx++]=id; seen[id]=true; } }
        for (int id=1; id<=6 && outIdx<6; ++id) if (!seen[id]) out[outIdx++]=id;
        for (int i=0;i<6;i++) cfg.patternOrder[i]=out[i];
        // New per-pattern silent flags: normalize to defaults if invalid
        if (cfg.silentAfterWimHof != 0 && cfg.silentAfterWimHof != 1) cfg.silentAfterWimHof = DEFAULT_SILENT_AFTER_WIMHOF;
        if (cfg.silentAfterBox != 0 && cfg.silentAfterBox != 1) cfg.silentAfterBox = DEFAULT_SILENT_AFTER_BOX;
        if (cfg.silentAfter478 != 0 && cfg.silentAfter478 != 1) cfg.silentAfter478 = DEFAULT_SILENT_AFTER_478;
        if (cfg.silentAfterResonant != 0 && cfg.silentAfterResonant != 1) cfg.silentAfterResonant = DEFAULT_SILENT_AFTER_RESONANT;
        if (cfg.silentAfterCustom != 0 && cfg.silentAfterCustom != 1) cfg.silentAfterCustom = DEFAULT_SILENT_AFTER_CUSTOM;
        if (cfg.silentAfterDynamic != 0 && cfg.silentAfterDynamic != 1) cfg.silentAfterDynamic = DEFAULT_SILENT_AFTER_DYNAMIC;
        if (cfg.guidedBreathingMinutes < 1 || cfg.guidedBreathingMinutes > 120) cfg.guidedBreathingMinutes = DEFAULT_GUIDED_BREATHING_MINUTES;
        return cfg;
    } else {
        AppConfig defaultConfig = {
            DEFAULT_MAX_ROUNDS,
            DEFAULT_DEEP_BREATHING_S,
            DEFAULT_RECOVERY_S,
            DEFAULT_IDLE_TIMEOUT_MIN,
            DEFAULT_SILENT_PHASE_MAX_MIN,
            DEFAULT_SILENT_REMINDER_ENABLED,
            DEFAULT_SILENT_REMINDER_INTERVAL_MIN,
            1, // currentRound
            DEFAULT_PATTERN_ID,
            DEFAULT_BOX_SECONDS,
            DEFAULT_START_CONFIRMATION_HAPTICS,
            DEFAULT_ABORT_SAVE_THRESHOLD_S,
            DEFAULT_CUSTOM_INHALE,
            DEFAULT_CUSTOM_HOLD_IN,
            DEFAULT_CUSTOM_EXHALE,
            DEFAULT_CUSTOM_HOLD_OUT,
            DEFAULT_INCLUDE_WIMHOF,
            DEFAULT_INCLUDE_BOX,
            DEFAULT_INCLUDE_478,
            DEFAULT_INCLUDE_RESONANT,
            DEFAULT_INCLUDE_CUSTOM,
            DEFAULT_INCLUDE_DYNAMIC,
            {1,2,3,4,5,6},
            DEFAULT_SILENT_AFTER_WIMHOF,
            DEFAULT_SILENT_AFTER_BOX,
            DEFAULT_SILENT_AFTER_478,
            DEFAULT_SILENT_AFTER_RESONANT,
            DEFAULT_SILENT_AFTER_CUSTOM,
            DEFAULT_SILENT_AFTER_DYNAMIC,
            DEFAULT_GUIDED_BREATHING_MINUTES
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

// Multi-network WiFi storage functions
void saveWiFiNetworks(const WiFiNetworkList& networks) {
    StoredWiFiNetworks stored;
    stored.magicNumber = WIFI_NETWORKS_MAGIC;
    stored.networkList = networks;
    
    Serial.println("=== SAVING WIFI NETWORKS ===");
    Serial.print("Network count: ");
    Serial.println(networks.count);
    for (int i = 0; i < networks.count; i++) {
        Serial.print("Network ");
        Serial.print(i);
        Serial.print(" - SSID: '");
        Serial.print(networks.networks[i].ssid);
        Serial.print("', Priority: ");
        Serial.println(networks.networks[i].priority);
    }
    
    EEPROM.put(WIFI_NETWORKS_ADDRESS, stored);
    EEPROM.commit();
    Serial.println("WiFi networks saved to EEPROM");
    Serial.println("=== END SAVE ===");
}

WiFiNetworkList loadWiFiNetworks() {
    StoredWiFiNetworks stored;
    
    Serial.println("=== LOADING WIFI NETWORKS ===");
    EEPROM.get(WIFI_NETWORKS_ADDRESS, stored);
    
    if (stored.magicNumber == WIFI_NETWORKS_MAGIC) {
        Serial.println("WiFi networks loaded from EEPROM");
        Serial.print("Network count: ");
        Serial.println(stored.networkList.count);
        for (int i = 0; i < stored.networkList.count; i++) {
            Serial.print("Network ");
            Serial.print(i);
            Serial.print(" - SSID: '");
            Serial.print(stored.networkList.networks[i].ssid);
            Serial.print("', Priority: ");
            Serial.println(stored.networkList.networks[i].priority);
        }
        Serial.println("=== END LOAD ===");
        return stored.networkList;
    } else {
        Serial.println("No valid WiFi networks found, returning empty list");
        Serial.println("=== END LOAD ===");
        WiFiNetworkList emptyList;
        emptyList.count = 0;
        return emptyList;
    }
}

void clearWiFiNetworks() {
    WiFiNetworkList emptyList;
    emptyList.count = 0;
    saveWiFiNetworks(emptyList);
    Serial.println("WiFi networks cleared");
} 
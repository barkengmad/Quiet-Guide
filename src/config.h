#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Pin Assignments
const int BUTTON_PIN = 25;
const int VIBRATION_PIN = 26;
const int LED_PIN = 2;

// PWM settings for vibration motor
const int VIBRATION_PWM_CHANNEL = 0;
const int VIBRATION_PWM_FREQ = 5000;
const int VIBRATION_PWM_RESOLUTION = 8;

// WiFi credentials
extern const char* ssid;
extern const char* password;

// Default configuration values
const int DEFAULT_MAX_ROUNDS = 5;
const int DEFAULT_DEEP_BREATHING_S = 30;
const int DEFAULT_RECOVERY_S = 10;
const int DEFAULT_IDLE_TIMEOUT_MIN = 3;
const int DEFAULT_SILENT_PHASE_MAX_MIN = 30;
const bool DEFAULT_SILENT_REMINDER_ENABLED = false;
const int DEFAULT_SILENT_REMINDER_INTERVAL_MIN = 10;
const bool DEFAULT_START_CONFIRMATION_HAPTICS = true;
const int DEFAULT_ABORT_SAVE_THRESHOLD_S = 60;
const int DEFAULT_BOX_SECONDS = 4; // Range 2..8
const int DEFAULT_PATTERN_ID = 1; // 1=Wim Hof, 2=Box, 3=4-7-8, 4=Resonant, 5=Custom, 6=Dynamic
// Custom timed prompts defaults (0 means skip)
const int DEFAULT_CUSTOM_INHALE = 0;
const int DEFAULT_CUSTOM_HOLD_IN = 0;
const int DEFAULT_CUSTOM_EXHALE = 0;
const int DEFAULT_CUSTOM_HOLD_OUT = 0;
// Pattern inclusion defaults (offering rotation)
const bool DEFAULT_INCLUDE_WIMHOF = true;
const bool DEFAULT_INCLUDE_BOX = true;
const bool DEFAULT_INCLUDE_478 = true;
const bool DEFAULT_INCLUDE_RESONANT = true;
const bool DEFAULT_INCLUDE_CUSTOM = true;
const bool DEFAULT_INCLUDE_DYNAMIC = true;

struct AppConfig {
    int maxRounds;
    int deepBreathingSeconds;
    int recoverySeconds;
    int idleTimeoutMinutes;
    int silentPhaseMaxMinutes;
    bool silentReminderEnabled;
    int silentReminderIntervalMinutes;
    int currentRound;
    // New fields (appended for backward-compat):
    int currentPatternId;              // 1=Wim Hof, 2=Box, ...
    int boxSeconds;                    // 2..8
    bool startConfirmationHaptics;     // announce type+value on start
    int abortSaveThresholdSeconds;     // discard under this duration
    // Custom timed prompts (seconds; 0 means skip)
    int customInhaleSeconds;
    int customHoldInSeconds;
    int customExhaleSeconds;
    int customHoldOutSeconds;
    // Pattern inclusion toggles (controls long-press rotation/offering)
    bool includeWimHof;
    bool includeBox;
    bool include478;
    bool includeResonant;
    bool includeCustom;
    bool includeDynamic;
    // Order of patterns for rotation/haptic count (values are IDs 1..6)
    int patternOrder[6];
};

#endif // CONFIG_H 
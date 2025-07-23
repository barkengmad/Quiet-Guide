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

struct AppConfig {
    int maxRounds;
    int deepBreathingSeconds;
    int recoverySeconds;
    int idleTimeoutMinutes;
    int silentPhaseMaxMinutes;
    bool silentReminderEnabled;
    int silentReminderIntervalMinutes;
    int currentRound;
};

#endif // CONFIG_H 
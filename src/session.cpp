#include "session.h"
#include "config.h"
#include "vibration.h"
#include "storage.h"
#include "rtc_time.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_sleep.h>

SessionState currentState = SessionState::BOOTING;
AppConfig config;

// Timers
unsigned long state_enter_time = 0;
unsigned long last_interaction_time = 0;
unsigned long next_pulse_time = 0;

// Session data
int current_session_round = 0;
int pulse_count_remaining = 0;
JsonDocument session_log_doc;
JsonArray rounds_log;

// Button handling - moved into session module
const long LONG_PRESS_DURATION = 2000;
const long DEBOUNCE_DELAY = 50;
const long ROUND_SELECT_DELAY = 1000; // 1 second delay before pulsing
int last_stable_state = HIGH;
int last_flicker_state = HIGH;
unsigned long last_debounce_time = 0;
unsigned long button_down_time = 0;
bool short_press_detected = false;
bool long_press_detected = false;

// Round selection delay handling
unsigned long last_round_press_time = 0;
bool round_selection_pending = false;

void handleButtonInSession() {
    int current_reading = digitalRead(BUTTON_PIN);

    // If the raw reading has changed, reset the debounce timer
    if (current_reading != last_flicker_state) {
        last_debounce_time = millis();
    }

    // If enough time has passed with no change in the raw reading...
    if ((millis() - last_debounce_time) > DEBOUNCE_DELAY) {
        // ...then the current reading is stable.
        // If this stable reading is different from our last known stable state...
        if (current_reading != last_stable_state) {
            last_stable_state = current_reading;

            // Perform actions based on the edge of this stable state change
            if (last_stable_state == LOW) {
                // A press has just been confirmed
                button_down_time = millis();
                Serial.println("DEBUG: Button press confirmed");
            } else {
                // A release has just been confirmed
                unsigned long press_duration = millis() - button_down_time;
                Serial.print("DEBUG: Button released, duration: ");
                Serial.println(press_duration);
                
                if (press_duration >= LONG_PRESS_DURATION) {
                    long_press_detected = true;
                    Serial.println("Long press detected.");
                } else if (press_duration > DEBOUNCE_DELAY) {
                    short_press_detected = true;
                    Serial.println("Short press detected.");
                }
            }
        }
    }

    // Always update the last raw reading
    last_flicker_state = current_reading;
}

void startPulsing(int count) {
    pulse_count_remaining = count;
    next_pulse_time = millis(); // Start immediately
}

void loopPulsing() {
    if (pulse_count_remaining > 0 && millis() >= next_pulse_time) {
        vibrate(250); // Increased duration
        pulse_count_remaining--;
        if (pulse_count_remaining > 0) {
            next_pulse_time = millis() + 500; // Schedule next pulse
        }
    }
}

void enterState(SessionState newState) {
    currentState = newState;
    state_enter_time = millis();
    last_interaction_time = millis();
    pulse_count_remaining = 0; // Stop any pulsing from previous state
    
    // Clear any pending round selection when changing states
    if (round_selection_pending) {
        saveConfig(config);
        round_selection_pending = false;
    }

    switch (newState) {
        case SessionState::BOOTING:
            Serial.println("State: BOOTING");
            break;
        case SessionState::IDLE:
            Serial.println("State: IDLE");
            Serial.print("DEBUG: Currently selected rounds: ");
            Serial.println(config.currentRound);
            startPulsing(config.currentRound);
            break;
        case SessionState::DEEP_BREATHING:
            Serial.println("State: DEEP_BREATHING");
            current_session_round++;
            startPulsing(current_session_round);
            if (rounds_log.isNull()) {
                rounds_log = session_log_doc["rounds"].to<JsonArray>();
            }
            rounds_log.add<JsonObject>();
            break;
        case SessionState::BREATH_HOLD:
            Serial.println("State: BREATH_HOLD");
            vibrate(250);
            break;
        case SessionState::RECOVERY:
            Serial.println("State: RECOVERY");
            vibrate(250);
            break;
        case SessionState::SILENT:
            Serial.println("State: SILENT");
            vibrate(750); // Longer pulse to indicate start of silent phase
            break;
    }
}

void saveCurrentSession() {
    if (current_session_round > 0) {
        Serial.println("DEBUG: Saving session data...");
        time_t now = getEpochTime();
        struct tm* timeinfo = localtime(&now);
        char date_buf[11];
        char time_buf[9];
        strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", timeinfo);
        strftime(time_buf, sizeof(time_buf), "%H:%M:%S", timeinfo);
        
        session_log_doc["date"] = date_buf;
        session_log_doc["start_time"] = time_buf;

        // Calculate silent phase duration if we're ending from silent state
        unsigned long silent_duration = 0;
        if (currentState == SessionState::SILENT) {
            silent_duration = (millis() - state_enter_time) / 1000;
        }

        // Calculate total duration by summing all round times plus silent phase
        unsigned long total_duration = silent_duration;
        for (size_t i = 0; i < rounds_log.size(); i++) {
            JsonObject round = rounds_log[i];
            total_duration += round["deep"].as<unsigned long>();
            total_duration += round["hold"].as<unsigned long>();
            total_duration += round["recover"].as<unsigned long>();
        }

        session_log_doc["silent"] = silent_duration;
        session_log_doc["total"] = total_duration;

        Serial.print("DEBUG: Session log JSON: ");
        serializeJson(session_log_doc, Serial);
        Serial.println();

        saveSessionLog(session_log_doc.as<JsonObject>());
        Serial.println("DEBUG: Session saved to storage");
    } else {
        Serial.println("DEBUG: No session data to save (current_session_round = 0)");
    }
    // Clear log for next session
    session_log_doc.clear();
    rounds_log = session_log_doc["rounds"].to<JsonArray>();
}

void setupSession() {
    config = loadConfig();
    rounds_log = session_log_doc["rounds"].to<JsonArray>();
    pinMode(BUTTON_PIN, INPUT_PULLUP); // Setup button here
    
    // Check if we woke up from deep sleep
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    switch(wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0:
            Serial.println("Woke up from deep sleep by button press!");
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            Serial.println("Woke up from deep sleep by timer");
            break;
        default:
            Serial.println("Normal startup (not from deep sleep)");
            break;
    }
    
    enterState(SessionState::BOOTING);
}

void loopSession() {
    handleButtonInSession(); // Handle button directly in session
    loopPulsing(); // Always check if a pulse is due

    switch (currentState) {
        case SessionState::BOOTING:
            // The boot sequence is now handled entirely by the non-blocking main loop
            break;

        case SessionState::IDLE:
            if (short_press_detected) {
                last_interaction_time = millis();
                last_round_press_time = millis();
                config.currentRound++;
                if (config.currentRound > config.maxRounds) {
                    config.currentRound = 1;
                }
                Serial.print("DEBUG: Selected rounds: ");
                Serial.println(config.currentRound);
                round_selection_pending = true;
                short_press_detected = false; // Clear the flag AFTER acting on it
            }
            
            // Handle delayed pulsing after round selection
            if (round_selection_pending && (millis() - last_round_press_time > ROUND_SELECT_DELAY)) {
                saveConfig(config);
                startPulsing(config.currentRound);
                round_selection_pending = false;
                Serial.print("DEBUG: Starting delayed pulse for rounds: ");
                Serial.println(config.currentRound);
            }
            if (long_press_detected) {
                // If round selection is pending, finalize it before starting session
                if (round_selection_pending) {
                    saveConfig(config);
                    round_selection_pending = false;
                }
                Serial.println("DEBUG: Long press flag detected in session logic");
                Serial.print("DEBUG: Starting session with ");
                Serial.print(config.currentRound);
                Serial.println(" rounds");
                vibrate(100); // Immediate feedback for session start
                current_session_round = 0;
                session_log_doc.clear();
                rounds_log = session_log_doc["rounds"].to<JsonArray>();
                Serial.println("DEBUG: About to enter DEEP_BREATHING state");
                enterState(SessionState::DEEP_BREATHING);
                Serial.println("DEBUG: Returned from enterState call");
                long_press_detected = false; // Clear the flag AFTER acting on it
            }
            if (millis() - last_interaction_time > (unsigned long)config.idleTimeoutMinutes * 60 * 1000) {
                Serial.println("Entering deep sleep due to inactivity.");
                Serial.println("Press button to wake up...");
                
                // Configure button pin as wake-up source (wake on LOW - button pressed)
                esp_sleep_enable_ext0_wakeup(GPIO_NUM_25, 0);
                
                // Give a moment for serial to flush
                delay(100);
                
                esp_deep_sleep_start();
            }
            break;

        case SessionState::DEEP_BREATHING:
            if (short_press_detected) {
                unsigned long duration = (millis() - state_enter_time) / 1000;
                rounds_log[current_session_round-1]["deep"] = duration;
                enterState(SessionState::BREATH_HOLD);
                short_press_detected = false;
            }
            // Timeout transition
            if (millis() - state_enter_time > (unsigned long)config.deepBreathingSeconds * 1000) {
                 unsigned long duration = (millis() - state_enter_time) / 1000;
                rounds_log[current_session_round-1]["deep"] = duration;
                vibrate(750); // Long pulse for timeout
                enterState(SessionState::BREATH_HOLD);
            }
            // Global long press abort for active sessions
            if (long_press_detected) {
                long_press_detected = false;
                Serial.println("Session aborted by long press.");
                vibrate(1500); // Distinct long pulse for abort
                current_session_round = 0;
                enterState(SessionState::IDLE);
                return;
            }
            break;

        case SessionState::BREATH_HOLD:
            if (short_press_detected) {
                unsigned long duration = (millis() - state_enter_time) / 1000;
                rounds_log[current_session_round-1]["hold"] = duration;
                enterState(SessionState::RECOVERY);
                short_press_detected = false;
            }
            // Global long press abort for active sessions
            if (long_press_detected) {
                long_press_detected = false;
                Serial.println("Session aborted by long press.");
                vibrate(1500); // Distinct long pulse for abort
                current_session_round = 0;
                enterState(SessionState::IDLE);
                return;
            }
            break;

        case SessionState::RECOVERY:
             if (short_press_detected) {
                unsigned long duration = (millis() - state_enter_time) / 1000;
                rounds_log[current_session_round-1]["recover"] = duration;
                // Transition to next round or silent phase
                 if (current_session_round < config.currentRound) {
                    enterState(SessionState::DEEP_BREATHING);
                } else {
                    enterState(SessionState::SILENT);
                }
                short_press_detected = false;
            }
            // Timeout transition
            if (millis() - state_enter_time > (unsigned long)config.recoverySeconds * 1000) {
                unsigned long duration = (millis() - state_enter_time) / 1000;
                rounds_log[current_session_round-1]["recover"] = duration;
                vibrate(750);
                 if (current_session_round < config.currentRound) {
                    enterState(SessionState::DEEP_BREATHING);
                } else {
                    enterState(SessionState::SILENT);
                }
            }
            // Global long press abort for active sessions
            if (long_press_detected) {
                long_press_detected = false;
                Serial.println("Session aborted by long press.");
                vibrate(1500); // Distinct long pulse for abort
                current_session_round = 0;
                enterState(SessionState::IDLE);
                return;
            }
            break;
            
        case SessionState::SILENT:
            if (short_press_detected) {
                // Silent phase is complete, save session and go to idle
                saveCurrentSession();
                enterState(SessionState::IDLE);
                short_press_detected = false;
            }
            // Auto-end silent phase after max duration
            if (millis() - state_enter_time > (unsigned long)config.silentPhaseMaxMinutes * 60 * 1000) {
                Serial.println("Silent phase max duration reached. Ending session.");
                saveCurrentSession();
                enterState(SessionState::IDLE);
            }
            // Silent reminder pulse
            if (config.silentReminderEnabled) {
                static unsigned long last_reminder_time = 0;
                if (millis() - last_reminder_time > (unsigned long)config.silentReminderIntervalMinutes * 60 * 1000) {
                    vibrate(750);
                    last_reminder_time = millis();
                }
            }
            // Global long press abort for active sessions
            if (long_press_detected) {
                long_press_detected = false;
                Serial.println("Session aborted by long press.");
                vibrate(1500); // Distinct long pulse for abort
                current_session_round = 0;
                enterState(SessionState::IDLE);
                return;
            }
            break;
    }
}

SessionState getCurrentState() {
    return currentState;
}

int getCurrentSessionRound() {
    return current_session_round;
}

int getTotalRounds() {
    return config.currentRound;
}

void finishBooting() {
    if (currentState == SessionState::BOOTING) {
        enterState(SessionState::IDLE);
    }
} 
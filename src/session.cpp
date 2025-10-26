#include "session.h"
#include "config.h"
#include "vibration.h"
#include "storage.h"
#include "rtc_time.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_sleep.h>
#include "driver/rtc_io.h"

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
const long LONG_PRESS_MIN = 1500;      // 1.5s ≤ long < 3s
const long VERY_LONG_PRESS_MIN = 3000; // ≥3s
const long DEBOUNCE_DELAY = 50;
const long ROUND_SELECT_DELAY = 1000; // 1 second delay before pulsing
int last_stable_state = HIGH;
int last_flicker_state = HIGH;
unsigned long last_debounce_time = 0;
unsigned long button_down_time = 0;
bool short_press_detected = false;
// Release-classified flags
bool released_long_press = false;      // 2–4s
bool released_very_long_press = false; // ≥4s
// Zone haptic cues while holding
bool long_zone_buzzed = false;
bool very_long_zone_buzzed = false;

// Round selection delay handling
unsigned long last_round_press_time = 0;
bool round_selection_pending = false;
unsigned long session_start_time_ms = 0;
static bool boot_haptics_done = false;
static bool skip_idle_preview = false;     // suppress one-time IDLE preview after silent
static bool pending_silent_exit = false;   // wait to finish swell before exit
static bool pending_recovery_transition = false; // wait to finish fade-out before transitioning from recovery
static SessionState recovery_next_state = SessionState::IDLE; // target state after recovery fade-out
static unsigned long recovery_gap_start_time = 0; // timestamp when fade-out finished, to add gap before next haptic
static bool recovery_doing_prehold = false; // true during the 300ms hold before fade-out

// Custom mode runtime
struct PhaseDef { const char* name; int seconds; };
static PhaseDef customPhases[4];
static int customPhaseCount = 0;
static int customPhaseIndex = 0;
static unsigned long phase_start_time = 0;

// Dynamic mode runtime
static bool dynamicExpectInhale = true; // expecting inhale boundary first
static unsigned long last_teach_press_ms = 0;
static int teach_samples_inhale[3] = {0,0,0};
static int teach_samples_exhale[3] = {0,0,0};
static int teach_inhale_count = 0;
static int teach_exhale_count = 0;
static int avg_inhale_sec = 0;
static int avg_exhale_sec = 0;
static unsigned long dynamic_phase_start_ms = 0;
static bool dynamic_inhale_phase = true; // running inhale/exhale in guided

// Box mode runtime
static unsigned long box_phase_start_ms = 0;
static int box_phase_index = 0; // 0=inhale,1=hold-in,2=exhale,3=hold-out
// 4-7-8 and 6:6 runtime
static unsigned long guided_phase_start_ms = 0;
static int guided_phase_index = 0; // pattern-specific indexing
static unsigned long guided_session_start_ms = 0; // overall guided duration start (non-Wim Hof)

static int getPatternValueForPulse() {
    if (config.currentPatternId == 2) { // Box seconds
        int val = config.boxSeconds;
        if (val < 2) val = 2; if (val > 8) val = 8;
        return val;
    }
    // Default to rounds
    return config.currentRound;
}

static int getPatternTypeCount() {
    int id = config.currentPatternId;
    if (id < 1) id = 1; if (id > 6) id = 6;
    return id;
}

static void announceTypeAndValueBlocking() {
    // Stop any queued pulses
    pulse_count_remaining = 0;
    int typeCount = getPatternTypeCount();
    int valueCount = getPatternValueForPulse();
    vibrateTypeLong(typeCount);
    delay(600);
    // Only announce a value for patterns that have a selectable value (Wim Hof rounds, Box seconds)
    if (config.currentPatternId == 1 || config.currentPatternId == 2) {
        vibrateValueShort(valueCount);
    }
}

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
                long_zone_buzzed = false;
                very_long_zone_buzzed = false;
            } else {
                // A release has just been confirmed
                unsigned long press_duration = millis() - button_down_time;
                Serial.print("DEBUG: Button released, duration: ");
                Serial.println(press_duration);
                
                if (press_duration >= VERY_LONG_PRESS_MIN) {
                    released_very_long_press = true;
                    Serial.println("Very long press detected.");
                } else if (press_duration >= LONG_PRESS_MIN) {
                    released_long_press = true;
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

    // While button is held, provide zone-entry haptic cues
    if (last_stable_state == LOW) { // currently pressed
        unsigned long held = millis() - button_down_time;
        if (!long_zone_buzzed && held >= LONG_PRESS_MIN) {
            vibrate(200); // short cue
            long_zone_buzzed = true;
        }
        if (!very_long_zone_buzzed && held >= VERY_LONG_PRESS_MIN) {
            vibrate(450); // longer cue (2.25x first buzz)
            very_long_zone_buzzed = true;
        }
    }
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
            if (!boot_haptics_done) {
                announceTypeAndValueBlocking();
                boot_haptics_done = true;
            }
            Serial.print("DEBUG: Currently selected value: ");
            Serial.println(getPatternValueForPulse());
            // Only pulse selection preview for Wim Hof (rounds) and Box (seconds)
            if (skip_idle_preview) {
                // Suppress preview once after silent end
                pulse_count_remaining = 0;
                skip_idle_preview = false;
            } else {
                if (config.currentPatternId == 1 || config.currentPatternId == 2) {
                    startPulsing(getPatternValueForPulse());
                } else {
                    pulse_count_remaining = 0;
                }
            }
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
            // Wim Hof transition: deep → hold-out boundary cue
            vibrateFadeOut(3000);
            break;
        case SessionState::RECOVERY:
            Serial.println("State: RECOVERY");
            // Wim Hof transition: hold → recovery (inhale) boundary cue
            vibrateFadeIn(3000, 300);
            break;
        case SessionState::SILENT:
            Serial.println("State: SILENT");
            // Start-of-silent swell: 25% -> 100% -> 25% over 5 seconds
            vibrateSwell(2500, 2500);
            pending_silent_exit = false;
            break;
        case SessionState::CUSTOM_RUNNING:
            Serial.println("State: CUSTOM_RUNNING");
            vibratePhaseCue(PhaseCue::Inhale); // boundary cue at session start
            phase_start_time = millis();
            guided_session_start_ms = millis();
            break;
        case SessionState::BOX_RUNNING:
            Serial.println("State: BOX_RUNNING");
            box_phase_index = 0;
            box_phase_start_ms = millis();
            vibratePhaseCue(PhaseCue::Inhale);
            guided_session_start_ms = millis();
            break;
        case SessionState::FOURSEVENEIGHT_RUNNING:
            Serial.println("State: 4-7-8_RUNNING");
            guided_phase_index = 0; // 0=inhale(4),1=hold(7),2=exhale(8)
            guided_phase_start_ms = millis();
            vibratePhaseCue(PhaseCue::Inhale);
            guided_session_start_ms = millis();
            break;
        case SessionState::RESONANT_RUNNING:
            Serial.println("State: RESONANT_RUNNING");
            guided_phase_index = 0; // 0=inhale(6),1=exhale(6)
            guided_phase_start_ms = millis();
            vibratePhaseCue(PhaseCue::Inhale);
            guided_session_start_ms = millis();
            break;
        case SessionState::DYNAMIC_TEACHING:
            Serial.println("State: DYNAMIC_TEACHING");
            dynamicExpectInhale = true;
            teach_inhale_count = teach_exhale_count = 0;
            for(int i=0;i<3;i++){teach_samples_inhale[i]=0; teach_samples_exhale[i]=0;}
            last_teach_press_ms = 0;
            vibrate(100);
            break;
        case SessionState::DYNAMIC_GUIDED:
            Serial.println("State: DYNAMIC_GUIDED");
            dynamic_inhale_phase = true;
            dynamic_phase_start_ms = millis();
            vibrate(100);
            guided_session_start_ms = millis();
            break;
    }
}

void saveCurrentSession() {
    Serial.println("DEBUG: Saving session data...");
    time_t now = getEpochTime();
    struct tm* timeinfo = localtime(&now);
    char date_buf[11];
    char time_buf[9];
    strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", timeinfo);
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", timeinfo);

    session_log_doc["date"] = date_buf;
    session_log_doc["start_time"] = time_buf;

    // Pattern metadata
    session_log_doc["pattern_id"] = config.currentPatternId;
    const char* pattern_name = "Unknown";
    switch (config.currentPatternId) {
        case 1: pattern_name = "Wim Hof"; break;
        case 2: pattern_name = "Box"; break;
        case 3: pattern_name = "4-7-8"; break;
        case 4: pattern_name = "Resonant"; break;
        case 5: pattern_name = "Custom"; break;
        case 6: pattern_name = "Dynamic"; break;
    }
    session_log_doc["pattern_name"] = pattern_name;

    // Calculate silent phase duration if we're ending from silent state
    unsigned long silent_duration = 0;
    if (currentState == SessionState::SILENT) {
        silent_duration = (millis() - state_enter_time) / 1000;
    }

    // Total duration: prefer explicit rounds sum for Wim Hof, otherwise elapsed since start
    unsigned long total_duration = 0;
    if (rounds_log.size() > 0) {
        for (size_t i = 0; i < rounds_log.size(); i++) {
            JsonObject round = rounds_log[i];
            total_duration += round["deep"].as<unsigned long>();
            total_duration += round["hold"].as<unsigned long>();
            total_duration += round["recover"].as<unsigned long>();
        }
        total_duration += silent_duration;
    } else {
        total_duration = (millis() - session_start_time_ms) / 1000UL;
    }

    // Apply keep-partial threshold
    if (total_duration < (unsigned long)config.abortSaveThresholdSeconds) {
        Serial.println("DEBUG: Discarding session under threshold.");
        session_log_doc.clear();
        rounds_log = session_log_doc["rounds"].to<JsonArray>();
        return;
    }

    session_log_doc["silent"] = silent_duration;
    session_log_doc["total"] = total_duration;

    // Pattern-specific settings snapshot
    JsonObject settings = session_log_doc["settings"].to<JsonObject>();
    if (config.currentPatternId == 1) {
        settings["rounds_selected"] = config.currentRound;
        settings["deepBreathingSeconds"] = config.deepBreathingSeconds;
        settings["recoverySeconds"] = config.recoverySeconds;
        settings["silentAfter"] = config.silentAfterWimHof;
    } else if (config.currentPatternId == 2) {
        settings["boxSeconds"] = config.boxSeconds;
        settings["guidedMinutes"] = config.guidedBreathingMinutes;
        settings["silentAfter"] = config.silentAfterBox;
    } else if (config.currentPatternId == 3) {
        settings["guidedMinutes"] = config.guidedBreathingMinutes;
        settings["silentAfter"] = config.silentAfter478;
    } else if (config.currentPatternId == 4) {
        settings["guidedMinutes"] = config.guidedBreathingMinutes;
        settings["silentAfter"] = config.silentAfterResonant;
    } else if (config.currentPatternId == 5) {
        settings["customInhaleSeconds"] = config.customInhaleSeconds;
        settings["customHoldInSeconds"] = config.customHoldInSeconds;
        settings["customExhaleSeconds"] = config.customExhaleSeconds;
        settings["customHoldOutSeconds"] = config.customHoldOutSeconds;
        settings["guidedMinutes"] = config.guidedBreathingMinutes;
        settings["silentAfter"] = config.silentAfterCustom;
    } else if (config.currentPatternId == 6) {
        settings["avgInhaleSec"] = avg_inhale_sec;
        settings["avgExhaleSec"] = avg_exhale_sec;
        settings["guidedMinutes"] = config.guidedBreathingMinutes;
        settings["silentAfter"] = config.silentAfterDynamic;
    }
    settings["silentPhaseMaxMinutes"] = config.silentPhaseMaxMinutes;
    settings["silentReminderEnabled"] = config.silentReminderEnabled;
    settings["silentReminderIntervalMinutes"] = config.silentReminderIntervalMinutes;

    Serial.print("DEBUG: Session log JSON: ");
    serializeJson(session_log_doc, Serial);
    Serial.println();

    saveSessionLog(session_log_doc.as<JsonObject>());
    Serial.println("DEBUG: Session saved to storage");

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
                if (config.currentPatternId == 2) { // Box seconds 2..8
                    int v = config.boxSeconds + 1;
                    if (v > 8) v = 2;
                    config.boxSeconds = v;
                    Serial.print("DEBUG: Selected box seconds: ");
                    Serial.println(config.boxSeconds);
                } else if (config.currentPatternId == 1) { // Wim Hof rounds 1..maxRounds
                    config.currentRound++;
                    if (config.currentRound > config.maxRounds) {
                        config.currentRound = 1;
                    }
                    Serial.print("DEBUG: Selected rounds: ");
                    Serial.println(config.currentRound);
                } else {
                    // For other modes, short press in IDLE does nothing to value
                }
                round_selection_pending = true;
                short_press_detected = false; // Clear the flag AFTER acting on it
            }
            
            // Handle delayed pulsing after round selection
            if (round_selection_pending && (millis() - last_round_press_time > ROUND_SELECT_DELAY)) {
                saveConfig(config);
                if (config.currentPatternId == 1 || config.currentPatternId == 2) {
                    startPulsing(getPatternValueForPulse());
                }
                round_selection_pending = false;
                Serial.print("DEBUG: Starting delayed pulse for value: ");
                Serial.println(getPatternValueForPulse());
            }
            // Handle pattern change (2–4s) and start (≥4s)
            if (released_long_press) {
                released_long_press = false;
                // Advance to next included pattern id and announce
                // Use configured order to find the next included pattern
                int currentIdx = 0;
                for (int i=0;i<6;i++){ if (config.patternOrder[i]==config.currentPatternId){ currentIdx=i; break; } }
                for (int step=1; step<=6; ++step){
                    int idx = (currentIdx + step) % 6;
                    int id = config.patternOrder[idx];
                    bool ok = (id==1&&config.includeWimHof) || (id==2&&config.includeBox) || (id==3&&config.include478) || (id==4&&config.includeResonant) || (id==5&&config.includeCustom) || (id==6&&config.includeDynamic);
                    if (ok) { config.currentPatternId = id; break; }
                }
                saveConfig(config);
                announceTypeAndValueBlocking();
            }
            if (released_very_long_press) {
                released_very_long_press = false;
                // Finalize any pending selection
                if (round_selection_pending) {
                    saveConfig(config);
                    round_selection_pending = false;
                }
                // Optional start confirmation haptics
                if (config.startConfirmationHaptics) {
                    announceTypeAndValueBlocking();
                }
                vibrate(100); // start cue
                current_session_round = 0;
                session_log_doc.clear();
                rounds_log = session_log_doc["rounds"].to<JsonArray>();
                session_start_time_ms = millis();
                if (config.currentPatternId == 1) {
                    enterState(SessionState::DEEP_BREATHING);
                } else if (config.currentPatternId == 2) {
                    enterState(SessionState::BOX_RUNNING);
                } else if (config.currentPatternId == 3) {
                    enterState(SessionState::FOURSEVENEIGHT_RUNNING);
                } else if (config.currentPatternId == 4) {
                    enterState(SessionState::RESONANT_RUNNING);
                } else if (config.currentPatternId == 5) {
                    // Build custom phases list
                    customPhaseCount = 0;
                    if (config.customInhaleSeconds > 0) customPhases[customPhaseCount++] = {"Inhale", config.customInhaleSeconds};
                    if (config.customHoldInSeconds > 0) customPhases[customPhaseCount++] = {"HoldIn", config.customHoldInSeconds};
                    if (config.customExhaleSeconds > 0) customPhases[customPhaseCount++] = {"Exhale", config.customExhaleSeconds};
                    if (config.customHoldOutSeconds > 0) customPhases[customPhaseCount++] = {"HoldOut", config.customHoldOutSeconds};
                    if (customPhaseCount == 0) {
                        Serial.println("Custom: no active phases. Refusing to start.");
                        vibrate(300);
                        break;
                    }
                    customPhaseIndex = 0;
                    enterState(SessionState::CUSTOM_RUNNING);
                } else if (config.currentPatternId == 6) {
                    enterState(SessionState::DYNAMIC_TEACHING);
                } else {
                    // 4-7-8 / Resonant: minimal placeholder behavior (no runtime engine yet)
                    enterState(SessionState::DEEP_BREATHING);
                }
            }
            if (millis() - last_interaction_time > (unsigned long)config.idleTimeoutMinutes * 60 * 1000) {
                if (shouldPreventDeepSleep()) {
                    Serial.println("Preventing deep sleep - device is in setup mode or connecting to WiFi");
                    last_interaction_time = millis(); // Reset timer
                } else {
                    Serial.println("Entering deep sleep due to inactivity.");
                    Serial.println("Press button to wake up...");
                    
                    // Configure RTC GPIO for wake (GPIO25 is RTC-capable) with pull-up during deep sleep
                    rtc_gpio_deinit((gpio_num_t)BUTTON_PIN); // ensure clean state
                    rtc_gpio_init((gpio_num_t)BUTTON_PIN);
                    rtc_gpio_set_direction((gpio_num_t)BUTTON_PIN, RTC_GPIO_MODE_INPUT_ONLY);
                    rtc_gpio_pullup_en((gpio_num_t)BUTTON_PIN);
                    rtc_gpio_pulldown_dis((gpio_num_t)BUTTON_PIN);
                    // Enable ext0 wake on LOW level (button pressed)
                    esp_sleep_enable_ext0_wakeup(GPIO_NUM_25, 0);
                    
                    // Give a moment for serial to flush
                    delay(100);
                    
                    esp_deep_sleep_start();
                }
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
            if (released_long_press || released_very_long_press) {
                released_long_press = false;
                released_very_long_press = false;
                Serial.println("Session aborted by long press.");
                vibrate(1500); // Distinct long pulse for abort
                // Save partial if above threshold
                unsigned long elapsed = (millis() - session_start_time_ms) / 1000;
                if (elapsed >= (unsigned long)config.abortSaveThresholdSeconds) {
                    saveCurrentSession();
                } else {
                    Serial.println("DEBUG: Discarding session under threshold.");
                    // Clear any partial log
                    session_log_doc.clear();
                    rounds_log = session_log_doc["rounds"].to<JsonArray>();
                }
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
            if (released_long_press || released_very_long_press) {
                released_long_press = false;
                released_very_long_press = false;
                Serial.println("Session aborted by long press.");
                vibrate(1500); // Distinct long pulse for abort
                unsigned long elapsed = (millis() - session_start_time_ms) / 1000;
                if (elapsed >= (unsigned long)config.abortSaveThresholdSeconds) {
                    saveCurrentSession();
                } else {
                    Serial.println("DEBUG: Discarding session under threshold.");
                    session_log_doc.clear();
                    rounds_log = session_log_doc["rounds"].to<JsonArray>();
                }
                current_session_round = 0;
                enterState(SessionState::IDLE);
                return;
            }
            break;

        case SessionState::RECOVERY:
             if (short_press_detected && !pending_recovery_transition) {
                unsigned long duration = (millis() - state_enter_time) / 1000;
                rounds_log[current_session_round-1]["recover"] = duration;
                // Start with 300ms hold at full strength, then fade-out (mirror of fade-in)
                vibrate(300); // Hold at 100% for 300ms
                if (current_session_round < config.currentRound) {
                    recovery_next_state = SessionState::DEEP_BREATHING;
                } else {
                    if (config.silentAfterWimHof) recovery_next_state = SessionState::SILENT; 
                    else recovery_next_state = SessionState::IDLE;
                }
                pending_recovery_transition = true;
                recovery_doing_prehold = true;
                short_press_detected = false;
            }
            // Timeout transition
            if (!pending_recovery_transition && millis() - state_enter_time > (unsigned long)config.recoverySeconds * 1000) {
                unsigned long duration = (millis() - state_enter_time) / 1000;
                rounds_log[current_session_round-1]["recover"] = duration;
                // Start with 300ms hold at full strength, then fade-out (mirror of fade-in)
                vibrate(300); // Hold at 100% for 300ms
                if (current_session_round < config.currentRound) {
                    recovery_next_state = SessionState::DEEP_BREATHING;
                } else {
                    if (config.silentAfterWimHof) recovery_next_state = SessionState::SILENT; 
                    else recovery_next_state = SessionState::IDLE;
                }
                pending_recovery_transition = true;
                recovery_doing_prehold = true;
            }
            // Handle the pre-hold -> fade-out -> gap -> transition sequence
            if (pending_recovery_transition) {
                if (recovery_doing_prehold && !isVibrationBusy()) {
                    // Pre-hold finished, now start the 3-second fade-out
                    vibrateFadeOut(3000); // 3 second ramp down (25% to off)
                    recovery_doing_prehold = false;
                }
                if (!recovery_doing_prehold && !isVibrationBusy()) {
                    // Fade-out finished, start 2 second gap timer
                    if (recovery_gap_start_time == 0) {
                        recovery_gap_start_time = millis();
                    }
                    // After 2 second gap, transition to next state
                    if (millis() - recovery_gap_start_time >= 2000) {
                        pending_recovery_transition = false;
                        recovery_gap_start_time = 0;
                        enterState(recovery_next_state);
                        return;
                    }
                }
            }
            // Global long press abort for active sessions
            if (released_long_press || released_very_long_press) {
                released_long_press = false;
                released_very_long_press = false;
                Serial.println("Session aborted by long press.");
                vibrate(1500); // Distinct long pulse for abort
                unsigned long elapsed = (millis() - session_start_time_ms) / 1000;
                if (elapsed >= (unsigned long)config.abortSaveThresholdSeconds) {
                    saveCurrentSession();
                } else {
                    Serial.println("DEBUG: Discarding session under threshold.");
                    session_log_doc.clear();
                    rounds_log = session_log_doc["rounds"].to<JsonArray>();
                }
                current_session_round = 0;
                enterState(SessionState::IDLE);
                return;
            }
            break;
            
        case SessionState::SILENT:
            if (short_press_detected) {
                // Trigger end-of-silent swell, then mark to exit when finished
                vibrateSwell(2500, 2500);
                pending_silent_exit = true;
                short_press_detected = false;
            }
            // Auto-end silent phase after max duration
            if (millis() - state_enter_time > (unsigned long)config.silentPhaseMaxMinutes * 60 * 1000) {
                Serial.println("Silent phase max duration reached. Ending session.");
                vibrateSwell(2500, 2500);
                pending_silent_exit = true;
            }
            // Silent reminder pulse
            if (config.silentReminderEnabled) {
                static unsigned long last_reminder_time = 0;
                if (millis() - last_reminder_time > (unsigned long)config.silentReminderIntervalMinutes * 60 * 1000) {
                    if (!isVibrationBusy()) {
                        vibrate(750);
                    }
                    last_reminder_time = millis();
                }
            }
            // If swell finished and we were asked to exit, save and go to IDLE
            if (pending_silent_exit && !isVibrationBusy()) {
                saveCurrentSession();
                skip_idle_preview = true; // don't preview selection immediately on return to idle
                enterState(SessionState::IDLE);
                return;
            }
            // Global long press abort for active sessions
            if (released_long_press || released_very_long_press) {
                released_long_press = false;
                released_very_long_press = false;
                Serial.println("Session aborted by long press.");
                vibrate(1500); // Distinct long pulse for abort
                unsigned long elapsed = (millis() - session_start_time_ms) / 1000;
                if (elapsed >= (unsigned long)config.abortSaveThresholdSeconds) {
                    saveCurrentSession();
                } else {
                    Serial.println("DEBUG: Discarding session under threshold.");
                    session_log_doc.clear();
                    rounds_log = session_log_doc["rounds"].to<JsonArray>();
                }
                current_session_round = 0;
                enterState(SessionState::IDLE);
                return;
            }
            break;

        case SessionState::CUSTOM_RUNNING: {
            // Loop through custom phases with boundary buzz and countdown timing
            if (customPhaseCount == 0) { enterState(SessionState::IDLE); break; }
            unsigned long elapsed = (millis() - phase_start_time) / 1000;
            int dur = customPhases[customPhaseIndex].seconds;
            if (elapsed >= (unsigned long)dur) {
                // Advance to next phase
                customPhaseIndex = (customPhaseIndex + 1) % customPhaseCount;
                // Next phase cue - map index to PhaseCue
                PhaseCue cue = (customPhases[customPhaseIndex].name[0]=='I') ? PhaseCue::Inhale :
                                 (customPhases[customPhaseIndex].name[0]=='H' && customPhases[customPhaseIndex].name[4]=='I') ? PhaseCue::HoldIn :
                                 (customPhases[customPhaseIndex].name[0]=='E') ? PhaseCue::Exhale :
                                 PhaseCue::HoldOut;
                vibratePhaseCue(cue);
                phase_start_time = millis();
                // End at boundary if guided duration elapsed
                if (config.guidedBreathingMinutes > 0) {
                    unsigned long guidedElapsed = (millis() - guided_session_start_ms) / 1000UL;
                    if (guidedElapsed >= (unsigned long)config.guidedBreathingMinutes * 60UL) {
                        if (config.silentAfterCustom) enterState(SessionState::SILENT); else enterState(SessionState::IDLE);
                        return;
                    }
                }
            }
            
            // Abort handling
            if (released_long_press || released_very_long_press) {
                released_long_press = released_very_long_press = false;
                Serial.println("Custom: stopped by long press");
                enterState(SessionState::IDLE);
                return;
            }
            break;
        }

        case SessionState::BOX_RUNNING: {
            // Four equal phases of length config.boxSeconds, each boundary short buzz
            unsigned long elapsed = (millis() - box_phase_start_ms) / 1000;
            int sec = config.boxSeconds;
            if (sec < 2) sec = 2; if (sec > 8) sec = 8;
            if (elapsed >= (unsigned long)sec) {
                box_phase_index = (box_phase_index + 1) % 4;
                PhaseCue cue = (box_phase_index == 0) ? PhaseCue::Inhale :
                               (box_phase_index == 1) ? PhaseCue::HoldIn :
                               (box_phase_index == 2) ? PhaseCue::Exhale :
                                                        PhaseCue::HoldOut;
                vibratePhaseCue(cue);
                box_phase_start_ms = millis();
                // End at boundary if guided duration elapsed
                if (config.guidedBreathingMinutes > 0) {
                    unsigned long guidedElapsed = (millis() - guided_session_start_ms) / 1000UL;
                    if (guidedElapsed >= (unsigned long)config.guidedBreathingMinutes * 60UL) {
                        if (config.silentAfterBox) enterState(SessionState::SILENT); else enterState(SessionState::IDLE);
                        return;
                    }
                }
            }
            // Abort handling
            if (released_long_press || released_very_long_press) {
                released_long_press = released_very_long_press = false;
                Serial.println("Box: stopped by long press");
                enterState(SessionState::IDLE);
                return;
            }
            break;
        }

        case SessionState::FOURSEVENEIGHT_RUNNING: {
            // Phases: Inhale 4s -> Hold 7s -> Exhale 8s -> repeat
            unsigned long elapsed = (millis() - guided_phase_start_ms) / 1000;
            int target = (guided_phase_index == 0) ? 4 : (guided_phase_index == 1) ? 7 : 8;
            if (elapsed >= (unsigned long)target) {
                guided_phase_index = (guided_phase_index + 1) % 3;
                PhaseCue cue = (guided_phase_index == 0) ? PhaseCue::Inhale :
                               (guided_phase_index == 1) ? PhaseCue::HoldIn :
                                                           PhaseCue::Exhale;
                vibratePhaseCue(cue);
                guided_phase_start_ms = millis();
                // End at boundary if guided duration elapsed
                if (config.guidedBreathingMinutes > 0) {
                    unsigned long guidedElapsed = (millis() - guided_session_start_ms) / 1000UL;
                    if (guidedElapsed >= (unsigned long)config.guidedBreathingMinutes * 60UL) {
                        if (config.silentAfter478) enterState(SessionState::SILENT); else enterState(SessionState::IDLE);
                        return;
                    }
                }
            }
            if (released_long_press || released_very_long_press) {
                released_long_press = released_very_long_press = false;
                Serial.println("4-7-8: stopped by long press");
                enterState(SessionState::IDLE);
                return;
            }
            break;
        }

        case SessionState::RESONANT_RUNNING: {
            // Phases: Inhale 6s -> Exhale 6s -> repeat
            unsigned long elapsed = (millis() - guided_phase_start_ms) / 1000;
            int target = (guided_phase_index == 0) ? 6 : 6;
            if (elapsed >= (unsigned long)target) {
                guided_phase_index = (guided_phase_index + 1) % 2;
                PhaseCue cue = (guided_phase_index == 0) ? PhaseCue::Inhale : PhaseCue::Exhale;
                vibratePhaseCue(cue);
                guided_phase_start_ms = millis();
                // End at boundary if guided duration elapsed
                if (config.guidedBreathingMinutes > 0) {
                    unsigned long guidedElapsed = (millis() - guided_session_start_ms) / 1000UL;
                    if (guidedElapsed >= (unsigned long)config.guidedBreathingMinutes * 60UL) {
                        if (config.silentAfterResonant) enterState(SessionState::SILENT); else enterState(SessionState::IDLE);
                        return;
                    }
                }
            }
            if (released_long_press || released_very_long_press) {
                released_long_press = released_very_long_press = false;
                Serial.println("Resonant: stopped by long press");
                enterState(SessionState::IDLE);
                return;
            }
            break;
        }

        case SessionState::DYNAMIC_TEACHING: {
            // Timeout to idle if no activity
            if (last_teach_press_ms != 0 && millis() - last_teach_press_ms > 20000UL) {
                Serial.println("Dynamic teach timeout → IDLE");
                enterState(SessionState::IDLE);
                break;
            }
            // Handle button edges as sample boundaries
            if (short_press_detected) {
                unsigned long now = millis();
                if (last_teach_press_ms != 0) {
                    unsigned long deltaMs = now - last_teach_press_ms;
                    if (deltaMs >= 150) {
                        int sec = (int)((deltaMs + 500) / 1000); // round ~1s
                        if (sec < 1) sec = 1; if (sec > 16) sec = 16;
                        if (dynamicExpectInhale) {
                            // press at start inhale: previous phase completed was exhale, but teaching alternates; we treat sequence as inhale then exhale
                            // First valid interval after an inhale press is inhale duration
                            if (teach_inhale_count < 3) teach_samples_inhale[teach_inhale_count % 3] = sec;
                            teach_inhale_count++;
                            dynamicExpectInhale = false;
                        } else {
                            if (teach_exhale_count < 3) teach_samples_exhale[teach_exhale_count % 3] = sec;
                            teach_exhale_count++;
                            dynamicExpectInhale = true;
                        }
                        vibrate(100); // boundary capture feedback
                    }
                }
                last_teach_press_ms = now;
                short_press_detected = false;
            }
            if (released_long_press || released_very_long_press) {
                released_long_press = released_very_long_press = false;
                Serial.println("Dynamic teach stopped → IDLE");
                vibrate(300);
                enterState(SessionState::IDLE);
                break;
            }
            if (teach_inhale_count >= 3 && teach_exhale_count >= 3) {
                int sumA = teach_samples_inhale[0] + teach_samples_inhale[1] + teach_samples_inhale[2];
                int sumB = teach_samples_exhale[0] + teach_samples_exhale[1] + teach_samples_exhale[2];
                avg_inhale_sec = sumA / 3; if (avg_inhale_sec < 1) avg_inhale_sec = 1; if (avg_inhale_sec > 16) avg_inhale_sec = 16;
                avg_exhale_sec = sumB / 3; if (avg_exhale_sec < 1) avg_exhale_sec = 1; if (avg_exhale_sec > 16) avg_exhale_sec = 16;
                // Confirm double buzz
                vibrate(100); delay(150); vibrate(100);
                enterState(SessionState::DYNAMIC_GUIDED);
            }
            break;
        }

        case SessionState::DYNAMIC_GUIDED: {
            unsigned long now = millis();
            int phaseSec = dynamic_inhale_phase ? avg_inhale_sec : avg_exhale_sec;
            if (phaseSec < 1) phaseSec = 1; if (phaseSec > 16) phaseSec = 16;
            if ((now - dynamic_phase_start_ms) / 1000 >= (unsigned long)phaseSec) {
                dynamic_inhale_phase = !dynamic_inhale_phase;
                dynamic_phase_start_ms = now;
                vibrate(100);
                // End at boundary if guided duration elapsed
                if (config.guidedBreathingMinutes > 0) {
                    unsigned long guidedElapsed = (now - guided_session_start_ms) / 1000UL;
                    if (guidedElapsed >= (unsigned long)config.guidedBreathingMinutes * 60UL) {
                        if (config.silentAfterDynamic) enterState(SessionState::SILENT); else enterState(SessionState::IDLE);
                        return;
                    }
                }
            }
            // Re-teach updates on short presses; reuse teaching capture alternating
            if (short_press_detected) {
                unsigned long deltaMs = now - last_teach_press_ms;
                if (last_teach_press_ms != 0 && deltaMs >= 150) {
                    int sec = (int)((deltaMs + 500) / 1000);
                    if (sec < 1) sec = 1; if (sec > 16) sec = 16;
                    if (dynamic_inhale_phase) {
                        teach_samples_inhale[teach_inhale_count % 3] = sec; teach_inhale_count++;
                    } else {
                        teach_samples_exhale[teach_exhale_count % 3] = sec; teach_exhale_count++;
                    }
                    int sumA = teach_samples_inhale[0] + teach_samples_inhale[1] + teach_samples_inhale[2];
                    int sumB = teach_samples_exhale[0] + teach_samples_exhale[1] + teach_samples_exhale[2];
                    avg_inhale_sec = sumA / 3; if (avg_inhale_sec < 1) avg_inhale_sec = 1; if (avg_inhale_sec > 16) avg_inhale_sec = 16;
                    avg_exhale_sec = sumB / 3; if (avg_exhale_sec < 1) avg_exhale_sec = 1; if (avg_exhale_sec > 16) avg_exhale_sec = 16;
                    vibrate(100); // updated cadence feedback
                }
                last_teach_press_ms = now;
                short_press_detected = false;
            }
            if (released_long_press || released_very_long_press) {
                released_long_press = released_very_long_press = false;
                Serial.println("Dynamic guided stopped → IDLE");
                vibrate(300);
                enterState(SessionState::IDLE);
                return;
            }
            break;
        }
    }
}

SessionState getCurrentState() {
    return currentState;
}

void reloadSessionConfig() {
    // Safely reload persisted config so runtime durations reflect latest settings
    config = loadConfig();
}

int getCurrentSessionRound() {
    return current_session_round;
}

int getTotalRounds() {
    return config.currentRound;
}

bool shouldPreventDeepSleep() {
    // Prevent deep sleep if we're in hotspot mode or if WiFi is connecting
    extern bool isHotspotMode();
    extern bool isWifiConnected();
    
    return isHotspotMode() || !isWifiConnected();
}

void finishBooting() {
    if (currentState == SessionState::BOOTING) {
        enterState(SessionState::IDLE);
    }
}

void resetIdleTimer() {
    last_interaction_time = millis();
} 
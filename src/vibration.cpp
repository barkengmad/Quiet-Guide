#include "vibration.h"
#include "config.h"
#include "storage.h"
#include "session.h"
#include <Arduino.h>

unsigned long vibration_stop_time = 0;
static bool fade_active = false;
static unsigned long fade_start_time = 0;
static int fade_duration_ms = 0;
static bool fade_in = false;
static int fade_post_hold_ms = 0;
// Swell (25% -> 100% -> 25%) state
static bool swell_active = false;
static unsigned long swell_start_time = 0;
static int swell_up_ms = 0;
static int swell_down_ms = 0;

void setupVibration() {
    ledcSetup(VIBRATION_PWM_CHANNEL, VIBRATION_PWM_FREQ, VIBRATION_PWM_RESOLUTION);
    ledcAttachPin(VIBRATION_PIN, VIBRATION_PWM_CHANNEL);
}

void vibrate(int duration_ms) {
    ledcWrite(VIBRATION_PWM_CHANNEL, 200); // 78% duty cycle for more strength
    vibration_stop_time = millis() + duration_ms;
}

bool isVibrationBusy() {
    return vibration_stop_time > 0 || fade_active || swell_active;
}

void loopVibration() {
    unsigned long now = millis();
    if (fade_active) {
        unsigned long elapsed = now - fade_start_time;
        if (elapsed >= (unsigned long)fade_duration_ms) {
            // End of fade
            if (fade_in) {
                // After fade-in, optionally hold full strength briefly
                if (fade_post_hold_ms > 0) {
                    ledcWrite(VIBRATION_PWM_CHANNEL, 200);
                    vibration_stop_time = now + fade_post_hold_ms;
                } else {
                    ledcWrite(VIBRATION_PWM_CHANNEL, 200);
                    vibration_stop_time = now; // allow immediate stop below
                }
            } else {
                // For fade-out, finish ramp at perceivable threshold (25%), then stop
                ledcWrite(VIBRATION_PWM_CHANNEL, 50);
                vibration_stop_time = now; // stop shortly after
            }
            fade_active = false;
        } else {
            // Linear ramp within 25%..100% (50..200)
            const int minLevel = 50;
            const int maxLevel = 200;
            const int range = maxLevel - minLevel; // 150
            int level = (int)((elapsed * (long)range) / fade_duration_ms);
            if (!fade_in) level = range - level;
            level += minLevel;
            if (level < 0) level = 0; if (level > 200) level = 200;
            ledcWrite(VIBRATION_PWM_CHANNEL, level);
        }
    }
    // Swell handling (25% -> 100% -> 25%)
    if (swell_active) {
        unsigned long elapsed = now - swell_start_time;
        unsigned long total = (unsigned long)swell_up_ms + (unsigned long)swell_down_ms;
        if (elapsed >= total) {
            // End at threshold then stop
            ledcWrite(VIBRATION_PWM_CHANNEL, 50);
            swell_active = false;
            vibration_stop_time = now; // allow stop logic below to clear
        } else if (elapsed < (unsigned long)swell_up_ms) {
            // Up ramp
            const int minLevel = 50;
            const int maxLevel = 200;
            const int range = maxLevel - minLevel;
            int level = minLevel + (int)((elapsed * (long)range) / swell_up_ms);
            if (level < 0) level = 0; if (level > 200) level = 200;
            ledcWrite(VIBRATION_PWM_CHANNEL, level);
        } else {
            // Down ramp
            unsigned long downElapsed = elapsed - (unsigned long)swell_up_ms;
            const int minLevel = 50;
            const int maxLevel = 200;
            const int range = maxLevel - minLevel;
            int level = maxLevel - (int)((downElapsed * (long)range) / swell_down_ms);
            if (level < 0) level = 0; if (level > 200) level = 200;
            ledcWrite(VIBRATION_PWM_CHANNEL, level);
        }
    }
    if (vibration_stop_time > 0 && now >= vibration_stop_time) {
        ledcWrite(VIBRATION_PWM_CHANNEL, 0); // Stop vibration
        vibration_stop_time = 0;
    }
}

// Pulse function needs to be handled by the state machine now,
// as it can't block with delays.
void pulse(int count, int duration_ms, int delay_ms) {
    // This function will be re-implemented inside the session manager
    // to work with the non-blocking architecture. For now, it's a single pulse.
    vibrate(duration_ms);
}

void vibrateTypeLong(int count) {
    for (int i = 0; i < count; ++i) {
        ledcWrite(VIBRATION_PWM_CHANNEL, 200);
        delay(800);
        ledcWrite(VIBRATION_PWM_CHANNEL, 0);
        if (i < count - 1) delay(1600);
    }
}

void vibrateValueShort(int count) {
    if (count <= 0) return;
    for (int i = 0; i < count; ++i) {
        ledcWrite(VIBRATION_PWM_CHANNEL, 200);
        delay(300);
        ledcWrite(VIBRATION_PWM_CHANNEL, 0);
        if (i < count - 1) delay(300);
    }
}

void vibratePhaseCue(PhaseCue phase) {
    // For now, all phases use a short 100ms cue. This can be customized later.
    (void)phase; // suppress unused warning
    vibrate(100);
}

void vibrateFadeOut(int duration_ms) {
    fade_active = true;
    fade_in = false;
    fade_duration_ms = (duration_ms < 50) ? 50 : duration_ms;
    fade_post_hold_ms = 0;
    fade_start_time = millis();
}

void vibrateFadeIn(int duration_ms, int post_hold_ms) {
    fade_active = true;
    fade_in = true;
    fade_duration_ms = (duration_ms < 50) ? 50 : duration_ms;
    fade_post_hold_ms = post_hold_ms;
    fade_start_time = millis();
}

void vibrateSwell(int up_ms, int down_ms) {
    if (up_ms < 50) up_ms = 50;
    if (down_ms < 50) down_ms = 50;
    swell_active = true;
    swell_up_ms = up_ms;
    swell_down_ms = down_ms;
    swell_start_time = millis();
    // Initialize at 25%
    ledcWrite(VIBRATION_PWM_CHANNEL, 50);
}

void vibrateIPAddress(IPAddress ip) {
    static bool isRunning = false;
    
    if (isRunning) {
        Serial.println("WARNING: vibrateIPAddress called while already running - ignoring!");
        return;
    }
    
    isRunning = true;
    Serial.println("*** STARTING vibrateIPAddress function ***");
    
    // Get the last octet of the IP address (e.g., 165 from 10.10.10.165)
    int lastOctet = ip[3];
    
    Serial.print("Vibrating IP address last octet: ");
    Serial.println(lastOctet);
    
    // Long buzz to indicate start
    ledcWrite(VIBRATION_PWM_CHANNEL, 200);
    delay(1000);
    ledcWrite(VIBRATION_PWM_CHANNEL, 0);
    delay(500);
    
    // Convert number to individual digits and vibrate each
    String numberStr = String(lastOctet);
    
    for (int i = 0; i < numberStr.length(); i++) {
        int digit = numberStr.charAt(i) - '0'; // Convert char to int
        
        Serial.print("Vibrating digit: ");
        Serial.println(digit);
        
        // Vibrate the number of times equal to the digit
        // Special case: 0 = 10 buzzes to distinguish from no buzz
        int buzzes = (digit == 0) ? 10 : digit;
        
        for (int j = 0; j < buzzes; j++) {
            ledcWrite(VIBRATION_PWM_CHANNEL, 200);
            delay(300); // Short buzz (1.5x longer: 200 * 1.5 = 300)
            ledcWrite(VIBRATION_PWM_CHANNEL, 0);
            delay(300); // Short pause between buzzes (also 1.5x longer)
        }
        
        // Longer pause between digits (double: 800 * 2 = 1600)
        delay(1600);
    }
    
    // Long buzz to indicate end of IP
    ledcWrite(VIBRATION_PWM_CHANNEL, 200);
    delay(1000);
    ledcWrite(VIBRATION_PWM_CHANNEL, 0);
    
    Serial.println("IP address vibration complete");
    
    // 3 second pause to separate IP vibration from subsequent session pulsing
    delay(3000);
    
    isRunning = false;
    Serial.println("*** FINISHED vibrateIPAddress function ***");
} 
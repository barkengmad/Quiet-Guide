#include "vibration.h"
#include "config.h"
#include <Arduino.h>

unsigned long vibration_stop_time = 0;

void setupVibration() {
    ledcSetup(VIBRATION_PWM_CHANNEL, VIBRATION_PWM_FREQ, VIBRATION_PWM_RESOLUTION);
    ledcAttachPin(VIBRATION_PIN, VIBRATION_PWM_CHANNEL);
}

void vibrate(int duration_ms) {
    ledcWrite(VIBRATION_PWM_CHANNEL, 200); // 78% duty cycle for more strength
    vibration_stop_time = millis() + duration_ms;
}

void loopVibration() {
    if (vibration_stop_time > 0 && millis() >= vibration_stop_time) {
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
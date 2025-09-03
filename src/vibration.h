#ifndef VIBRATION_H
#define VIBRATION_H

#include <WiFi.h>

void setupVibration();
void loopVibration();
void vibrate(int duration_ms);
void pulse(int count, int duration_ms = 100, int delay_ms = 200);
void vibrateIPAddress(IPAddress ip);
// New helpers: type and value announcements
void vibrateTypeLong(int count);
void vibrateValueShort(int count);
// Non-blocking fades for phase boundaries
void vibrateFadeOut(int duration_ms);
void vibrateFadeIn(int duration_ms, int post_hold_ms = 0);

// Phase cue abstraction for future haptic variations
enum class PhaseCue { Inhale, HoldIn, Exhale, HoldOut };
void vibratePhaseCue(PhaseCue phase);

#endif // VIBRATION_H 
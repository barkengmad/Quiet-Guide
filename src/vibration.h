#ifndef VIBRATION_H
#define VIBRATION_H

#include <WiFi.h>

void setupVibration();
void loopVibration();
void vibrate(int duration_ms);
void pulse(int count, int duration_ms = 100, int delay_ms = 200);
void vibrateIPAddress(IPAddress ip);

#endif // VIBRATION_H 
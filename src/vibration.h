#ifndef VIBRATION_H
#define VIBRATION_H

void setupVibration();
void loopVibration();
void vibrate(int duration_ms);
void pulse(int count, int duration_ms = 100, int delay_ms = 200);

#endif // VIBRATION_H 
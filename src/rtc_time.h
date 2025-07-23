#ifndef RTC_TIME_H
#define RTC_TIME_H

#include <Arduino.h>
#include <time.h>

void setupRtcTime();
void updateRtcTime();
String getFormattedTime();
time_t getEpochTime();

#endif // RTC_TIME_H 
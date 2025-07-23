#include "rtc_time.h"
#include <time.h>
#include "network.h"

RTC_DATA_ATTR time_t last_sync_time = 0;
RTC_DATA_ATTR unsigned long last_sync_millis = 0;

void setupRtcTime() {
    if (isWifiConnected()) {
        updateRtcTime();
    }
}

void updateRtcTime() {
    time(&last_sync_time);
    last_sync_millis = millis();
}

time_t getEpochTime() {
    if (isWifiConnected()) {
        time_t now;
        time(&now);
        return now;
    } else {
        // Estimate time since last sync
        return last_sync_time + (millis() - last_sync_millis) / 1000;
    }
}

String getFormattedTime() {
    time_t now = getEpochTime();
    struct tm * timeinfo;
    timeinfo = localtime(&now);
    
    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    return String(buffer);
} 
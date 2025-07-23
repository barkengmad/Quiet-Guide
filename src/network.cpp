#include "network.h"
#include "config.h"
#include <WiFi.h>
#include "time.h"

enum class NetworkStatus {
    DISCONNECTED,
    CONNECTING_WIFI,
    SYNCING_NTP,
    CONNECTED,
    BOOTING
};

NetworkStatus networkStatus = NetworkStatus::BOOTING;
unsigned long network_timeout_start = 0;

void setupNetwork() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW); // Start with LED off
    networkStatus = NetworkStatus::CONNECTING_WIFI;
    network_timeout_start = millis();
    
    if (ssid && strlen(ssid) > 0) {
        WiFi.begin(ssid, password);
    } else {
        networkStatus = NetworkStatus::DISCONNECTED;
    }
}

bool isWifiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

void loopNetwork() {
    switch (networkStatus) {
        case NetworkStatus::CONNECTING_WIFI:
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("WiFi Connected.");
                networkStatus = NetworkStatus::SYNCING_NTP;
                network_timeout_start = millis();
                configTime(0, 0, "pool.ntp.org");
            } else if (millis() - network_timeout_start > 5000) {
                Serial.println("WiFi connection failed.");
                networkStatus = NetworkStatus::DISCONNECTED;
            }
            break;
            
        case NetworkStatus::SYNCING_NTP:
            {
                struct tm timeinfo;
                if (getLocalTime(&timeinfo)) {
                    Serial.println("NTP Sync successful.");
                    networkStatus = NetworkStatus::CONNECTED;
                } else if (millis() - network_timeout_start > 5000) {
                    Serial.println("NTP Sync failed.");
                    networkStatus = NetworkStatus::DISCONNECTED;
                }
            }
            break;
        default:
            // Do nothing in other states
            break;
    }
}

void handleLedIndicator() {
    static unsigned long lastToggleTime = 0;
    static bool ledState = false;

    unsigned long interval = 0;
    bool solid_on = false;

    switch (networkStatus) {
        case NetworkStatus::CONNECTING_WIFI:
            interval = 250; // Slow flash
            break;
        case NetworkStatus::SYNCING_NTP:
            interval = 62; // Fast flash
            break;
        case NetworkStatus::CONNECTED:
            solid_on = true;
            break;
        default:
            // LED is off
            break;
    }

    if (solid_on) {
        digitalWrite(LED_PIN, HIGH);
    } else if (interval > 0) {
        if (millis() - lastToggleTime > interval) {
            ledState = !ledState;
            digitalWrite(LED_PIN, ledState);
            lastToggleTime = millis();
        }
    } else {
        digitalWrite(LED_PIN, LOW);
    }
} 
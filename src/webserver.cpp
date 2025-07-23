#include <Arduino.h>
#include "webserver.h"
#include "storage.h"
#include "network.h"
#include <WiFi.h>

// Simplified web server - just print IP for now
// TODO: Implement proper web server once core functionality is working

void setupWebServer() {
    if (isWifiConnected()) {
        Serial.print("WiFi connected! IP address: ");
        Serial.println(WiFi.localIP());
        Serial.println("Web server temporarily disabled - core functionality ready");
    }
}

void stopWebServer() {
    // Nothing to stop for now
} 